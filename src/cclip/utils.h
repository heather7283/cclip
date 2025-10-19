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

bool str_to_int64(const char* str, int64_t* res);

/* if str is "-", tries to read stdin */
bool get_id(const char* str, int64_t* res);

enum select_fields {
    FIELD_ID = 0,
    FIELD_PREVIEW = 1,
    FIELD_MIME_TYPE = 2,
    FIELD_DATA_SIZE = 3,
    FIELD_TIMESTAMP = 4,
    FIELD_TAGS = 5,

    SELECT_FIELDS_COUNT
};

/*
 * Returns number of fields in raw_list, puts converted fields into fields array in order.
 * NOTE: mutates raw_list.
 */
int build_field_list(char* raw_list, enum select_fields fields[SELECT_FIELDS_COUNT]);

/*
 * writev(2) wrapper that ensure all data gets written
 * NOTE: mutates the iov array
 */
bool writev_full(int fd, struct iovec *iov, int iovcnt);

/*
 * Disallow non-printable ASCII in tags,
 * also check if there is at least one non-space character.
 * TODO: make this more sane
 */
bool is_tag_valid(const char* tag);

