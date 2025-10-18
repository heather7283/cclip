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

#include <stddef.h>
#include <stdbool.h>

struct string {
    size_t len; /* without null terminator */
    size_t capacity; /* with null terminator */
    char* str; /* always null terminated (unless NULL) */
};

void string_clear(struct string* str);
void string_free(struct string* str);

bool string_reserve(struct string* str, size_t len);

bool string_append(struct string* str, const char* suffix);
bool string_appendn(struct string* str, const char* suffix, size_t suffix_len);

bool string_appendf(struct string* str, const char* fmt, ...);
bool string_appendfn(struct string* str, size_t maxlen, const char* fmt, ...);

