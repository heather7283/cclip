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
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <stdio.h>

#include "common.h"
#include "config.h"

#define MAX_PATH_LENGTH 1024

struct config config = {
    .verbose = false,
    .accepted_mime_types_len = 0,
    .accepted_mime_types = NULL,
    .min_data_size = 1,
    .db_path = NULL,
    .primary_selection = false,
    .max_entries_count = 1000
};

char* get_default_db_path(void) {
    char* db_path = malloc(MAX_PATH_LENGTH * sizeof(char));
    if (db_path == NULL) {
        die("failed to allocate memory for db path string\n");
    }

    char* xdg_data_home = getenv("XDG_DATA_HOME");
    if (xdg_data_home != NULL) {
        snprintf(db_path, MAX_PATH_LENGTH, "%s/%s", xdg_data_home, "cclip/db.sqlite3");
    } else {
        char* home = getenv("HOME");
        if (home == NULL) {
            die("both HOME and XDG_DATA_HOME are unset, unable to determine db file path\n");
        }
        snprintf(db_path, MAX_PATH_LENGTH, "%s/.local/share/%s", home, "cclip/db.sqlite3");
    }

    debug("setting default db path: %s\n", db_path);
    return db_path;
}

void config_set_default_values(void) {
    if (config.db_path == NULL) {
        config.db_path = get_default_db_path();
    }
    if (config.accepted_mime_types == NULL) {
        config.accepted_mime_types = malloc(sizeof(char*) * 1);
        config.accepted_mime_types[0] = "*";

        config.accepted_mime_types_len = 1;
    }
}

