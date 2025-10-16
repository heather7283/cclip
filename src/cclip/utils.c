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

static ssize_t strtcpy(char* restrict dst, const char* restrict src, size_t dsize) {
    bool    trunc;
    size_t  dlen, slen;

    if (dsize == 0) {
        errno = ENOBUFS;
        return -1;
    }

    slen = strnlen(src, dsize);
    trunc = (slen == dsize);
    dlen = slen - trunc;

    stpcpy(mempcpy(dst, src, dlen), "");
    if (trunc) {
        errno = E2BIG;
        return -1;
    }
    return slen;
}

static char* stpecpy(char* dst, char* end, const char* restrict src) {
    ssize_t dlen;

    if (dst == NULL) {
        return NULL;
    }

    dlen = strtcpy(dst, src, end - dst);
    return (dlen == -1) ? NULL : dst + dlen;
}

const char* build_field_list(char* raw_list) {
    enum fields {
        ROWID, TIMESTAMP, MIME_TYPE, PREVIEW, DATA_SIZE, TAG, ALLOWED_FIELDS_COUNT
    };
    const char** allowed_fields[ALLOWED_FIELDS_COUNT];
    allowed_fields[ROWID]     = (const char*[]){"rowid", "id", NULL};
    allowed_fields[TIMESTAMP] = (const char*[]){"timestamp", "time", NULL};
    allowed_fields[MIME_TYPE] = (const char*[]){"mime_type", "mime", "type", NULL};
    allowed_fields[PREVIEW]   = (const char*[]){"preview", NULL};
    allowed_fields[DATA_SIZE] = (const char*[]){"data_size", "size", NULL};
    allowed_fields[TAG]       = (const char*[]){"tag", NULL};

    static char result_list[MAX_FIELD_LIST_SIZE];
    char *result_pos = result_list;
    char *result_end = result_list + sizeof(result_list);

    char* token = strtok(raw_list, ",");
    while (token != NULL) {
        bool token_valid = false;
        for (int i = 0; i < ALLOWED_FIELDS_COUNT; i++) {
            for (int j = 0; allowed_fields[i][j] != NULL; j++) {
                if (strcmp(token, allowed_fields[i][j]) == 0) {
                    result_pos = stpecpy(result_pos, result_end, allowed_fields[i][0]);
                    result_pos = stpecpy(result_pos, result_end, ",");
                    if (result_pos == NULL) {
                        log_print(ERR, "field list is too long");
                        return NULL;
                    }

                    token_valid = true;
                    goto loop_out;
                }
            }
        }
    loop_out:

        if (!token_valid) {
            log_print(ERR, "invalid field: %s", token);
            return NULL;
        }

        token = strtok(NULL, ",");
    }

    if (result_pos > result_list && *(result_pos - 1) == ',') {
        *(result_pos - 1) = '\0';
    }

    return result_list;
}

/*
 * writev(2) wrapper that ensures all data gets written.
 * NOTE: mutates iov array
 */
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

