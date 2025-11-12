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

struct vec_generic {
    size_t size, capacity;
    void *data;
};

#define VEC(type) \
    struct { \
        size_t size, capacity; \
        type *data; \
    }

#define VEC_INITALISER {0}

#define VEC_INIT(pvec) \
    do { \
        (pvec)->size = (pvec)->capacity = 0; \
        (pvec)->data = NULL; \
    } while (0)


#define VEC_SIZE(pvec) ((pvec)->size)
#define VEC_DATA(pvec) ((pvec)->data)

#define VEC_TYPEOF_DATA(pvec) __typeof__((pvec)->data)
#define VEC_SIZEOF_DATA(pvec) sizeof(*(pvec)->data)

#define VEC_TYPECHECK_SELF(pvec) ({ \
    _Static_assert((__builtin_offsetof(struct vec_generic, size) == \
                    __builtin_offsetof(__typeof__(*pvec), size)) && \
                   (__builtin_offsetof(struct vec_generic, capacity) == \
                    __builtin_offsetof(__typeof__(*pvec), capacity)) && \
                   (__builtin_offsetof(struct vec_generic, data) == \
                    __builtin_offsetof(__typeof__(*pvec), data)), \
                   "passed structure is not a vector"); \
})

#define VEC_TYPECHECK_PELEM(pvec, pelem) ({ \
    _Static_assert(__builtin_types_compatible_p(VEC_TYPEOF_DATA(pvec), __typeof__(pelem)), \
                   "type mismatch between vector contents and passed argument"); \
})


/*
 * Insert elem_count elements, each of size elem_size, in vec at index.
 * If elems is not NULL, elements are initialised from elems.
 * If elems is NULL and zero_init is true, memory is zero-initialised.
 * If elems is NULL and zero_init is false, memory is NOT initialised.
 * Returns address of the first inserted element.
 * Dumps core on OOB access.
 */
void *vec_insert_generic(struct vec_generic *vec, size_t index,
                         const void *elems, size_t elem_size, size_t elem_count,
                         bool zero_init);

#define VEC_INSERT_N(pvec, index, pelem, nelem) ({ \
    VEC_TYPECHECK_SELF(pvec); \
    VEC_TYPECHECK_PELEM(pvec, pelem); \
    vec_insert_generic((struct vec_generic *)(pvec), (index), \
                       (pelem), VEC_SIZEOF_DATA(pvec), (nelem), false); \
})

#define VEC_INSERT(pvec, index, pelem) \
    VEC_INSERT_N(pvec, index, pelem, 1)

#define VEC_EMPLACE_INTERNAL_DO_NOT_USE(pvec, index, nelem, zeroed) ({ \
    VEC_TYPECHECK_SELF(pvec); \
    (VEC_TYPEOF_DATA(pvec))vec_insert_generic((struct vec_generic *)(pvec), (index), \
                                              NULL, VEC_SIZEOF_DATA(pvec), (nelem), \
                                              (zeroed)); \
})

#define VEC_EMPLACE_N(pvec, index, nelem) \
    VEC_EMPLACE_INTERNAL_DO_NOT_USE(pvec, index, nelem, false)

#define VEC_EMPLACE(pvec, index) \
    VEC_EMPLACE_N(pvec, index, 1)

#define VEC_EMPLACE_N_ZEROED(pvec, index, nelem) \
    VEC_EMPLACE_INTERNAL_DO_NOT_USE(pvec, index, nelem, true)

#define VEC_EMPLACE_ZEROED(pvec, index) \
    VEC_EMPLACE_N_ZEROED(pvec, index, 1)

/*
 * Appends elem_count elements, each of size elem_size, to the end of vec.
 * If elems is not NULL, elements are initialised from elems.
 * If elems is NULL and zero_init is true, memory is zero-initialised.
 * If elems is NULL and zero_init is false, memory is NOT initialised.
 * Returns address of the first appended element.
 */
void *vec_append_generic(struct vec_generic *vec, const void *elems,
                         size_t elem_size, size_t elem_count, bool zero_init);

#define VEC_APPEND_N(pvec, pelem, nelem) ({ \
    VEC_TYPECHECK_SELF(pvec); \
    VEC_TYPECHECK_PELEM(pvec, pelem); \
    vec_append_generic((struct vec_generic *)(pvec), (pelem), \
                       VEC_SIZEOF_DATA(pvec), (nelem), false); \
})

#define VEC_APPEND(pvec, pelem) \
    VEC_APPEND_N(pvec, pelem, 1)

#define VEC_EMPLACE_BACK_INTERNAL_DO_NOT_USE(pvec, nelem, zeroed) ({ \
    VEC_TYPECHECK_SELF(pvec); \
    (VEC_TYPEOF_DATA(pvec))vec_append_generic((struct vec_generic *)(pvec), NULL, \
                                              VEC_SIZEOF_DATA(pvec), (nelem), (zeroed)); \
})

#define VEC_EMPLACE_BACK_N(pvec, nelem) \
    VEC_EMPLACE_BACK_INTERNAL_DO_NOT_USE(pvec, nelem, false)

#define VEC_EMPLACE_BACK(pvec) \
    VEC_EMPLACE_BACK_N(pvec, 1)

#define VEC_EMPLACE_BACK_N_ZEROED(pvec, nelem) \
    VEC_EMPLACE_BACK_INTERNAL_DO_NOT_USE(pvec, nelem, true)

#define VEC_EMPLACE_BACK_ZEROED(pvec) \
    VEC_EMPLACE_BACK_N_ZEROED(pvec, 1)

/*
 * Removes elem_count elements, each of size elem_size, at index from vec.
 * Dumps core on OOB access.
 */
void vec_erase_generic(struct vec_generic *vec, size_t index,
                       size_t elem_size, size_t elem_count);

#define VEC_ERASE_N(pvec, index, count) ({ \
    VEC_TYPECHECK_SELF(pvec); \
    vec_erase_generic((struct vec_generic *)(pvec), (index), VEC_SIZEOF_DATA(pvec), (count)); \
})

#define VEC_ERASE(pvec, index) \
    VEC_ERASE_N(pvec, index, 1)

/*
 * Returns pointer to element of vec at index.
 * Dumps core on OOB access.
 */
void *vec_at_generic(struct vec_generic *vec, size_t index, size_t elem_size);

#define VEC_AT(pvec, index) ({ \
    VEC_TYPECHECK_SELF(pvec); \
    (VEC_TYPEOF_DATA(pvec))vec_at_generic((struct vec_generic *)(pvec), \
                                          (index), VEC_SIZEOF_DATA(pvec)); \
})

#define VEC_AT_UNCHECKED(pvec, index) (&(pvec)->data[index])

/*
 * Sets size to 0 but does not free memory.
 */
void vec_clear_generic(struct vec_generic *vec);

#define VEC_CLEAR(pvec) ({ \
    VEC_TYPECHECK_SELF(pvec); \
    vec_clear_generic((struct vec_generic *)(pvec)); \
})

/*
 * Frees all memory and makes vec ready for reuse.
 */
void vec_free_generic(struct vec_generic *vec);

#define VEC_FREE(pvec) ({ \
    VEC_TYPECHECK_SELF(pvec); \
    vec_free_generic((struct vec_generic *)(pvec)); \
})

/*
 * Reserves memory for elem_count elements, each of size elem_size.
 */
void vec_reserve_generic(struct vec_generic *vec, size_t elem_size, size_t elem_count);

#define VEC_RESERVE(pvec, count) ({ \
    VEC_TYPECHECK_SELF(pvec); \
    vec_reserve_generic((struct vec_generic *)(pvec), VEC_SIZEOF_DATA(pvec), (count)); \
})


#define VEC_FOREACH(pvec, iter) \
    for (size_t iter = 0; iter < (pvec)->size; iter++)

#define VEC_FOREACH_REVERSE(pvec, iter) \
    for (size_t iter = (pvec)->size; iter-- > 0; /* no-op */)

