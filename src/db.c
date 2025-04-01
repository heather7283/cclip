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

enum {
    STMT_INSERT,
    STMT_DELETE_OLDEST,
    STMT_END,
};
static sqlite3_stmt* statements[STMT_END];

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

void db_init(const char* const db_path, bool create_if_not_exists, bool prepare_statements) {
    char* errmsg = NULL;
    int rc = 0;

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

    rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        die("sqlite error: %s\n", sqlite3_errstr(rc));
    }

    /* enable WAL https://sqlite.org/wal.html */
    rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        die("sqlite error: %s\n", errmsg);
    }
    rc = sqlite3_exec(db, "PRAGMA synchronous=NORMAL", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
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
    rc = sqlite3_exec(db, db_create_expr, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        die("sqlite error: %s\n", errmsg);
    }

    const char* db_create_data_index_expr =
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_history_data ON history(data)";
    rc = sqlite3_exec(db, db_create_data_index_expr, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        die("sqlite error: %s\n", errmsg);
    }

    const char* db_create_timestamp_index_expr =
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_history_timestamp ON history(timestamp)";
    rc = sqlite3_exec(db, db_create_timestamp_index_expr, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        die("sqlite error: %s\n", errmsg);
    }

    if (prepare_statements) {
        /* prepare statements in advance */
        const char* insert_stmt =
            "INSERT OR REPLACE INTO history "
            "(data, data_size, preview, mime_type, timestamp) "
            "VALUES (?, ?, ?, ?, ?)";
        rc = sqlite3_prepare_v2(db, insert_stmt, -1, &statements[STMT_INSERT], NULL);
        if (rc != SQLITE_OK) {
            die("sqlite error: %s\n", sqlite3_errmsg(db));
        }

        const char* delete_oldest_stmt =
            "DELETE FROM history WHERE rowid IN ("
            "   SELECT rowid FROM history "
            "   ORDER BY timestamp DESC "
            "   LIMIT -1 OFFSET ?"
            ")";
        rc = sqlite3_prepare_v2(db, delete_oldest_stmt, -1, &statements[STMT_DELETE_OLDEST], NULL);
        if (rc != SQLITE_OK) {
            die("sqlite error: %s\n", sqlite3_errmsg(db));
        }
    }
}

void db_cleanup(void) {
    for (int i = 0; i < STMT_END; i++) {
        sqlite3_finalize(statements[i]);
    }
    sqlite3_close_v2(db);
}

int insert_db_entry(const struct db_entry* const entry, int max_entries_count) {
    int rc = 0;

    /* bind parameters */
    sqlite3_bind_blob(statements[STMT_INSERT], 1, entry->data, entry->data_size, SQLITE_STATIC);
    sqlite3_bind_int64(statements[STMT_INSERT], 2, entry->data_size);
    sqlite3_bind_text(statements[STMT_INSERT], 3, entry->preview, -1, SQLITE_STATIC);
    sqlite3_bind_text(statements[STMT_INSERT], 4, entry->mime_type, -1, SQLITE_STATIC);
    sqlite3_bind_int64(statements[STMT_INSERT], 5, entry->timestamp);

    /* execute the statement */
    sqlite3_step(statements[STMT_INSERT]);

    sqlite3_clear_bindings(statements[STMT_INSERT]);
    rc = sqlite3_reset(statements[STMT_INSERT]);
    if (rc != SQLITE_OK) {
        critical("sqlite error: %s\n", sqlite3_errmsg(db));
        return -1;
    } else {
        debug("record inserted successfully\n");
    }

    if (max_entries_count > 0) {
        /* bind the limit */
        sqlite3_bind_int(statements[STMT_DELETE_OLDEST], 1, max_entries_count);

        sqlite3_step(statements[STMT_DELETE_OLDEST]);

        sqlite3_clear_bindings(statements[STMT_DELETE_OLDEST]);
        rc = sqlite3_reset(statements[STMT_DELETE_OLDEST]);
        if (rc != SQLITE_OK) {
            critical("sqlite error: %s\n", sqlite3_errmsg(db));
            return -1;
        }
    }

    return 0;
}

