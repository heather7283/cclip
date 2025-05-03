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
#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_ACCEPTED_MIME_TYPES 64
struct config {
    int accepted_mime_types_count;
    char* accepted_mime_types[MAX_ACCEPTED_MIME_TYPES];
    size_t min_data_size;
    const char* db_path;
    bool primary_selection;
    int max_entries_count;
    bool create_db_if_not_exists;
    size_t preview_len;
};

extern struct config config;

const char* get_default_db_path(void);

#endif /* #ifndef CONFIG_H */

