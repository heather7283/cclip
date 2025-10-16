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

#pragma once

#include <sys/uio.h>
#include <stdbool.h>
#include <stdint.h>

#define SHIFT(n) (argc -= (n), argv += (n), argc > 0)

bool str_to_int64(const char* str, int64_t* res);

/* if str is null, tries to read stdin */
bool get_id(const char* str, int64_t* res);

#define MAX_FIELD_LIST_SIZE 1024
const char* build_field_list(char* raw_list);

/*
 * writev(2) wrapper that ensure all data gets written
 * NOTE: mutates the iov array
 */
bool writev_full(int fd, struct iovec *iov, int iovcnt);

