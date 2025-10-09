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

/* malloc, but aborts on alloc fail */
void* xmalloc(size_t size);
/* calloc, but aborts on alloc fail */
void* xcalloc(size_t n, size_t size);
/* realloc, but aborts on alloc fail */
void* xrealloc(void* ptr, size_t size);

/* strdup, but aborts on alloc fail and returns NULL when called with NULL */
char* xstrdup(const char* s);

