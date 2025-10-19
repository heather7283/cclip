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

#include <sys/uio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <sqlite3.h>

#include "../utils.h"
#include "collections/string.h"
#include "db.h"
#include "macros.h"
#include "getopt.h"
#include "log.h"

static void print_help(void) {
    static const char help[] =
        "Usage:\n"
        "    cclip list [-t] [-T TAG] [FIELDS]\n"
        "\n"
        "Command line options:\n"
        "    -t      Only list entries with non-empty tag\n"
        "    -T TAG  Only list entries that have matching TAG (implies -t)\n"
        "    FIELDS  Comma-separated list of fields to print\n"
    ;

    fputs(help, stdout);
}

int action_list(int argc, char** argv, struct sqlite3* db) {
    int retcode = 0;
    bool print_only_tagged = false;
    const char* tag = NULL;

    struct sqlite3_stmt* stmt = NULL;

    int opt;
    optreset = 1;
    optind = 0;
    while ((opt = getopt(argc, argv, ":T:th")) != -1) {
        switch (opt) {
        case 'T':
            tag = optarg;
            print_only_tagged = true;
            break;
        case 't':
            print_only_tagged = true;
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

    enum select_fields fields[SELECT_FIELDS_COUNT];
    int nfields = 0;
    if (argc < 1) {
        static char default_fields[] = "rowid,mime_type,preview";
        nfields = build_field_list(default_fields, fields);
    } else if (argc == 1) {
        nfields = build_field_list(argv[0], fields);
    } else {
        log_print(ERR, "extra arguments on the command line");
        retcode = 1;
        goto out;
    }

    if (nfields < 1) {
        retcode = 1;
        goto out;
    }

    struct string sql = {0};
    string_reserve(&sql, 512);

    string_append(&sql, "SELECT");

    bool print_tags = false;
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
            print_tags = true;
            string_append(&sql, " group_concat(t.name, ',') AS tags,");
            break;
        default:
            log_print(ERR, "invalid field enum value: %d (BUG)", fields[i]);
            retcode = 1;
            goto out;
        }
    }
    sql.str[sql.len - 1] = ' ';

    string_append(&sql, " FROM history AS h ");

    if (print_tags || print_only_tagged) {
        string_append(&sql, print_only_tagged ? " INNER " : " LEFT ");
        string_append(&sql, " JOIN history_tags AS ht ON h.id = ht.entry_id ");

        string_append(&sql, print_only_tagged ? " INNER " : " LEFT ");
        string_append(&sql, " JOIN tags AS t ON ht.tag_id = t.id ");

        if (tag != NULL) {
            /* FIXME: probably very inefficient idk */
            static const char stupid_filter[] = TOSTRING(
                WHERE h.id IN (
                    SELECT ht2.entry_id
                    FROM history_tags AS ht2
                    INNER JOIN tags AS t2 ON ht2.tag_id = t2.id
                    WHERE t2.name = ?
                )
            );
            string_appendn(&sql, stupid_filter, strlen(stupid_filter));
        }

        if (print_tags) {
            string_append(&sql, " GROUP BY h.id ");
        }
    }

    string_append(&sql, " ORDER BY h.timestamp DESC");

    int rc;

    if (!db_prepare_stmt(db, sql.str, &stmt)) {
        retcode = 1;
        goto out;
    }

    if (tag != NULL) {
        sqlite3_bind_text(stmt, 1, tag, -1, SQLITE_STATIC);
    }

    /* field + tab + field + tab + field + newline */
    const int ncols = sqlite3_column_count(stmt);
    struct iovec* iov = malloc(sizeof(*iov) * (ncols * 2));
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
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

        if (!writev_full(1, iov, ncols * 2)) {
            log_print(ERR, "failed to write row: %s", strerror(errno));
            retcode = 1;
            goto out;
        }
    }
    if (rc != SQLITE_DONE) {
        log_print(ERR, "failed to list rows: %s", sqlite3_errmsg(db));
        retcode = 1;
        goto out;
    }

out:
    sqlite3_finalize(stmt);
    return retcode;
}

