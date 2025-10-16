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

#include "action_list.h"
#include "cclip.h"
#include "utils.h"
#include "getopt.h"
#include "log.h"

static void print_help_and_exit(FILE *stream, int rc) {
    const char *help =
        "Usage:\n"
        "    cclip list [-t] [FIELDS]\n"
        "\n"
        "Command line options:\n"
        "    -t      Only list entries with non-empty tag\n"
        "    FIELDS  Comma-separated list of fields to print\n"
    ;

    fprintf(stream, "%s", help);
    exit(rc);
}

int action_list(int argc, char** argv) {
    bool only_tagged = false;

    int opt;
    optreset = 1;
    optind = 0;
    while ((opt = getopt(argc, argv, ":th")) != -1) {
        switch (opt) {
        case 't':
            only_tagged = true;
            break;
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

    const char* fields = NULL;
    if (argc < 1) {
        fields = "rowid,mime_type,preview";
    } else if (argc == 1) {
        fields = build_field_list(argv[0]);
        if (fields == NULL) {
            return 1;
        }
    } else {
        log_print(ERR, "extra arguments on the command line");
        return 1;
    }

    const char sql1[] = "SELECT ";
    const char sql2[] = " FROM history";
    const char sql3[] = " WHERE tag IS NOT NULL";
    const char sql4[] = " ORDER BY timestamp DESC";
    char sql[MAX_FIELD_LIST_SIZE + sizeof(sql1) + sizeof(sql2) + sizeof(sql3) + sizeof(sql4)];
    snprintf(sql, sizeof(sql), "%s%s%s%s%s", sql1, fields, sql2, only_tagged ? sql3 : "", sql4);

    int rc;
    struct sqlite3_stmt* stmt = NULL;

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to prepare sql statement:");
        log_print(ERR, "source: %s", sql);
        log_print(ERR, "reason: %s", sqlite3_errmsg(db));
        return 1;
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
        }
    }
    if (rc != SQLITE_DONE) {
        log_print(ERR, "failed to list rows: %s", sqlite3_errmsg(db));
        return 1;
    }

    return 0;
}

