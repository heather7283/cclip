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
#include <getopt.h>
#include <string.h>
#include <stdbool.h>

#include "action_wipe.h"
#include "cclip.h"

static void print_help_and_exit(FILE *stream, int rc) {
    const char *help =
        "Usage:\n"
        "    cclip wipe [-ts]\n"
        "\n"
        "Command line options:\n"
        "    -t  Do not preserve tagged entries\n"
        "    -s  Enable secure delete pragma\n"
    ;

    fprintf(stream, "%s", help);
    exit(rc);
}

int action_wipe(int argc, char** argv) {
    bool preserve_tagged = true;
    bool secure_delete = false;

    int opt;
    optind = 0;
    while ((opt = getopt(argc, argv, ":hts")) != -1) {
        switch (opt) {
        case 's':
            secure_delete = true;
            break;
        case 't':
            preserve_tagged = false;
            break;
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

    if (secure_delete) {
        char* errmsg;
        int ret = sqlite3_exec(db, "PRAGMA secure_delete = 1", NULL, NULL, &errmsg);
        if (ret != SQLITE_OK) {
            fprintf(stderr, "sqlite error: %s\n", errmsg);
            return 1;
        }
    }

    const char* sql;
    if (preserve_tagged) {
        sql = "DELETE FROM history WHERE tag = ''";
    } else {
        sql = "DELETE FROM history";
    }

    char* errmsg;
    if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        fprintf(stderr, "sqlite error: %s\n", errmsg);
        return 1;
    }

    return 0;
}

