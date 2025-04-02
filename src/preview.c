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
#include <stdbool.h>
#include <fnmatch.h>
#include <inttypes.h>

#include "preview.h"
#include "common.h"
#include "xmalloc.h"

static void generate_text_preview(char* out_buf, const char* const in_buf,
                                  size_t preview_len, size_t data_len) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    bool last_was_space = true;

    preview_len -= 1; /* null terminator */

    while (in_pos < data_len && out_pos < preview_len) {
        unsigned char c = (unsigned char)in_buf[in_pos];

        /* ASCII control characters (also space) */
        if (c <= 0x20 || c == 0x7F) {
            if (c == '\t' || c == '\n' || c == '\r' || c == ' ') {
                if (!last_was_space) {
                    out_buf[out_pos++] = ' ';
                    last_was_space = true;
                }
            }
            in_pos++;
            continue;
        }

        /* ASCII printable characters */
        if (c <= 0x7F) {
            out_buf[out_pos++] = c;
            last_was_space = false;
            in_pos++;
            continue;
        }

        /* UTF-8 from here */
        size_t seq_len = 0;
        uint32_t code_point = 0;
        if ((c & 0xE0) == 0xC0) {
            seq_len = 2;
            code_point = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            seq_len = 3;
            code_point = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            seq_len = 4;
            code_point = c & 0x07;
        } else {
            /* invalid UTF-8 */
            out_buf[out_pos++] = '?';
            in_pos++;
            continue;
        }

        /* check if UTF-8 sequence fits into both buffers */
        if (in_pos + seq_len > data_len || out_pos + seq_len > preview_len) {
            out_buf[out_pos++] = '?';
            break;
        }

        /* validate continuation bytes and build codepoint */
        bool seq_valid = true;
        for (size_t i = 1; i < seq_len; i++) {
            unsigned char b = (unsigned char)in_buf[in_pos + i];
            if ((b & 0xC0) != 0x80) {
                seq_valid = false;
                break;
            }
            /* I heckin love bitwise operations (no I don't) */
            code_point = (code_point << 6) | (b & 0x3F);
        }

        /* check for illegal overlong encodings and invalid code points
         * I'm getting serious brain damage from this shit and I hate UTF-8 */
        bool valid_range = false;
        if (seq_len == 2) {
            valid_range = code_point >= 0x80 && code_point <= 0x7FF;
        } else if (seq_len == 3) {
            valid_range = code_point >= 0x800 && code_point <= 0xFFFF;
        } else if (seq_len == 4) {
            valid_range = code_point >= 0x10000 && code_point <= 0x10FFFF;
        }
        /* surrogate pairs (whatever that means, some windows bullshit) */
        if (code_point >= 0xD800 && code_point <= 0xDFFF) {
            valid_range = false;
        }

        if (seq_valid && valid_range) {
            for (size_t i = 0; i < seq_len; i++) {
                out_buf[out_pos++] = in_buf[in_pos++];
            }
        } else {
            out_buf[out_pos++] = '?';
            in_pos++;
        }
        last_was_space = false;
    }

    out_buf[out_pos] = '\0';
}

static void generate_binary_preview(char* const out_buf, size_t preview_len,
                                    size_t data_size, const char* const mime_type) {
    static const char* units[] = {"B", "KiB", "MiB"};
    int units_index = 0;
    double size = data_size;

    while (size >= 1024 && units_index < 2) {
        size /= 1024;
        units_index += 1;
    }

    if (units_index == 0) {
        snprintf(out_buf, preview_len, "%s | %" PRIu64 " B", mime_type, data_size);
    } else {
        snprintf(out_buf, preview_len, "%s | %" PRIu64 " B (%.2f %s)",
                 mime_type, data_size, size, units[units_index]);
    }
}

char* generate_preview(const void* const data, size_t preview_len,
                       size_t data_size, const char* const mime_type) {
    char* preview = xcalloc(preview_len, sizeof(char));

    if (fnmatch("text/*", mime_type, 0) == 0) {
        generate_text_preview(preview, data, preview_len, data_size);
    } else {
        generate_binary_preview(preview, preview_len, data_size, mime_type);
    }

    debug("generated preview: %s\n", preview);

    return preview;
}

