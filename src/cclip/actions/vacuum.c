/*
 * This file is part of cclip, clipboard manager for wayland
 * Copyright (C) 2026  heather7283
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

#include <sqlite3.h>

#include "actions.h"
#include "log.h"

static void print_help(void) {
    static const char help[] =
        "Usage:\n"
        "    cclip vacuum\n"
        "\n"
        "Command line options:\n"
        "    cclip vacuum does not take command line options\n"
    ;

    fputs(help, stdout);
}

void action_vacuum(int argc, char** argv, struct sqlite3* db) {
    int retcode = 0;

    RESET_GETOPT();
    int opt;
    while ((opt = getopt(argc, argv, ":h")) != -1) {
        switch (opt) {
        case 'h':
            print_help();
            OUT(0);
        case '?':
            log_print(ERR, "unknown option: %c", optopt);
            OUT(1);
        case ':':
            log_print(ERR, "missing arg for %c", optopt);
            OUT(1);
        default:
            log_print(ERR, "error while parsing command line options");
            OUT(1);
        }
    }
    argc = argc - optind;
    argv = &argv[optind];

    if (argc > 0) {
        log_print(ERR, "extra arguments on the command line");
        OUT(1);
    }

    if (sqlite3_exec(db, "VACUUM", NULL, NULL, NULL) != SQLITE_OK) {
        log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
        OUT(1);
    }

    OUT(0);

out:
    sqlite3_close(db);
    exit(retcode);
}

