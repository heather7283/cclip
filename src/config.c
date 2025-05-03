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
#include <stdlib.h>
#include <limits.h>

#include "config.h"
#include "common.h"

struct config config = {
    .accepted_mime_types_count = 0,
    .accepted_mime_types = {0},
    .min_data_size = 1,
    .db_path = NULL,
    .primary_selection = false,
    .max_entries_count = 1000,
    .create_db_if_not_exists = true,
    .preview_len = 128,
};

const char* get_default_db_path(void) {
    static char db_path[PATH_MAX];

    char* xdg_data_home = getenv("XDG_DATA_HOME");
    char* home = getenv("HOME");

    if (xdg_data_home != NULL) {
        snprintf(db_path, sizeof(db_path), "%s/cclip/db.sqlite3", xdg_data_home);
    } else if (home != NULL) {
        snprintf(db_path, sizeof(db_path), "%s/.local/share/cclip/db.sqlite3", home);
    } else {
        err("both HOME and XDG_DATA_HOME are unset, unable to determine db file path\n");
        return NULL;
    }

    return db_path;
}

