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

#include "log.h"
#define POLLEN_LOG_INFO(fmt, ...) log_print(DEBUG, fmt, ##__VA_ARGS__)
#define POLLEN_LOG_WARN(fmt, ...) log_print(WARN, fmt, ##__VA_ARGS__)
#define POLLEN_LOG_ERR(fmt, ...) log_print(ERR, fmt, ##__VA_ARGS__)

#include "xmalloc.h"
#define POLLEN_CALLOC(n, size) xcalloc((n), (size))
#define POLLEN_FREE(ptr) free(ptr)

#define POLLEN_IMPLEMENTATION
#include "eventloop.h"

struct pollen_loop* eventloop = NULL;

