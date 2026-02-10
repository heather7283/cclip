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

#include <sqlite3.h>

#include "actions.h"
#include "../utils.h"
#include "db.h"
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

void action_tag(int argc, char** argv, struct sqlite3* db) {
    int retcode = 0;
    struct sqlite3_stmt* stmt = NULL;

    bool delete_tag = false;

    RESET_GETOPT();
    int opt;
    while ((opt = getopt(argc, argv, ":hd")) != -1) {
        switch (opt) {
        case 'd':
            delete_tag = true;
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

    char* id_str = NULL;
    char* tag_str = NULL;
    if (argc < 2) {
        if (!delete_tag) {
            log_print(ERR, "not enough arguments");
            OUT(1);
        }
        id_str = argv[0];
    } else if (argc == 2) {
        id_str = argv[0];
        tag_str = argv[1];
    } else {
        log_print(ERR, "extra arguments on the command line");
        OUT(1);
    }

    if (tag_str != NULL && !delete_tag && !is_tag_valid(tag_str)) {
        log_print(ERR, "invalid tag");
        OUT(1);
    }

    int64_t entry_id;
    if (!get_id(id_str, &entry_id)) {
        OUT(1);
    }

    int ret;
    if (delete_tag) {
        const char* sql;
        if (tag_str != NULL) {
            sql = TOSTRING(
                DELETE FROM history_tags WHERE entry_id = @entry_id AND tag_id = (
                    SELECT id FROM tags WHERE name = @tag_name
                );
            );
        } else {
            sql = TOSTRING(
                DELETE FROM history_tags WHERE entry_id = ?;
            );
        }

        if (!db_prepare_stmt(db, sql, &stmt)) {
            OUT(1);
        }

        STMT_BIND(stmt, int64, "@entry_id", entry_id);
        if (tag_str != NULL) {
            STMT_BIND(stmt, text, "@tag_name", tag_str, -1, SQLITE_STATIC);
        }

        ret = sqlite3_step(stmt);
        if (ret != SQLITE_DONE) {
            log_print(ERR, "failed to delete tags from entry: %s", sqlite3_errmsg(db));
            OUT(1);
        }

        if (sqlite3_changes(db) < 1) {
            log_print(ERR, "table was not modified, either tag or entry do not exist");
            OUT(1);
        }
    } else /* if (!delete_tag) */ {
        const char* sql_insert_into_tags = TOSTRING(
            INSERT OR IGNORE INTO tags ( name ) VALUES ( @tag_name );
        );

        if (!db_prepare_stmt(db, sql_insert_into_tags, &stmt)) {
            OUT(1);
        }

        STMT_BIND(stmt, text, "@tag_name", tag_str, -1, SQLITE_STATIC);

        ret = sqlite3_step(stmt);
        if (ret != SQLITE_DONE) {
            log_print(ERR, "failed to add tag into tags table: %s", sqlite3_errmsg(db));
            OUT(1);
        }

        sqlite3_finalize(stmt);

        const char* sql_insert_into_history_tags = TOSTRING(
            INSERT INTO history_tags ( tag_id, entry_id ) VALUES (
                ( SELECT id FROM tags WHERE name = @tag_name ), @entry_id
            );
        );

        if (!db_prepare_stmt(db, sql_insert_into_history_tags, &stmt)) {
            OUT(1);
        }

        STMT_BIND(stmt, text, "@tag_name", tag_str, -1, SQLITE_STATIC);
        STMT_BIND(stmt, int64, "@entry_id", entry_id);

        ret = sqlite3_step(stmt);
        if (ret != SQLITE_DONE) {
            log_print(ERR, "failed to add tag to entry: %s (duplicate tag?)", sqlite3_errmsg(db));
            OUT(1);
        }
    }

out:
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    exit(retcode);
}

