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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <wayland-client.h>

#include "wayland.h"
#include "sql.h"
#include "config.h"
#include "log.h"
#include "xmalloc.h"
#include "wlr-data-control-unstable-v1.h"

struct {
    int fd;
    struct wl_display* display;
    struct wl_seat* seat;
    struct wl_registry* registry;
    struct zwlr_data_control_manager_v1* data_control_manager;
    struct zwlr_data_control_device_v1* data_control_device;
} wayland = {0};

static VEC(char *) offered_mime_types;

static const char *pick_mime_type(void) {
    /* finds first offered mime type that matches or returns NULL if none matched */
    VEC_FOREACH(&config.accepted_mime_types, i) {
        VEC_FOREACH(&offered_mime_types, j) {
            const char* pattern = *VEC_AT_UNCHECKED(&config.accepted_mime_types, i);
            const char* type = *VEC_AT_UNCHECKED(&offered_mime_types, j);

            if (fnmatch(pattern, type, 0) == 0) {
                log_print(DEBUG, "selected mime type: %s", type);
                return type;
            }
        }
    }

    return NULL;
}

static size_t receive_data(int fd, char** buffer) {
    /* reads offer into buffer, returns number of bytes read */

    /* is it really a good idea to multiply buffer size by 2 every time? */
    const size_t INITIAL_BUFFER_SIZE = 4096;
    const int GROWTH_FACTOR = 2;

    *buffer = xmalloc(INITIAL_BUFFER_SIZE);

    size_t buffer_size = INITIAL_BUFFER_SIZE;
    size_t total_read = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(fd, *buffer + total_read, buffer_size - total_read)) > 0) {
        total_read += bytes_read;

        if (total_read == buffer_size) {
            buffer_size *= GROWTH_FACTOR;
            *buffer = xrealloc(*buffer, buffer_size);
        }
    }

    if (bytes_read == -1) {
        log_print(ERR, "error reading from pipe: %s", strerror(errno));
    }

    close(fd);

    return total_read;
}

static void receive_offer(struct zwlr_data_control_offer_v1* offer) {
    char* buffer = NULL;

    const char* selected_type = pick_mime_type();
    if (selected_type == NULL) {
        log_print(DEBUG, "didn't match any mime type, not receiving this offer");
        goto out;
    }

    int pipes[2];
    if (pipe(pipes) == -1) {
        log_print(ERR, "failed to create pipe");
        goto out;
    }

    log_print(TRACE, "receiving offer %p...", (void*)offer);
    zwlr_data_control_offer_v1_receive(offer, selected_type, pipes[1]);
    /* make sure the sender received our request and is ready for transfer */
    wl_display_roundtrip(wayland.display);
    close(pipes[1]);

    size_t bytes_read = receive_data(pipes[0], &buffer);
    log_print(TRACE, "done receiving offer %p", (void*)offer);

    if (bytes_read == 0) {
        log_print(WARN, "received 0 bytes");
        goto out;
    } else {
        log_print(DEBUG, "received %" PRIu64 " bytes", bytes_read);
    }

    if (bytes_read < config.min_data_size) {
        log_print(DEBUG, "received less bytes than min_data_size, not saving this entry");
        goto out;
    }

    if (!insert_db_entry(buffer, bytes_read, selected_type)) {
        log_print(ERR, "failed to insert entry into database!");
    };

out:
    free(buffer);
}

static void mime_type_offer_handler(void* data, struct zwlr_data_control_offer_v1* offer,
                                    const char* mime_type) {
    log_print(TRACE, "got mime type offer %s for offer %p", mime_type, (void*)offer);

    VEC_APPEND(&offered_mime_types, &(char *){ xstrdup(mime_type) });
}

const struct zwlr_data_control_offer_v1_listener data_control_offer_listener = {
    .offer = mime_type_offer_handler,
};

static void data_offer_handler(void* data, struct zwlr_data_control_device_v1* device,
                        struct zwlr_data_control_offer_v1* offer) {
    log_print(DEBUG, "got new wlr_data_control_offer %p", (void*)offer);

    zwlr_data_control_offer_v1_add_listener(offer, &data_control_offer_listener, NULL);
}

static void common_selection_handler(struct zwlr_data_control_offer_v1* offer, bool primary) {
    if (offer == NULL) {
        return;
    }

    if (!primary || config.primary_selection) {
        receive_offer(offer);
    } else {
        log_print(DEBUG, "ignoring primary selection event for offer %p", (void*)offer);
    }

    log_print(TRACE, "destroying offer %p", (void*)offer);
    zwlr_data_control_offer_v1_destroy(offer);
    VEC_FOREACH(&offered_mime_types, i) {
        free(*VEC_AT_UNCHECKED(&offered_mime_types, i));
    }
    VEC_CLEAR(&offered_mime_types);
}

static void selection_handler(void* data, struct zwlr_data_control_device_v1* device,
                              struct zwlr_data_control_offer_v1* offer) {
    log_print(DEBUG, "got selection event for offer %p", (void*)offer);
    common_selection_handler(offer, false);
}

static void primary_selection_handler(void* data, struct zwlr_data_control_device_v1* device,
                                      struct zwlr_data_control_offer_v1* offer) {
    log_print(DEBUG, "got primary selection event for offer %p", (void*)offer);
    common_selection_handler(offer, true);
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

int wayland_init(void) {
    wayland.display = wl_display_connect(NULL);
    if (wayland.display == NULL) {
        log_print(ERR, "failed to connect to display");
        return -1;
    }

    wayland.fd = wl_display_get_fd(wayland.display);

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

int wayland_process_events(void) {
    return wl_display_dispatch(wayland.display);
}

