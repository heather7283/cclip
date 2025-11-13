/*
 * This file is part of cclip, clipboard manager for wayland
 * Copyright (C) 2024  heather7283
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <fcntl.h>

#include <wayland-client.h>

#include "wayland.h"
#include "proto_impl.h"
#include "sql.h"
#include "log.h"
#include "config.h"
#include "macros.h"
#include "xmalloc.h"
#include "eventloop.h"

struct pipe {
    union {
        int fds[2];
        struct {
            int read;
            int write;
        };
    };
};

struct mime_type {
    char name[256]; /* https://stackoverflow.com/a/643772 */
};

struct clipboard_offer_data {
    VEC(uint8_t) data;
    struct mime_type type;
    struct pollen_event_source* fd_source;
};

static struct {
    int fd;
    struct wl_display* display;
    struct wl_seat* seat;
    struct wl_registry* registry;

    struct zwlr_data_control_manager_v1* wlr_data_ctrl_manager;
    struct ext_data_control_manager_v1* ext_data_ctrl_manager;

    const struct protocol_implementation* impl;
    struct data_ctrl_device* device;
    struct data_ctrl_offer* offer;
    VEC(struct mime_type) offer_types;
} wayland = {
    .fd = -1
};

static int on_pipe_ready(struct pollen_event_source* source, int fd, uint32_t ev, void* data) {
    struct clipboard_offer_data* od = data;
    bool free_vec = true;

    /* TODO: maybe just read into static buffer and memcpy into vec instead? */
    int available;
    if (ioctl(fd, FIONREAD, &available) == -1) {
        log_print(ERR, "failed to retrieve number of bytes in a pipe: %s", strerror(errno));
        goto free;
    }

    VEC_RESERVE(&od->data, od->data.size + available);

    while (available > 0) {
        ssize_t ret = read(fd, &od->data.data[od->data.size], available);
        if (ret == -1 && errno != EINTR) {
            log_print(ERR, "failed to read from pipe: %s", strerror(errno));
            goto free;
        }

        available -= ret;
        od->data.size += ret;
    }

    if (ev & EPOLLHUP) {
        /* writing client closed its end of the pipe - finalize transfer */
        if (od->data.size == 0) {
            log_print(WARN, "nothing was received!");
        } else if (od->data.size < config.min_data_size) {
            log_print(DEBUG, "received %d bytes which is less than %d, not saving",
                      od->data.size, config.min_data_size);
        } else {
            queue_for_insertion(od->data.data, od->data.size, xstrdup(od->type.name));
            free_vec = false;
        }
        goto free;
    }

    return 0;

free:
    pollen_event_source_remove(source);
    if (free_vec) {
        VEC_FREE(&od->data);
    }
    free(od);

    return 0;
}

static bool check_secret(void) {
    bool has_password_manager_hint = false;
    VEC_FOREACH(&wayland.offer_types, i) {
        const struct mime_type* type = &wayland.offer_types.data[i];
        if (STREQ(type->name, "x-kde-passwordManagerHint")) {
            log_print(TRACE, "got x-kde-passwordManagerHint");
            has_password_manager_hint = true;
            break;
        }
    }
    if (has_password_manager_hint) {
        struct pipe p = { .fds = { -1, -1 } };

        if (pipe(p.fds) == -1) {
            log_print(ERR, "failed to create pipe: %s", strerror(errno));
            /* can't really know what the value is, but better safe than sorry */
            return true;
        }

        wayland.impl->data_ctrl_offer_receive(wayland.offer, "x-kde-passwordManagerHint", p.write);
        wl_display_flush(wayland.display);
        close(p.write);

        /* No need to make this read async - expected data is tiny */
        char buf[sizeof("secret")] = { '\0' };
        ssize_t ret;
        do {
            ret = read(p.read, buf, sizeof(buf) - 1);
        } while (ret < 0 && errno == EINTR);
        close(p.read);

        if (STREQ(buf, "secret")) {
            return true;
        }
    }

    return false;
}

static void receive_offer(void) {
    struct clipboard_offer_data* od = NULL;
    struct pipe p = { .fds = { -1, -1 } };

    const struct mime_type* selected_type = NULL;
    VEC_FOREACH(&config.accepted_mime_types, i) {
        const char* pattern = config.accepted_mime_types.data[i];

        VEC_FOREACH(&wayland.offer_types, j) {
            const struct mime_type* type = &wayland.offer_types.data[j];

            if (fnmatch(pattern, type->name, 0) == 0) {
                log_print(DEBUG, "picked mime type: %s", type->name);
                selected_type = type;
                goto loop_out;
            }
        }
    }
loop_out:
    if (selected_type == NULL) {
        log_print(DEBUG, "didn't match any mime type, not receiving this offer");
        return;
    }

    if (config.ignore_secrets && check_secret()) {
        log_print(DEBUG, "offer is marked as secret, ignoring");
        return;
    }

    /* create a pipe for data transfer between us and source client */
    if (pipe(p.fds) == -1) {
        log_print(ERR, "failed to create pipe: %s", strerror(errno));
        return;
    }

    /* make it big - don't really care if fails, transfer will just be slower */
    fcntl(p.read, F_SETPIPE_SZ, 1 * 1024 * 1024 /* 1 MiB */);

    log_print(TRACE, "receiving offer %p...", (void*)wayland.offer);
    wayland.impl->data_ctrl_offer_receive(wayland.offer, selected_type->name, p.write);

    /* make sure the sender received our request and is ready for transfer */
    wl_display_flush(wayland.display);

    /* close writing end on our side, we don't need it */
    close(p.write);

    od = xcalloc(1, sizeof(*od));
    od->type = *selected_type;

    od->fd_source = pollen_loop_add_fd(eventloop, p.read, EPOLLIN, true, on_pipe_ready, od);
    if (od->fd_source == NULL) {
        log_print(ERR, "failed to add pipe fd to event loop: %s", strerror(errno));
        goto err;
    }

    return;

err:
    if (od != NULL) {
        free(od);
    }
    if (p.read > 0) {
        close(p.read);
    }
    if (p.write > 0) {
        close(p.write);
    }
}

static void common_selection_handler(struct data_ctrl_offer* offer, bool primary) {
    if (!primary || config.primary_selection) {
        receive_offer();
    } else {
        log_print(DEBUG, "ignoring primary selection event for offer %p", (void*)offer);
    }

    log_print(TRACE, "destroying offer %p", (void*)offer);
    wayland.impl->data_ctrl_offer_destroy(offer);
    wayland.offer = NULL;
}

static void selection_handler(void* data, struct data_ctrl_device* device,
                              struct data_ctrl_offer* offer) {
    log_print(DEBUG, "got selection event for offer %p", (void*)offer);
    if (offer == NULL) {
        return;
    }

    common_selection_handler(offer, false);
}

static void primary_selection_handler(void* data, struct data_ctrl_device* device,
                                      struct data_ctrl_offer* offer) {
    log_print(DEBUG, "got primary selection event for offer %p", (void*)offer);
    if (offer == NULL) {
        return;
    }

    common_selection_handler(offer, true);
}

static void mime_type_offer_handler(void* data, struct data_ctrl_offer* offer,
                                    const char* mime_type) {
    log_print(TRACE, "got mime type offer %s for offer %p", mime_type, (void*)offer);

    size_t mime_type_len = strlen(mime_type);
    if (mime_type_len > 255) {
        log_print(ERR, "mime type is too long (%zu): %s", mime_type_len, mime_type);
        return;
    }

    struct mime_type* t = VEC_EMPLACE_BACK(&wayland.offer_types);
    memcpy(&t->name, mime_type, mime_type_len);
    t->name[mime_type_len] = '\0';
}

static const struct data_ctrl_offer_listener data_ctrl_offer_listener = {
    .offer = mime_type_offer_handler,
};

static void data_offer_handler(void* data, struct data_ctrl_device* device,
                               struct data_ctrl_offer* offer) {
    log_print(DEBUG, "got new data_control_offer %p", (void*)offer);

    wayland.offer = offer;
    VEC_CLEAR(&wayland.offer_types);
    wayland.impl->data_ctrl_offer_add_listener(offer, &data_ctrl_offer_listener, NULL);
}

static const struct data_ctrl_device_listener data_ctrl_device_listener = {
    .data_offer = data_offer_handler,
    .selection = selection_handler,
    .primary_selection = primary_selection_handler,
};

static void registry_global(void* data, struct wl_registry* registry, uint32_t name,
                            const char* interface, uint32_t version) {
    if (wayland.seat == NULL && STREQ(interface, wl_seat_interface.name)) {
        wayland.seat = wl_registry_bind(registry, name, &wl_seat_interface, 2);
    } else if (STREQ(interface, zwlr_data_control_manager_v1_interface.name)) {
        wayland.wlr_data_ctrl_manager =
            wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, 2);
    } else if (STREQ(interface, ext_data_control_manager_v1_interface.name)) {
        wayland.ext_data_ctrl_manager =
            wl_registry_bind(registry, name, &ext_data_control_manager_v1_interface, 1);
    }
}

static void registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {
    /* no-op */
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static int on_wayland_events(struct pollen_event_source* src, int fd, uint32_t ev, void* data) {
    if (wl_display_dispatch(wayland.display) < 0) {
        log_print(ERR, "failed to process wayland events: %s", errno);
        return -1;
    }

    return 0;
}

int wayland_init(void) {
    wayland.display = wl_display_connect(NULL);
    if (wayland.display == NULL) {
        log_print(ERR, "failed to connect to display");
        return -1;
    }

    wayland.fd = wl_display_get_fd(wayland.display);
    pollen_loop_add_fd(eventloop, wayland.fd, EPOLLIN, false, on_wayland_events, NULL);

    wayland.registry = wl_display_get_registry(wayland.display);
    if (wayland.registry == NULL) {
        log_print(ERR, "failed to get registry");
        return -1;
    }

    wl_registry_add_listener(wayland.registry, &registry_listener, NULL);

    /* Roundtrip to enumerate all globals so we can proceed with initialisation */
    wl_display_roundtrip(wayland.display);

    if (wayland.seat == NULL) {
        log_print(ERR, "failed to bind to seat interface");
        return -1;
    }

    struct data_ctrl_manager* manager;
    if (wayland.wlr_data_ctrl_manager == NULL && wayland.ext_data_ctrl_manager == NULL) {
        log_print(ERR, "failed to bind either wlr_data_control_manager "
                       "or ext_data_control_manager interface, no compositor support?");
        return -1;
    } else if (wayland.ext_data_ctrl_manager != NULL) {
        log_print(INFO, "using ext_data_control_manager");

        wayland.impl = get_protocol_implementation_ext();
        manager = (struct data_ctrl_manager*)wayland.ext_data_ctrl_manager;
    } else /* if (wayland.wlr_data_ctrl_manager != NULL) */ {
        log_print(INFO, "ext_data_control_manager is unavailable, using wlr_data_control_manager");

        wayland.impl = get_protocol_implementation_ext();
        manager = (struct data_ctrl_manager*)wayland.wlr_data_ctrl_manager;
    }

    wayland.device = wayland.impl->get_data_ctrl_device(manager, wayland.seat);
    if (wayland.device == NULL) {
        log_print(ERR, "failed to get a data_control_device");
        return -1;
    }
    wayland.impl->data_ctrl_device_add_listener(wayland.device, &data_ctrl_device_listener, NULL);

    /*
     * Don't roundtrip here so we don't start processing clipboard events
     * before entering actual main event loop, just flush instead.
     * From wayland docs: "Clients should always call this function before
     * blocking on input from the display fd."
     */
    wl_display_flush(wayland.display);

    return wayland.fd;
}

void wayland_cleanup(void) {
    if (wayland.offer) {
        wayland.impl->data_ctrl_offer_destroy(wayland.offer);
    }
    if (wayland.device) {
        wayland.impl->data_ctrl_device_destroy(wayland.device);
    }

    if (wayland.wlr_data_ctrl_manager) {
        zwlr_data_control_manager_v1_destroy(wayland.wlr_data_ctrl_manager);
    }
    if (wayland.ext_data_ctrl_manager) {
        ext_data_control_manager_v1_destroy(wayland.ext_data_ctrl_manager);
    }

    if (wayland.seat) {
        wl_seat_destroy(wayland.seat);
    }
    if (wayland.registry) {
        wl_registry_destroy(wayland.registry);
    }
    if (wayland.display) {
        wl_display_disconnect(wayland.display);
    }
}

