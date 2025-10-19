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
#include <string.h>

#include <sqlite3.h>

#include "../utils.h"
#include "getopt.h"
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

int action_delete(int argc, char** argv, struct sqlite3* db) {
    int retcode = 0;
    bool secure_delete = false;

    sqlite3_stmt* stmt = NULL;

    int opt;
    optreset = 1;
    optind = 0;
    while ((opt = getopt(argc, argv, ":hs")) != -1) {
        switch (opt) {
        case 's':
            secure_delete = true;
            break;
        case 'h':
            print_help();
            goto out;
        case '?':
            log_print(ERR, "unknown option: %c", optopt);
            retcode = 1;
            goto out;
        case ':':
            log_print(ERR, "missing arg for %c", optopt);
            retcode = 1;
            goto out;
        default:
            log_print(ERR, "error while parsing command line options");
            retcode = 1;
            goto out;
        }
    }
    argc = argc - optind;
    argv = &argv[optind];

    char* id_str;
    if (argc < 1) {
        log_print(ERR, "not enough arguments");
        retcode = 1;
        goto out;
    } else if (argc == 1) {
        id_str = argv[0];
    }  else {
        log_print(ERR, "extra arguments on the command line");
        retcode = 1;
        goto out;
    }

    int64_t entry_id;
    if (!get_id(id_str, &entry_id)) {
        retcode = 1;
        goto out;
    }

    if (secure_delete && !db_set_secure_delete(db, true)) {
        retcode = 1;
        goto out;
    }

    if (!db_prepare_stmt(db, "DELETE FROM history WHERE id = @entry_id", &stmt)) {
        retcode = 1;
        goto out;
    }

    STMT_BIND(stmt, int64, "@entry_id", entry_id);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        if (sqlite3_changes(db) == 0) {
            log_print(ERR, "table was not modified, does id %li exist?", entry_id);
            retcode = 1;
            goto out;
        }
    } else {
        log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
        retcode = 1;
        goto out;
    }

out:
    sqlite3_finalize(stmt);
    return retcode;
}

