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

#include <sys/uio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include <sqlite3.h>

#include "actions.h"
#include "../utils.h"
#include "db.h"
#include "macros.h"
#include "getopt.h"
#include "log.h"

static void print_help(void) {
    static const char help[] =
        "Usage:\n"
        "    cclip tags [list]\n"
        "    cclip tags delete TAG\n"
        "    cclip tags wipe\n"
    ;

    fputs(help, stdout);
}

static int do_list(struct sqlite3* db) {
    int retcode = 0;
    struct sqlite3_stmt* stmt = NULL;

    const char* sql = TOSTRING(
        SELECT name FROM tags;
    );

    if (!db_prepare_stmt(db, sql, &stmt)) {
        OUT(1);
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
            OUT(1);
        }
    }

    if (rc != SQLITE_DONE) {
        log_print(ERR, "failed to list tags: %s", sqlite3_errmsg(db));
        OUT(1);
    }

out:
    sqlite3_finalize(stmt);
    return retcode;
}

static int do_delete(struct sqlite3* db, const char* name) {
    int retcode = 0;
    struct sqlite3_stmt* stmt = NULL;

    const char* sql = TOSTRING(
        WITH tag_ids AS (
            SELECT id FROM tags WHERE name = @name
        ) DELETE FROM history_tags WHERE tag_id IN tag_ids;
    );

    if (!db_prepare_stmt(db, sql, &stmt)) {
        OUT(1);
    }

    STMT_BIND(stmt, text, "@name", name, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        log_print(ERR, "failed to delete tag(s): %s", sqlite3_errmsg(db));
        OUT(1);
    }

    if (sqlite3_changes(db) == 0) {
        log_print(WARN, "no tags were deleted");
    }

out:
    sqlite3_finalize(stmt);
    return retcode;
}

static int do_wipe(struct sqlite3* db) {
    const char* sql = TOSTRING(
        DELETE FROM history_tags;
    );

    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        log_print(ERR, "failed to delete tags: %s", sqlite3_errmsg(db));
        return 1;
    }

    return 0;
}

void action_tags(int argc, char** argv, struct sqlite3* db) {
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

    if (argc < 1 || STREQ(argv[0], "list")) {
        if (argc > 1) {
            log_print(ERR, "extra arguments on the command line");
            OUT(1);
        }

        retcode = do_list(db);
    } else if (STREQ(argv[0], "delete")) {
        if (argc < 2) {
            log_print(ERR, "tag name to delete is not specified");
            OUT(1);
        } else if (argc > 2) {
            log_print(ERR, "extra arguments on the command line");
            OUT(1);
        }

        retcode = do_delete(db, argv[1]);
    } else if (STREQ(argv[0], "wipe")) {
        if (argc > 1) {
            log_print(ERR, "extra arguments on the command line");
            OUT(1);
        }

        retcode = do_wipe(db);
    } else {
        log_print(ERR, "invalid argument to tags: %s", argv[0]);
        OUT(1);
    }

out:
    sqlite3_close(db);
    exit(retcode);
}

