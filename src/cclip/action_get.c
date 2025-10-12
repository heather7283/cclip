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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "action_get.h"
#include "cclip.h"
#include "utils.h"
#include "getopt.h"
#include "log.h"

static void print_help_and_exit(FILE *stream, int rc) {
    const char *help =
        "Usage:\n"
        "    cclip get ID [FIELDS]\n"
        "\n"
        "Command line options:\n"
        "    ID      Entry id to get (- to read from stdin)\n"
        "    FIELDS  Comma-separated list of rows to print instead of entry data\n"
    ;

    fprintf(stream, "%s", help);
    exit(rc);
}

static int print_row(void* data, int argc, char** argv, char** column_names) {
    for (int i = 0; i < argc - 1; i++) {
        printf("%s\t", argv[i] ? argv[i] : "");
    }
    printf("%s\n", argv[argc - 1] ? argv[argc - 1] : "");
    return 0;
}

int action_get(int argc, char** argv) {
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

    char* id_str;
    char* fields_str;
    if (argc < 1) {
        log_print(ERR, "not enough arguments");
        return 1;
    } else if (argc == 1) {
        id_str = argv[0];
        fields_str = NULL;
    } else if (argc == 2) {
        id_str = argv[0];
        fields_str = argv[1];
    } else {
        log_print(ERR, "extra arguments on the command line");
        return 1;
    }

    int64_t id;
    if (!get_id(id_str, &id)) {
        return 1;
    }

    if (fields_str == NULL) {
        const char* sql = "SELECT data FROM history WHERE rowid = ?";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            log_print(ERR, "failed to prepare statement: %s", sqlite3_errmsg(db));
            return 1;
        }

        sqlite3_bind_int(stmt, 1, id);

        int ret = sqlite3_step(stmt);
        if (ret == SQLITE_ROW) {
            int data_size = sqlite3_column_bytes(stmt, 0);
            const void* data = sqlite3_column_blob(stmt, 0);
            fwrite(data, 1, data_size, stdout);
        } else if (ret == SQLITE_DONE) {
            log_print(ERR, "no entry found with id %li", id);
            return 1;
        } else {
            log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
            return 1;
        }

        sqlite3_finalize(stmt);
    } else {
        const char* fields = build_field_list(fields_str);
        if (fields == NULL) {
            return 1;
        }

        const char sql1[] = "SELECT ";
        const char sql2[] = " FROM history";
        const char sql3[] = " WHERE rowid = ";
        char sql[MAX_FIELD_LIST_SIZE + sizeof(sql1) + sizeof(sql2) + sizeof(sql3)];
        snprintf(sql, sizeof(sql), "%s%s%s%s%li", sql1, fields, sql2, sql3, id);

        char* errmsg;
        if (sqlite3_exec(db, sql, print_row, NULL, &errmsg) != SQLITE_OK) {
            log_print(ERR, "sqlite error: %s", errmsg);
            return 1;
        }
    }

    return 0;
}

