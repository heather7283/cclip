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
#include <string.h>

#include "action_delete.h"
#include "cclip.h"
#include "utils.h"
#include "getopt.h"

static void print_help_and_exit(FILE *stream, int rc) {
    const char *help =
        "Usage:\n"
        "    cclip delete [-s] ID\n"
        "\n"
        "Command line options:\n"
        "    -s  Enable secure delete pragma\n"
        "    ID  Entry id to delete (- to read from stdin)\n"
    ;

    fprintf(stream, "%s", help);
    exit(rc);
}

int action_delete(int argc, char** argv) {
    bool secure_delete = false;

    int opt;
    optreset = 1;
    optind = 0;
    while ((opt = getopt(argc, argv, ":hs")) != -1) {
        switch (opt) {
        case 's':
            secure_delete = true;
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

    char* id_str;
    if (argc < 1) {
        fprintf(stderr, "not enough arguments\n");
        return 1;
    } else if (argc == 1) {
        id_str = argv[0];
    }  else {
        fprintf(stderr, "extra arguments on the command line\n");
        return 1;
    }

    int64_t id;
    if (!get_id((strcmp(id_str, "-") == 0) ? NULL : id_str, &id)) {
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

    const char* sql = "DELETE FROM history WHERE rowid = ?";

    sqlite3_stmt* stmt;
    int ret = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "sqlite error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_bind_int64(stmt, 1, id);

    ret = sqlite3_step(stmt);
    if (ret == SQLITE_DONE) {
        if (sqlite3_changes(db) == 0) {
            fprintf(stderr, "table was not modified, does id %li exist?\n", id);
            return 1;
        }
    } else {
        fprintf(stderr, "sqlite error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    return 0;
}

