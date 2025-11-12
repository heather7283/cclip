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

#include "config.h"

struct config config = {
    .accepted_mime_types = {0},
    .min_data_size = 1,
    .db_path = NULL,
    .primary_selection = false,
    .ignore_secrets = true,
    .max_entries_count = 1000,
    .create_db_if_not_exists = true,
    .preview_len = 128,
    .loglevel = INFO,
};

