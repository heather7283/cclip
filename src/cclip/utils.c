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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "macros.h"
#include "log.h"

bool str_to_int64(const char* str, int64_t* res) {
    char *endptr = NULL;

    errno = 0;
    int64_t res_tmp = strtoll(str, &endptr, 10);

    if (errno == 0 && *endptr == '\0') {
        *res = res_tmp;
        return true;
    }
    log_print(ERR, "failed to convert %s to int64", str);
    return false;
}

bool int64_from_stdin(int64_t* res) {
    int64_t res_tmp;
    if (scanf("%ld", &res_tmp) != 1) {
        log_print(ERR, "failed to read a number from stdin");
        return false;
    };
    *res = res_tmp;
    return true;
}

bool get_id(const char* str, int64_t* res) {
    if (strcmp(str, "-") == 0) {
        return int64_from_stdin(res);
    } else {
        return str_to_int64(str, res);
    }
}

int build_field_list(char* raw_list, enum select_fields out_fields[SELECT_FIELDS_COUNT]) {
    #define FOR_LIST_OF_FIELDS(DO) \
        DO(ID, "id", "rowid") \
        DO(PREVIEW, "preview") \
        DO(MIME_TYPE, "mime_type", "mime", "type") \
        DO(DATA_SIZE, "data_size", "size") \
        DO(TIMESTAMP, "timestamp", "time") \
        DO(TAGS, "tags", "tag")

    #define DEFINE_NAME_ARRAY(name, ...) \
        static const char* name##_names[] = { __VA_ARGS__ };
    FOR_LIST_OF_FIELDS(DEFINE_NAME_ARRAY)

    static const struct {
        const char** names;
        unsigned names_count;
    } fields[] = {
        #define DEFINE_STRUCT_MEMBER(n, ...) \
            [FIELD_##n] = { .names = n##_names, .names_count = SIZEOF_ARRAY(n##_names) },
        FOR_LIST_OF_FIELDS(DEFINE_STRUCT_MEMBER)
    };

    bool seen_fields[SELECT_FIELDS_COUNT];
    memset(&seen_fields, 0, sizeof(seen_fields));

    int out_fields_count = 0;

    char* token = strtok(raw_list, ",");
    while (token != NULL) {
        bool token_valid = false;
        for (unsigned f = 0; f < SIZEOF_ARRAY(fields); f++) {
            for (unsigned j = 0; j < fields[f].names_count; j++) {
                if (STREQ(token, fields[f].names[j])) {
                    if (seen_fields[f]) {
                        log_print(ERR, "field %s encountered more than once", token);
                        return 0;
                    }

                    seen_fields[f] = true;
                    out_fields[out_fields_count++] = f;
                    token_valid = true;
                    goto loop_out;
                }
            }
        }
    loop_out:

        if (!token_valid) {
            log_print(ERR, "invalid field: %s", token);
            return 0;
        }

        token = strtok(NULL, ",");
    }

    return out_fields_count;
}

bool writev_full(int fd, struct iovec *iov, int iovcnt) {
    while (iovcnt > 0) {
        ssize_t written = writev(fd, iov, iovcnt);
        if (written <= 0) {
            if (written < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }

        while (written > 0) {
            if (iov[0].iov_len <= (size_t)written) {
                /* entire iov was consumed */
                written -= iov[0].iov_len;
                iov += 1;
                iovcnt -= 1;
            } else {
                /* iov was partially consumed */
                iov[0].iov_base = (char *)iov[0].iov_base + written;
                iov[0].iov_len -= written;
                written = 0;
            }
        }
    }

    return true;
}

bool is_tag_valid(const char* tag) {
    bool has_nonspace = false;
    for (const char* p = tag; *p != '\0'; p++) {
        const char c = *p;
        if (c < 0x20 || c > 0x7E || c == ',') {
            return false;
        } else if (c == ' ') {
            continue;
        } else {
            has_nonspace = true;
        }
    }

    return has_nonspace;
}

