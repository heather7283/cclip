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

#include <sqlite3.h>

#include "../utils.h"
#include "collections/string.h"
#include "getopt.h"
#include "log.h"

static void print_help(void) {
    static const char help[] =
        "Usage:\n"
        "    cclip get ID [FIELDS]\n"
        "\n"
        "Command line options:\n"
        "    ID      Entry id to get (- to read from stdin)\n"
        "    FIELDS  Comma-separated list of rows to print instead of entry data\n"
    ;

    fputs(help, stdout);
}

int action_get(int argc, char** argv, struct sqlite3* db) {
    int retcode = 0;

    struct sqlite3_stmt* stmt = NULL;

    int opt;
    optreset = 1;
    optind = 0;
    while ((opt = getopt(argc, argv, ":h")) != -1) {
        switch (opt) {
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
    char* fields_str;
    if (argc < 1) {
        log_print(ERR, "not enough arguments");
        retcode = 1;
        goto out;
    } else if (argc == 1) {
        id_str = argv[0];
        fields_str = NULL;
    } else if (argc == 2) {
        id_str = argv[0];
        fields_str = argv[1];
    } else {
        log_print(ERR, "extra arguments on the command line");
        retcode = 1;
        goto out;
    }

    int64_t id;
    if (!get_id(id_str, &id)) {
        retcode = 1;
        goto out;
    }

    if (fields_str == NULL) {
        const char* sql = "SELECT data FROM history WHERE id = ?";

        if (sqlite3_prepare(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            log_print(ERR, "failed to prepare statement: %s", sqlite3_errmsg(db));
            retcode = 1;
            goto out;
        }

        sqlite3_bind_int64(stmt, 1, id);

        int ret = sqlite3_step(stmt);
        if (ret == SQLITE_ROW) {
            struct iovec iov = {
                .iov_base = (void*)sqlite3_column_blob(stmt, 0),
                .iov_len = sqlite3_column_bytes(stmt, 0),
            };
            writev_full(1, &iov, 1);
        } else if (ret == SQLITE_DONE) {
            log_print(ERR, "no entry found with id %li", id);
            retcode = 1;
            goto out;
        } else {
            log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
            retcode = 1;
            goto out;
        }
    } else {
        enum select_fields fields[SELECT_FIELDS_COUNT];
        int nfields = build_field_list(fields_str, fields);
        if (nfields < 1) {
            retcode = 1;
            goto out;
        }

        struct string sql = {0};
        string_reserve(&sql, 512);

        string_append(&sql, "SELECT ");

        bool has_field_tags = false;
        for (int i = 0; i < nfields; i++) {
            switch (fields[i]) {
            case FIELD_ID:
                string_append(&sql, " h.id,");
                break;
            case FIELD_PREVIEW:
                string_append(&sql, " h.preview,");
                break;
            case FIELD_MIME_TYPE:
                string_append(&sql, " h.mime_type,");
                break;
            case FIELD_DATA_SIZE:
                string_append(&sql, " h.data_size,");
                break;
            case FIELD_TIMESTAMP:
                string_append(&sql, " h.timestamp,");
                break;
            case FIELD_TAGS:
                has_field_tags = true;
                string_append(&sql, " GROUP_CONCAT(t.name, ',') AS tags,");
                break;
            default:
                log_print(ERR, "invalid field enum value: %d (BUG)", fields[i]);
                retcode = 1;
                goto out;
            }
        }
        sql.str[sql.len - 1] = ' ';

        string_append(&sql, " FROM history AS h ");

        if (has_field_tags) {
            string_append(&sql, " LEFT JOIN history_tags AS ht ON h.id = ht.entry_id ");
            string_append(&sql, " LEFT JOIN tags AS t ON ht.tag_id = t.id ");
        }

        string_append(&sql, " WHERE h.id = ? ");

        if (has_field_tags) {
            string_append(&sql, " GROUP BY h.id ");
        }

        int ret;

        ret = sqlite3_prepare_v2(db, sql.str, sql.len, &stmt, NULL);
        if (ret != SQLITE_OK) {
            log_print(ERR, "failed to prepare sql statement");
            log_print(ERR, "source: %s", sql.str);
            log_print(ERR, "reason: %s", sqlite3_errmsg(db));
            retcode = 1;
            goto out;
        }

        sqlite3_bind_int64(stmt, 1, id);

        ret = sqlite3_step(stmt);
        if (ret == SQLITE_ROW) {
            const int ncols = sqlite3_column_count(stmt);
            struct iovec* iov = malloc(sizeof(*iov) * (ncols * 2));
            for (int i = 0; i < ncols; i++) {
                iov[i * 2] = (struct iovec){
                    .iov_base = (void *)sqlite3_column_blob(stmt, i),
                    .iov_len = sqlite3_column_bytes(stmt, i),
                };
                iov[(i * 2) + 1] = (struct iovec){
                    .iov_base = (i < ncols - 1) ? "\t" : "\n",
                    .iov_len = 1,
                };
            }

            writev_full(1, iov, ncols * 2);
        } else if (ret == SQLITE_DONE) {
            log_print(ERR, "no entry found with id %li", id);
            retcode = 1;
            goto out;
        } else {
            log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
            retcode = 1;
            goto out;
        }
    }

out:
    sqlite3_finalize(stmt);
    return retcode;
}

