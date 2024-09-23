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
#include <stddef.h> /* NULL */
#include <stdbool.h>
#include <string.h>
#include <wayland-client.h>

#include <wlr-data-control-unstable-v1-client-protocol.h>
#include "wayland.h"
#include "common.h"

struct wl_display* display = NULL;
struct wl_seat* seat = NULL;
struct wl_registry* registry = NULL;
struct zwlr_data_control_manager_v1* data_control_manager = NULL;
struct zwlr_data_control_device_v1* data_control_device = NULL;
int wayland_fd = -1;

static bool seat_found = false;

/* TODO: find out why this exists
static void seat_capabilities(void* data, struct wl_seat* seat, uint32_t cap) {}

static void seat_name(void* data, struct wl_seat* _seat, const char* name) {
	if (!seat_found) {
		seat_found = true;
		seat = _seat;
	} else {
        wl_seat_destroy(_seat);
    }
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
	.name = seat_name,
};
*/

static void registry_global(void* data, struct wl_registry* registry, uint32_t name,
                            const char* interface, uint32_t version) {
    UNUSED(data);
    UNUSED(version);

	if (!seat_found && strcmp(interface, "wl_seat") == 0) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface, 2);
		seat_found = true;
	} else if (strcmp(interface, "zwlr_data_control_manager_v1") == 0) {
		data_control_manager = wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, 2);
	}
}

static void registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {
    UNUSED(data);
    UNUSED(registry);
    UNUSED(name);
}

const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

/* boilerplate code to initialise some required wayland stuff */
void wayland_init(void) {
	display = wl_display_connect(NULL);
	if (display == NULL) {
		die("failed to connect to display\n");
    }

    wayland_fd = wl_display_get_fd(display);

	registry = wl_display_get_registry(display);
	if (registry == NULL) {
		die("failed to get registry\n");
    }

	wl_registry_add_listener(registry, &registry_listener, NULL);

	wl_display_roundtrip(display);

	if (seat == NULL) {
		die("failed to bind to seat interface\n");
    }

	if (data_control_manager == NULL) {
		die("failed to bind to data_control_manager interface\n");
    }

	data_control_device = zwlr_data_control_manager_v1_get_data_device(data_control_manager, seat);
    if (data_control_device == NULL) {
		die("data device is null\n");
    }
}

void wayland_cleanup(void) {
    if (data_control_device) {
        zwlr_data_control_device_v1_destroy(data_control_device);
    }
    if (data_control_manager) {
        zwlr_data_control_manager_v1_destroy(data_control_manager);
    }
    if (seat) {
        wl_seat_destroy(seat);
    }
    if (registry) {
        wl_registry_destroy(registry);
    }
    if (display) {
        wl_display_disconnect(display);
    }
}
