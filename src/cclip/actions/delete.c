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
#include "../utils.h"
#include "db.h"
#include "log.h"

static void print_help(void) {
    static const char help[] =
        "Usage:\n"
        "    cclip delete [-s] ID\n"
        "\n"
        "Command line options:\n"
        "    -s  Enable secure delete pragma\n"
        "    ID  Entry id to delete (- to read from stdin)\n"
    ;

    fputs(help, stdout);
}

void action_delete(int argc, char** argv, struct sqlite3* db) {
    int retcode = 0;
    sqlite3_stmt* stmt = NULL;

    bool secure_delete = false;

    RESET_GETOPT();
    int opt;
    while ((opt = getopt(argc, argv, ":hs")) != -1) {
        switch (opt) {
        case 's':
            secure_delete = true;
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

    char* id_str;
    if (argc < 1) {
        log_print(ERR, "not enough arguments");
        OUT(1);
    } else if (argc == 1) {
        id_str = argv[0];
    }  else {
        log_print(ERR, "extra arguments on the command line");
        OUT(1);
    }

    int64_t entry_id;
    if (!get_id(id_str, &entry_id)) {
        OUT(1);
    }

    if (secure_delete && !db_set_secure_delete(db, true)) {
        OUT(1);
    }

    if (!db_prepare_stmt(db, "DELETE FROM history WHERE id = @entry_id", &stmt)) {
        OUT(1);
    }

    STMT_BIND(stmt, int64, "@entry_id", entry_id);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        if (sqlite3_changes(db) == 0) {
            log_print(ERR, "table was not modified, does id %li exist?", entry_id);
            OUT(1);
        }
    } else {
        log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
        OUT(1);
    }

out:
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    exit(retcode);
}

