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
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <sqlite3.h>

#include "../utils.h"
#include "db.h"
#include "getopt.h"
#include "log.h"

static void print_help(void) {
    static const char help[] =
        "Usage:\n"
        "    cclip tags\n"
        "\n"
        "Command line options:\n"
        "    cclip tags does not take command line options\n"
    ;

    fputs(help, stdout);
}

int action_tags(int argc, char** argv, struct sqlite3* db) {
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

    if (argc > 0) {
        log_print(ERR, "extra arguments on the command line");
        retcode = 1;
        goto out;
    }

    const char* sql = "SELECT name FROM tags";

    if (!db_prepare_stmt(db, sql, &stmt)) {
        retcode = 1;
        goto out;
    }

    int rc;
    struct iovec iov[2];
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        iov[0].iov_base = (void *)sqlite3_column_blob(stmt, 0);
        iov[0].iov_len = sqlite3_column_bytes(stmt, 0);

        iov[1].iov_base = "\n";
        iov[1].iov_len = 1;

        if (!writev_full(1, iov, 2)) {
            log_print(ERR, "failed to write tag name: %s", strerror(errno));
            retcode = 1;
            goto out;
        }
    }
    if (rc != SQLITE_DONE) {
        log_print(ERR, "failed to list tags: %s", sqlite3_errmsg(db));
        retcode = 1;
        goto out;
    }

out:
    sqlite3_finalize(stmt);
    return retcode;
}

