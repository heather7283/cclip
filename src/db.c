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
#include <sys/wait.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <libgen.h>
#include <errno.h>
#include <sqlite3.h>

#include "db.h"
#include "common.h"
#include "xmalloc.h"

struct sqlite3* db = NULL;

const char* get_default_db_path(void) {
    static char db_path[PATH_MAX];

    char* xdg_data_home = getenv("XDG_DATA_HOME");
    if (xdg_data_home != NULL) {
        snprintf(db_path, sizeof(db_path), "%s/%s", xdg_data_home, "cclip/db.sqlite3");
    } else {
        char* home = getenv("HOME");
        if (home == NULL) {
            die("both HOME and XDG_DATA_HOME are unset, unable to determine db file path\n");
        }
        snprintf(db_path, sizeof(db_path), "%s/.local/share/%s", home, "cclip/db.sqlite3");
    }

    return db_path;
}

void db_init(const char* const db_path, bool create_if_not_exists) {
    char* errmsg = NULL;
    int ret_code = 0;

    if (access(db_path, F_OK) == -1) {
        if (!create_if_not_exists) {
            die("database file %s does not exist\n", db_path);
        } else {
            info("database file %s does not exist, "
                 "attempting to create\n", db_path);
        }


        char* db_path_dup = xstrdup(db_path);
        char* db_dir = dirname(db_path_dup);

        char* mkdir_cmd[] = {"mkdir", "-p", db_dir, NULL};
        pid_t child_pid = fork();
        if (child_pid == -1) {
            warn("forking child failed: %s\n", strerror(errno));
        } else if (child_pid == 0) {
            /* child */
            execvp(mkdir_cmd[0], mkdir_cmd);
            warn("execing into mkdir -p failed: %s\n", strerror(errno));
            exit(1);
        } else {
            /* parent */
            if (waitpid(child_pid, NULL, 0) == -1) {
                warn("failed to wait for child: %s\n", strerror(errno));
            };
        }

        free(db_path_dup);

        FILE* db_file = fopen(db_path, "w+");
        if (db_file == NULL) {
            die("unable to create database file: %s\n", strerror(errno));
        }
        fclose(db_file);
    }

    ret_code = sqlite3_open(db_path, &db);
    if (ret_code != SQLITE_OK) {
        die("sqlite error: %s\n", sqlite3_errstr(ret_code));
    }

    /* enable WAL https://sqlite.org/wal.html */
    ret_code = sqlite3_exec(db, "PRAGMA journal_mode=WAL", NULL, NULL, &errmsg);
    if (ret_code != SQLITE_OK) {
        die("sqlite error: %s\n", errmsg);
    }
    ret_code = sqlite3_exec(db, "PRAGMA synchronous=NORMAL", NULL, NULL, &errmsg);
    if (ret_code != SQLITE_OK) {
        die("sqlite error: %s\n", errmsg);
    }

    const char* db_create_expr =
        "CREATE TABLE IF NOT EXISTS history ("
        "    data      BLOB    NOT NULL UNIQUE,"
        "    data_size INTEGER NOT NULL,"
        "    preview   TEXT    NOT NULL,"
        "    mime_type TEXT    NOT NULL,"
        "    timestamp INTEGER NOT NULL"
        ")";
    ret_code = sqlite3_exec(db, db_create_expr, NULL, NULL, &errmsg);
    if (ret_code != SQLITE_OK) {
        die("sqlite error: %s\n", errmsg);
    }

    const char* db_create_data_index_expr =
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_history_data ON history(data)";
    ret_code = sqlite3_exec(db, db_create_data_index_expr, NULL, NULL, &errmsg);
    if (ret_code != SQLITE_OK) {
        die("sqlite error: %s\n", errmsg);
    }

    const char* db_create_timestamp_index_expr =
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_history_timestamp ON history(timestamp)";
    ret_code = sqlite3_exec(db, db_create_timestamp_index_expr, NULL, NULL, &errmsg);
    if (ret_code != SQLITE_OK) {
        die("sqlite error: %s\n", errmsg);
    }
}

int insert_db_entry(const struct db_entry* const entry, int max_entries_count) {
    sqlite3_stmt* stmt = NULL;
    int retcode = 0;
    char* errmsg = NULL;

    /* something something atomic */
    retcode = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &errmsg);
    if (retcode != SQLITE_OK) {
        critical("sqlite error: %s\n", errmsg);
        goto rollback;
    }

    if (max_entries_count > 0) {
        int entries_count;
        /* find out how many entries are currently in db */
        const char* count_query =
            "SELECT COUNT(*) FROM history";
        retcode = sqlite3_prepare_v2(db, count_query, -1, &stmt, NULL);
        if (retcode != SQLITE_OK) {
            critical("sqlite error: %s\n", sqlite3_errmsg(db));
            goto rollback;
        }
        retcode = sqlite3_step(stmt);
        if (retcode == SQLITE_ROW) {
            entries_count = sqlite3_column_int(stmt, 0);
        } else {
            critical("sqlite error: %s\n", sqlite3_errstr(retcode));
            goto rollback;
        }
        sqlite3_finalize(stmt);

        /* delete oldest */
        if (entries_count > max_entries_count) {
            debug("deleting oldest entry\n");
            const char* delete_statement =
                "DELETE FROM history WHERE timestamp=(SELECT MIN(timestamp) FROM history)";
            retcode = sqlite3_exec(db, delete_statement, NULL, NULL, &errmsg);
            if (retcode != SQLITE_OK) {
                critical("sqlite error: %s\n", errmsg);
                goto rollback;
            }
        }
    }

    /* prepare the SQL statement */
    const char* insert_statement =
        "INSERT OR REPLACE INTO history "
        "(data, data_size, preview, mime_type, timestamp) "
        "VALUES (?, ?, ?, ?, ?)";
    retcode = sqlite3_prepare_v2(db, insert_statement, -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        die("sqlite error: %s\n", sqlite3_errmsg(db));
    }

    /* bind parameters */
    sqlite3_bind_blob(stmt, 1, entry->data, entry->data_size, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, entry->data_size);
    sqlite3_bind_text(stmt, 3, entry->preview, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, entry->mime_type, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, entry->timestamp);

    /* execute the statement */
    retcode = sqlite3_step(stmt);
    if (retcode != SQLITE_DONE) {
        critical("sqlite error: %s\n", sqlite3_errmsg(db));
        goto rollback;
    } else {
        debug("record inserted successfully\n");
    }
    sqlite3_finalize(stmt);

    retcode = sqlite3_exec(db, "COMMIT", NULL, NULL, &errmsg);
    if (retcode != SQLITE_OK) {
        critical("sqlite error: %s\n", sqlite3_errmsg(db));
        goto rollback;
    }
    return 0;
rollback:
    retcode = sqlite3_exec(db, "ROLLBACK", NULL, NULL, &errmsg);
    return 1;
}
