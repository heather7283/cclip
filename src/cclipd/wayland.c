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
#include "sql.h"
#include "config.h"
#include "log.h"
#include "xmalloc.h"
#include "eventloop.h"

#include "wlr-data-control-unstable-v1.h"

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

struct clipboard_offer {
    struct zwlr_data_control_offer_v1* offer;
    VEC(struct mime_type) mime_types;
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
    struct zwlr_data_control_manager_v1* data_control_manager;
    struct zwlr_data_control_device_v1* data_control_device;

    struct clipboard_offer offer;
} wayland = {0};

static void mime_type_copy(struct mime_type* dst, const struct mime_type* src) {
    memcpy(dst->name, src->name, sizeof(dst->name));
}

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
        queue_for_insertion(od->data.data, od->data.size, xstrdup(od->type.name));
        free_vec = false;
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

static void receive_offer(struct clipboard_offer* co) {
    struct clipboard_offer_data* od = NULL;
    struct pipe p = { .fds = { -1, -1 } };

    struct mime_type* selected_type = NULL;
    VEC_FOREACH(&config.accepted_mime_types, i) {
        const char* pattern = config.accepted_mime_types.data[i];

        VEC_FOREACH(&co->mime_types, j) {
            struct mime_type* t = &co->mime_types.data[j];

            if (fnmatch(pattern, t->name, 0) == 0) {
                log_print(DEBUG, "picked mime type: %s", t->name);
                selected_type = t;
                goto loop_out;
            }
        }
    }
loop_out:
    if (selected_type == NULL) {
        log_print(DEBUG, "didn't match any mime type, not receiving this offer");
        return;
    }

    /* create a pipe for data transfer between us and source client */
    if (pipe(p.fds) == -1) {
        log_print(ERR, "failed to create pipe: %s", strerror(errno));
        return;
    }

    /* make it big - don't really care if fails, transfer will just be slower */
    fcntl(p.read, F_SETPIPE_SZ, 1 * 1024 * 1024 /* 1 MiB */);

    log_print(TRACE, "receiving offer %p...", (void*)co->offer);
    zwlr_data_control_offer_v1_receive(co->offer, selected_type->name, p.write);

    /* make sure the sender received our request and is ready for transfer */
    wl_display_roundtrip(wayland.display);

    /* close writing end on our side, we don't need it */
    close(p.write);

    od = xcalloc(1, sizeof(*od));
    mime_type_copy(&od->type, selected_type);

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

static void common_selection_handler(struct clipboard_offer* co, bool primary) {
    if (!primary || config.primary_selection) {
        receive_offer(co);
    } else {
        log_print(DEBUG, "ignoring primary selection event for offer %p", (void*)co);
    }

    log_print(TRACE, "destroying offer %p", (void*)co);
    zwlr_data_control_offer_v1_destroy(co->offer);
    co->offer = NULL;
}

static void selection_handler(void* data, struct zwlr_data_control_device_v1* device,
                              struct zwlr_data_control_offer_v1* offer) {
    log_print(DEBUG, "got selection event for offer %p", (void*)offer);
    if (offer == NULL) {
        return;
    }

    struct clipboard_offer* co = &wayland.offer;
    common_selection_handler(co, false);
}

static void primary_selection_handler(void* data, struct zwlr_data_control_device_v1* device,
                                      struct zwlr_data_control_offer_v1* offer) {
    log_print(DEBUG, "got primary selection event for offer %p", (void*)offer);
    if (offer == NULL) {
        return;
    }

    struct clipboard_offer* co = &wayland.offer;
    common_selection_handler(co, true);
}

static void mime_type_offer_handler(void* data, struct zwlr_data_control_offer_v1* offer,
                                    const char* mime_type) {
    log_print(TRACE, "got mime type offer %s for offer %p", mime_type, (void*)offer);

    struct clipboard_offer* co = data;
    size_t mime_type_len = strlen(mime_type);
    if (mime_type_len > 255) {
        log_print(ERR, "mime type is too long (%zu): %s", mime_type_len, mime_type);
        return;
    }

    struct mime_type* t = VEC_EMPLACE_BACK(&co->mime_types);
    memcpy(&t->name, mime_type, mime_type_len);
    t->name[mime_type_len] = '\0';
}

static const struct zwlr_data_control_offer_v1_listener data_control_offer_listener = {
    .offer = mime_type_offer_handler,
};

static void data_offer_handler(void* data, struct zwlr_data_control_device_v1* device,
                               struct zwlr_data_control_offer_v1* offer) {
    log_print(DEBUG, "got new wlr_data_control_offer %p", (void*)offer);

    struct clipboard_offer* co = &wayland.offer;
    co->offer = offer;

    if (offer != NULL) {
        VEC_CLEAR(&co->mime_types);
        zwlr_data_control_offer_v1_add_listener(co->offer, &data_control_offer_listener, co);
    }
}

static const struct zwlr_data_control_device_v1_listener data_control_device_listener = {
    .data_offer = data_offer_handler,
    .selection = selection_handler,
    .primary_selection = primary_selection_handler,
};

static void registry_global(void* data, struct wl_registry* registry, uint32_t name,
                            const char* interface, uint32_t version) {
    if (wayland.seat == NULL && strcmp(interface, "wl_seat") == 0) {
        wayland.seat = wl_registry_bind(registry, name, &wl_seat_interface, 2);
    } else if (strcmp(interface, "zwlr_data_control_manager_v1") == 0) {
        wayland.data_control_manager =
            wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, 2);
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

    if (wayland.data_control_manager == NULL) {
        log_print(ERR, "failed to bind to data_control_manager interface");
        return -1;
    }

    wayland.data_control_device =
        zwlr_data_control_manager_v1_get_data_device(wayland.data_control_manager, wayland.seat);
    if (wayland.data_control_device == NULL) {
        log_print(ERR, "data device is null");
        return -1;
    }

    zwlr_data_control_device_v1_add_listener(wayland.data_control_device,
                                             &data_control_device_listener,
                                             NULL);

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
    if (wayland.data_control_device) {
        zwlr_data_control_device_v1_destroy(wayland.data_control_device);
    }
    if (wayland.data_control_manager) {
        zwlr_data_control_manager_v1_destroy(wayland.data_control_manager);
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

