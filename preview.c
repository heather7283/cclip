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
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <inttypes.h>

#include "preview.h"
#include "common.h"

#define PREVIEW_LEN 128

static size_t sanitise_string(char* str) {
    /*
     * makes sure garbage characters don't leak into preview
     * returns size of modified string because it can change
     */
    if (str == NULL) {
        return 0;
    }

    size_t read = 0;
    size_t write = 0;

    while (str[read] != '\0') {
        if (isprint((unsigned char)str[read]) || str[read] == ' ') {
            /* printable ASCII character or space */
            str[write] = str[read];
            write += 1;
            read += 1;
        } else if (isspace((unsigned char)str[read])) {
            /* other whitespace characters (newline, tab, etc.) */
            str[write] = ' ';
            write += 1;
            read += 1;
        } else {
            /* Check for UTF-8 multi-byte sequence */
            int utf8_len = 0;
            unsigned char first_byte = (unsigned char)str[read];

            if ((first_byte & 0x80) == 0) {
                utf8_len = 1;  /* ASCII char (0xxxxxxx) */
            } else if ((first_byte & 0xE0) == 0xC0) {
                utf8_len = 2;  /* 2-byte UTF-8 (110xxxxx) */
            } else if ((first_byte & 0xF0) == 0xE0) {
                utf8_len = 3;  /* 3-byte UTF-8 (1110xxxx) */
            } else if ((first_byte & 0xF8) == 0xF0) {
                utf8_len = 4;  /* 4-byte UTF-8 (11110xxx) */
            }

            if (utf8_len > 1 && read + utf8_len <= strlen(str)) {
                /* valid multibyte UTF-8 sequence */
                for (int i = 0; i < utf8_len; i++) {
                    str[write] = str[read];
                    write += 1;
                    read += 1;
                }
            } else {
                /* invalid or unprintable character */
                str[write] = '?';
                write += 1;
                read += 1;
            }
        }
    }

    /* dont forget the null terminator */
    str[write] = '\0';

    return write;
}

static size_t lstrip(char* str) {
    /*
     * removes leading whitespace from str
     * returns modified string length
     */
    if (str == NULL) {
        return 0;
    }

    size_t read = 0;
    size_t write = 0;
    bool non_whitespace_found = false;

    while (str[read] != '\0') {
        if (isspace((unsigned char)str[read]) && !non_whitespace_found) {
            read += 1;
        } else {
            non_whitespace_found = true;

            str[write] = str[read];
            write += 1;
            read += 1;
        }
    }

    /* dont forget the null terminator */
    str[write] = '\0';

    return write;
}

char* generate_preview(const void* const data, size_t preview_len,
                       const size_t data_size, const char* const mime_type) {
    char* preview = calloc(preview_len, sizeof(char));
    if (preview == NULL) {
        die("failed to allocate memory for preview string\n");
    }

    if (fnmatch("*text*", mime_type, 0) == 0) {
        strncpy(preview, data, min(data_size, preview_len));
        sanitise_string(preview);
        lstrip(preview);
    } else {
        snprintf(preview, preview_len, "%s | %" PRIi64 " bytes", mime_type, data_size);
    }

    debug("generated preview: %s\n", preview);

    return preview;
}

