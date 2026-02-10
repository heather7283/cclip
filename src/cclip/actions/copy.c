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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <sqlite3.h>
#include <wayland-client.h>

#include "actions.h"
#include "../utils.h"
#include "xmalloc.h"
#include "db.h"
#include "getopt.h"
#include "log.h"

#include "wlr-data-control-unstable-v1.h"

struct wayland {
    bool running;

    const void* data;
    size_t data_size;
    const char* mime_type;

    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_seat *seat;
    struct zwlr_data_control_manager_v1 *data_control_manager;
    struct zwlr_data_control_device_v1 *data_control_device;
    struct zwlr_data_control_source_v1 *data_control_source;
};

static void on_data_control_source_cancelled(void* data, struct zwlr_data_control_source_v1* _) {
    struct wayland* wl = data;

    wl->running = false;
}

static void on_data_control_source_send(void* data, struct zwlr_data_control_source_v1* _,
                                        const char* mime_type, int fd) {
    struct wayland* wl = data;

    if (strcmp(mime_type, wl->mime_type)) {
        goto out;
    }

    for (size_t total_wr = 0; total_wr < wl->data_size; ) {
        const ssize_t wr = write(fd, (char*)wl->data + total_wr, wl->data_size - total_wr);
        if (wr < 0) {
            if (errno == EINTR) {
                continue;
            }
            goto out;
        }
        total_wr += wr;
    }

out:
    close(fd);
}

static const struct zwlr_data_control_source_v1_listener data_control_source_listener = {
    .cancelled = on_data_control_source_cancelled,
    .send = on_data_control_source_send,
};

static void on_registry_global(void* data, struct wl_registry* registry, uint32_t name,
                               const char* interface, uint32_t version) {
    struct wayland* wl = data;

    if (!wl->seat && !strcmp(interface, "wl_seat")) {
        wl->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    } else if (strcmp(interface, "zwlr_data_control_manager_v1") == 0) {
        wl->data_control_manager =
            wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, 2);
    }
}

static void on_registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {
    /* no-op */
}

static const struct wl_registry_listener registry_listener = {
    .global = on_registry_global,
    .global_remove = on_registry_global_remove,
};

_Noreturn static void do_copy(const void* data, size_t data_size, const char* mime_type,
                              bool primary_selection, bool stay_in_foreground) {
    int retcode = 0;

    struct wayland* wl = xcalloc(1, sizeof(*wl));
    wl->data = data;
    wl->data_size = data_size;
    wl->mime_type = mime_type;

    wl->display = wl_display_connect(NULL);
    if (!wl->display) {
        log_print(ERR, "failed to connect to wayland display");
        OUT(1);
    }

    wl->registry = wl_display_get_registry(wl->display);
    wl_registry_add_listener(wl->registry, &registry_listener, wl);
    wl_display_roundtrip(wl->display);

    if (!wl->seat) {
        log_print(ERR, "failed to bind wl-seat");
        OUT(1);
    }
    if (!wl->data_control_manager) {
        log_print(ERR, "failed to bind wlr-data-control-manager");
        OUT(1);
    }

    wl->data_control_device =
        zwlr_data_control_manager_v1_get_data_device(wl->data_control_manager, wl->seat);
    if (!wl->data_control_device) {
        log_print(ERR, "couldn't create a data_control_device");
        OUT(1);
    }

    wl->data_control_source =
        zwlr_data_control_manager_v1_create_data_source(wl->data_control_manager);
    if (!wl->data_control_source) {
        log_print(ERR, "couldn't create a data_control_source");
        OUT(1);
    }
    zwlr_data_control_source_v1_add_listener(wl->data_control_source,
                                             &data_control_source_listener, wl);

    zwlr_data_control_source_v1_offer(wl->data_control_source, wl->mime_type);

    void (*f)(struct zwlr_data_control_device_v1*, struct zwlr_data_control_source_v1*);
    if (primary_selection) {
        f = zwlr_data_control_device_v1_set_primary_selection;
    } else {
        f = zwlr_data_control_device_v1_set_selection;
    }
    f(wl->data_control_device, wl->data_control_source);

    wl_display_flush(wl->display);

    if (!stay_in_foreground) {
        daemon(false, false);
    }

    wl->running = true;
    for (int ret = 0; wl->running && ret >= 0; ) {
        ret = wl_display_dispatch(wl->display);
    }

out:
    exit(retcode);
}

static void print_help(void) {
    static const char help[] =
        "Usage:\n"
        "    cclip copy [-pf] ID\n"
        "\n"
        "Command line options:\n"
        "    -p  Copy to primary selection\n"
        "    -f  Stay in foreground\n"
        "    ID  Entry id to copy (- to read from stdin)\n"
    ;

    fputs(help, stdout);
}

void action_copy(int argc, char** argv, struct sqlite3* db) {
    int retcode = 0;
    struct sqlite3_stmt* stmt = NULL;

    bool stay_in_foreground = false;
    bool primary_selection = false;

    RESET_GETOPT();
    int opt;
    while ((opt = getopt(argc, argv, ":pfh")) != -1) {
        switch (opt) {
        case 'p':
            primary_selection = true;
            break;
        case 'f':
            stay_in_foreground = true;
            break;
        case 'h':
            print_help();
            OUT(0);
        case '?':
            log_print(ERR, "unknown option: %c", optopt);
            OUT(1);
        case ':':
            log_print(ERR, "missing arg for %c", optopt);
            OUT(1);
        default:
            log_print(ERR, "error while parsing command line options");
            OUT(1);
        }
    }
    argc = argc - optind;
    argv = &argv[optind];

    if (argc < 1) {
        log_print(ERR, "not enough arguments");
        OUT(1);
    } else if (argc > 1) {
        log_print(ERR, "extra arguments on the command line");
        OUT(1);
    }

    int64_t entry_id;
    if (!get_id(argv[0], &entry_id)) {
        OUT(1);
    }

    const char* sql = "SELECT data, mime_type FROM history WHERE id = @entry_id";

    if (!db_prepare_stmt(db, sql, &stmt)) {
        OUT(1);
    }

    STMT_BIND(stmt, int64, "@entry_id", entry_id);

    int ret = sqlite3_step(stmt);
    if (ret == SQLITE_DONE) {
        log_print(ERR, "no entry found with id %li", entry_id);
        OUT(1);
    } else if (ret != SQLITE_ROW) {
        log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
        OUT(1);
    }

    const size_t data_size = sqlite3_column_bytes(stmt, 0);
    const void* data = xmemdup(sqlite3_column_blob(stmt, 0), data_size);
    const char* mime_type = xstrdup((const char*)sqlite3_column_text(stmt, 1));

    /* at this point we won't need stmt and db anymore */
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    /* does not return */
    do_copy(data, data_size, mime_type, primary_selection, stay_in_foreground);
    assert(!"unreachable");

out:
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    exit(retcode);
}

