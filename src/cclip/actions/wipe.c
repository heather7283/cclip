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
#include <stdbool.h>

#include <sqlite3.h>

#include "actions.h"
#include "db.h"
#include "log.h"

static void print_help(void) {
    static const char help[] =
        "Usage:\n"
        "    cclip wipe [-ts]\n"
        "\n"
        "Command line options:\n"
        "    -t  Do not preserve tagged entries\n"
        "    -s  Enable secure delete pragma\n"
    ;

    fputs(help, stdout);
}

void action_wipe(int argc, char** argv, struct sqlite3* db) {
    int retcode = 0;

    bool preserve_tagged = true;
    bool secure_delete = false;

    RESET_GETOPT();
    int opt;
    while ((opt = getopt(argc, argv, ":hts")) != -1) {
        switch (opt) {
        case 's':
            secure_delete = true;
            break;
        case 't':
            preserve_tagged = false;
            break;
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

    if (secure_delete && !db_set_secure_delete(db, true)) {
        OUT(1);
    }

    const char* sql;
    if (preserve_tagged) {
        sql = "DELETE FROM history WHERE id NOT IN ( SELECT entry_id FROM history_tags )";
    } else {
        sql = "DELETE FROM history";
    }

    char* errmsg;
    if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        log_print(ERR, "sqlite error: %s", errmsg);
        OUT(1);
    }

out:
    sqlite3_close(db);
    exit(retcode);
}

