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

struct config {
    bool verbose;
    int accepted_mime_types_len;
    char** accepted_mime_types;
    size_t min_data_size;
    char* db_path;
    bool primary_selection;
    int max_entries_count;
    bool create_db_if_not_exists;
};
extern struct config config;

void config_set_default_values(void);

char* get_default_db_path(void);

#endif /* #ifndef CONFIG_H */
