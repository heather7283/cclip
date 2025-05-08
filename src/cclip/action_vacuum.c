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

#include "action_vacuum.h"
#include "cclip.h"
#include "getopt.h"

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

int action_vacuum(int argc, char** argv) {
    int opt;
    optreset = 1;
    optind = 0;
    while ((opt = getopt(argc, argv, ":h")) != -1) {
        switch (opt) {
        case 'h':
            print_help_and_exit(stdout, 0);
            break;
        case '?':
            fprintf(stderr, "unknown option: %c\n\n", optopt);
            print_help_and_exit(stderr, 1);
            break;
        case ':':
            fprintf(stderr, "missing arg for %c\n\n", optopt);
            print_help_and_exit(stderr, 1);
            break;
        default:
            fprintf(stderr, "error while parsing command line options\n\n");
            print_help_and_exit(stderr, 1);
            break;
        }
    }
    argc = argc - optind;
    argv = &argv[optind];

    if (argc > 0) {
        fprintf(stderr, "extra arguments on the command line\n");
        return 1;
    }

    const char* sql = "VACUUM";

    char* errmsg;
    if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        fprintf(stderr, "sqlite error: %s\n", errmsg);
        return 1;
    }

    return 0;
}

