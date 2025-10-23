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
#include <stdio.h>
#include <string.h>

#include "xmalloc.h"

static void* check_alloc(void* alloc) {
    if (!alloc) {
        fprintf(stderr, "memory allocation failed, buy more ram lol\n");
        fflush(stderr);
        abort();
    }

    return alloc;
}

void* xmalloc(size_t size) {
    return check_alloc(malloc(size));
}

void *xcalloc(size_t n, size_t size) {
    return check_alloc(calloc(n, size));
}

void* xrealloc(void* ptr, size_t size) {
    return check_alloc(realloc(ptr, size));
}

char* xstrdup(const char* s) {
    return s ? check_alloc(strdup(s)) : 0;
}

