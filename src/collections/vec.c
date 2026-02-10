/*
 * This file is part of cclip, clipboard manager for wayland
 * Copyright (C) 2026  heather7283
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

#include <stdio.h>
#include <string.h>

#include "collections/vec.h"
#include "xmalloc.h"

#define MAX(a, b) ({ \
    const __typeof__(a) _a = (a); \
    const __typeof__(b) _b = (b); \
    _a > _b ? _a : _b; \
})

#define GROWTH_FACTOR 2

static void vec_ensure_capacity(struct vec_generic *vec, size_t elem_size, size_t cap) {
    if (cap > vec->capacity) {
        const size_t new_cap = MAX(cap, vec->capacity * GROWTH_FACTOR);
        vec->data = xreallocarray(vec->data, new_cap, elem_size);
        vec->capacity = new_cap;
    }
}

static size_t vec_bound_check(const struct vec_generic *vec, size_t index) {
    if (index >= vec->size) {
        fprintf(stderr, "Index %zu is out of bounds of vec of size %zu", index, vec->size);
        fflush(stderr);
        abort();
    }
    return index;
}

void *vec_insert_generic(struct vec_generic *vec, size_t index,
                         const void *elems, size_t elem_size, size_t elem_count,
                         bool zero_init) {
    vec_bound_check(vec, index);
    vec_ensure_capacity(vec, elem_size, vec->size + elem_count);

    /* shift existing elements to make space for new ones */
    memmove((char *)vec->data + ((index + elem_count) * elem_size),
            (char *)vec->data + (index * elem_size),
            (vec->size - index) * elem_size);

    /* copy new elements to vec */
    if (elems != NULL) {
        memcpy((char *)vec->data + (index * elem_size), elems, elem_size * elem_count);
    } else if (zero_init) {
        memset((char *)vec->data + (index * elem_size), '\0', elem_size * elem_count);
    }
    vec->size += elem_count;

    return (char *)vec->data + (index * elem_size);
}

void *vec_append_generic(struct vec_generic *vec, const void *elems,
                         size_t elem_size, size_t elem_count, bool zero_init) {
    vec_ensure_capacity(vec, elem_size, vec->size + elem_count);

    /* append new elements to the end */
    if (elems != NULL) {
        memcpy((char *)vec->data + (vec->size * elem_size), elems, elem_size * elem_count);
    } else if (zero_init) {
        memset((char *)vec->data + (vec->size * elem_size), '\0', elem_size * elem_count);
    }
    vec->size += elem_count;

    return (char *)vec->data + ((vec->size - elem_count) * elem_size);
}

void vec_erase_generic(struct vec_generic *vec, size_t index,
                       size_t elem_size, size_t elem_count) {
    vec_bound_check(vec, index);
    vec_bound_check(vec, index + elem_count - 1);

    memmove((char *)vec->data + (index * elem_size),
            (char *)vec->data + ((index + elem_count) * elem_size),
            (vec->size - index - elem_count) * elem_size);
    vec->size -= elem_count;
}

void *vec_at_generic(struct vec_generic *vec, size_t index, size_t elem_size) {
    vec_bound_check(vec, index);
    return (char *)vec->data + (index * elem_size);
}

void vec_clear_generic(struct vec_generic *vec) {
    vec->size = 0;
}

void vec_free_generic(struct vec_generic *vec) {
    vec->size = 0;
    vec->capacity = 0;
    free(vec->data);
    vec->data = NULL;
}

void vec_reserve_generic(struct vec_generic *vec, size_t elem_size, size_t elem_count) {
    vec_ensure_capacity(vec, elem_size, elem_count);
}

