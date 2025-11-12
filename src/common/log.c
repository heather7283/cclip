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

#include <sys/uio.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "log.h"

static struct {
    int fd;
    enum loglevel level;
} log_config = {
    .fd = -1,
    .level = LOGLEVEL_SILENT,
};

static const struct {
    char* prefix;
    size_t len;
} log_prefixes[] = {
    #define PREFIX(level, prefix) [level] = { prefix, strlen(prefix) }

    PREFIX(ERR, "error: "),
    PREFIX(WARN, "warning: "),
    PREFIX(INFO, "info: "),
    PREFIX(DEBUG, "debug: "),
    PREFIX(TRACE, "trace: "),

    #undef PREFIX
};

void log_init(int fd, enum loglevel level) {
    log_config.fd = fd;
    log_config.level = level;
}

void log_print(enum loglevel level, const char* fmt, ...) {
    if (log_config.level < level) {
        return;
    }

    char buf[1024];
    int buf_len;

    va_list args;
    va_start(args, fmt);
    buf_len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    struct iovec iov[3] = {
        {
            .iov_base = log_prefixes[level].prefix,
            .iov_len = log_prefixes[level].len
        },
        {
            .iov_base = buf,
            .iov_len = buf_len
        },
        {
            .iov_base = "\n",
            .iov_len = 1
        }
    };

    writev(log_config.fd, iov, 3);
}

