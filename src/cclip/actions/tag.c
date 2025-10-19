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
#include "macros.h"
#include "log.h"

static void print_help(void) {
    static const char help[] =
        "Usage:\n"
        "    cclip tag ID TAG\n"
        "    cclip tag -d ID [TAG]\n"
        "\n"
        "Command line options:\n"
        "    -d      Delete TAG from entry instead of adding.\n"
        "            If TAG is missing, delete all tags from entry.\n"
        "    ID      Entry id to tag or untag (- to read from stdin)\n"
        "    TAG     Tag to add or (with -d) delete\n"
    ;

    fputs(help, stdout);
}

int action_tag(int argc, char** argv, struct sqlite3* db) {
    int retcode = 0;
    bool delete_tag = false;

    struct sqlite3_stmt* stmt = NULL;

    int opt;
    optreset = 1;
    optind = 0;
    while ((opt = getopt(argc, argv, ":hd")) != -1) {
        switch (opt) {
        case 'd':
            delete_tag = true;
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

    char* id_str = NULL;
    char* tag_str = NULL;
    if (argc < 2) {
        if (!delete_tag) {
            log_print(ERR, "not enough arguments");
            retcode = 1;
            goto out;
        }
        id_str = argv[0];
    } else if (argc == 2) {
        id_str = argv[0];
        tag_str = argv[1];
    } else {
        log_print(ERR, "extra arguments on the command line");
        retcode = 1;
        goto out;
    }

    if (tag_str != NULL && !is_tag_valid(tag_str)) {
        log_print(ERR, "invalid tag");
        retcode = 1;
        goto out;
    }

    int64_t id;
    if (!get_id(id_str, &id)) {
        retcode = 1;
        goto out;
    }

    if (delete_tag) {
        const char* sql;
        if (tag_str != NULL) {
            sql = TOSTRING(
                DELETE FROM history_tags WHERE entry_id = ? AND tag_id = (
                    SELECT id FROM tags WHERE name = ?
                );
            );
        } else {
            sql = TOSTRING(
                DELETE FROM history_tags WHERE entry_id = ?;
            );
        }

        int ret = sqlite3_prepare(db, sql, -1, &stmt, NULL);
        if (ret != SQLITE_OK) {
            log_print(ERR, "failed to prepare sql statement");
            log_print(ERR, "source: %s", sql);
            log_print(ERR, "reason: %s", sqlite3_errmsg(db));
            retcode = 1;
            goto out;
        }

        sqlite3_bind_int64(stmt, 1, id);
        if (tag_str != NULL) {
            sqlite3_bind_text(stmt, 2, tag_str, -1, SQLITE_STATIC);
        }

        ret = sqlite3_step(stmt);
        if (ret != SQLITE_DONE) {
            log_print(ERR, "failed to delete tags from entry: %s", sqlite3_errmsg(db));
            retcode = 1;
            goto out;
        }

        if (sqlite3_changes(db) < 1) {
            log_print(ERR, "table was not modified, either tag or entry do not exist");
            retcode = 1;
            goto out;
        }
    } else /* if (!delete_tag) */ {
        const char* sql_insert_into_tags = TOSTRING(
            INSERT OR IGNORE INTO tags ( name ) VALUES ( ? );
        );

        int ret = sqlite3_prepare(db, sql_insert_into_tags, -1, &stmt, NULL);
        if (ret != SQLITE_OK) {
            log_print(ERR, "failed to prepare sql statement");
            log_print(ERR, "source: %s", sql_insert_into_tags);
            log_print(ERR, "reason: %s", sqlite3_errmsg(db));
            retcode = 1;
            goto out;
        }

        sqlite3_bind_text(stmt, 1, tag_str, -1, SQLITE_STATIC);

        ret = sqlite3_step(stmt);
        if (ret != SQLITE_DONE) {
            log_print(ERR, "failed to add tag into tags table: %s", sqlite3_errmsg(db));
            retcode = 1;
            goto out;
        }

        sqlite3_finalize(stmt);

        const char* sql_insert_into_history_tags = TOSTRING(
            INSERT INTO history_tags ( tag_id, entry_id ) VALUES (
                ( SELECT id FROM tags WHERE name = ? ),
                ?
            );
        );

        ret = sqlite3_prepare(db, sql_insert_into_history_tags, -1, &stmt, NULL);
        if (ret != SQLITE_OK) {
            log_print(ERR, "failed to prepare sql statement");
            log_print(ERR, "source: %s", sql_insert_into_tags);
            log_print(ERR, "reason: %s", sqlite3_errmsg(db));
            retcode = 1;
            goto out;
        }

        sqlite3_bind_text(stmt, 1, tag_str, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, id);

        ret = sqlite3_step(stmt);
        if (ret != SQLITE_DONE) {
            log_print(ERR, "failed to add tag to entry: %s (duplicate tag?)", sqlite3_errmsg(db));
            retcode = 1;
            goto out;
        }
    }

out:
    sqlite3_finalize(stmt);
    return retcode;
}

