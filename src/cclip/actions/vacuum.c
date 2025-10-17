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

#include <stdio.h>
#include <stdlib.h>

#include <sqlite3.h>

#include "getopt.h"
#include "log.h"

static void print_help_and_exit(FILE *stream, int rc) {
    const char *help =
        "Usage:\n"
        "    cclip vacuum\n"
        "\n"
        "Command line options:\n"
        "    cclip vacuum does not take command line options\n"
    ;

    fprintf(stream, "%s", help);
    exit(rc);
}

int action_vacuum(int argc, char** argv, struct sqlite3* db) {
    int opt;
    optreset = 1;
    optind = 0;
    while ((opt = getopt(argc, argv, ":h")) != -1) {
        switch (opt) {
        case 'h':
            print_help_and_exit(stdout, 0);
            break;
        case '?':
            log_print(ERR, "unknown option: %c", optopt);
            break;
        case ':':
            log_print(ERR, "missing arg for %c", optopt);
            break;
        default:
            log_print(ERR, "error while parsing command line options");
            break;
        }
    }
    argc = argc - optind;
    argv = &argv[optind];

    if (argc > 0) {
        log_print(ERR, "extra arguments on the command line");
        return 1;
    }

    const char* sql = "VACUUM";

    char* errmsg;
    if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        log_print(ERR, "sqlite error: %s", errmsg);
        return 1;
    }

    return 0;
}

