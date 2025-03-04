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

static void handle_alloc_failure(void) {
    fprintf(stderr, "memory allocation failed, buy more ram lol\n");
    fflush(stderr);
    abort();
}

void* xmalloc(size_t size) {
    void* alloc = malloc(size);
    if (alloc == NULL) {
        handle_alloc_failure();
    }
    return alloc;
}

void *xcalloc(size_t n, size_t size) {
    void* alloc = calloc(n, size);
    if (alloc == NULL) {
        handle_alloc_failure();
    }
    return alloc;
}

void* xrealloc(void* ptr, size_t size) {
    void* alloc = realloc(ptr, size);
    if (alloc == NULL) {
        handle_alloc_failure();
    }
    return alloc;
}

char* xstrdup(const char* s) {
    if (s == NULL) {
        return NULL;
    }

    char* alloc = strdup(s);
    if (alloc == NULL) {
        handle_alloc_failure();
    }
    return alloc;
}

