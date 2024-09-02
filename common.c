#define _POSIX_C_SOURCE 2 // getopt
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>

#include "protocol/wlr-data-control-unstable-v1-client-protocol.h"
#include "common.h"

struct config config = {
    .debug_level = 2,
    .db_path = NULL
};

static bool seat_found = false;

struct wl_seat* seat;
struct zwlr_data_control_manager_v1* data_control_manager;

static void seat_capabilities(void* data, struct wl_seat* seat, uint32_t cap) {}

static void seat_name(void *data, struct wl_seat *_seat, const char *name) {
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

void registry_global(void* data, struct wl_registry* registry, uint32_t name, const char *interface, uint32_t version) {
	if (!seat_found && strcmp(interface, "wl_seat") == 0) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface, 2);
		seat_found = true;
	} else if (strcmp(interface, "zwlr_data_control_manager_v1") == 0) {
		data_control_manager = wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, 2);
	}
}

void registry_global_remove(void *data, struct wl_registry* registry, uint32_t name) {}

const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

void die(const char* const message) {
	fprintf(stderr, "critical: %s\n", message);
	exit(1);
}

void warn(const char* const message) {
    if (config.debug_level >= 1) {
        fprintf(stderr, "warn: %s\n", message);
    }
}

void debug(const char* const message) {
    if (config.debug_level >= 2) {
        fprintf(stderr, "debug: %s\n", message);
    }
}

