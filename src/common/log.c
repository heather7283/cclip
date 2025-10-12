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

#include <stdarg.h>

#include "log.h"

static struct {
    FILE *stream;
    enum loglevel level;
} log_config = {
    .stream = NULL,
    .level = LOGLEVEL_SILENT,
};

void log_init(FILE *stream, enum loglevel level) {
    log_config.stream = stream;
    log_config.level = level;

    setvbuf(log_config.stream, NULL, _IOLBF, 0);
}

void log_print(enum loglevel level, const char* fmt, ...) {
    if (log_config.level < level) {
        return;
    }

    switch (level) {
    case LOGLEVEL_SILENT:
        return;
    case ERR:
        fprintf(log_config.stream, "error: ");
        break;
    case WARN:
        fprintf(log_config.stream, "warning: ");
        break;
    case INFO:
        fprintf(log_config.stream, "info: ");
        break;
    case DEBUG:
        fprintf(log_config.stream, "debug: ");
        break;
    default:
        fprintf(log_config.stream, "trace: ");
        break;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(log_config.stream, fmt, args);
    va_end(args);

    fprintf(log_config.stream, "\n");
}

