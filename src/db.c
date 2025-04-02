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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sqlite3.h>

#include "db.h"
#include "common.h"

enum {
    STMT_INSERT,
    STMT_DELETE_OLDEST,
    STMT_BEGIN,
    STMT_COMMIT,
    STMT_ROLLBACK,
    STMT_END,
};
static sqlite3_stmt* statements[STMT_END] = {0};

static struct sqlite3* db = NULL;

const char* get_default_db_path(void) {
    static char db_path[PATH_MAX];

    char* xdg_data_home = getenv("XDG_DATA_HOME");
    if (xdg_data_home != NULL) {
        snprintf(db_path, sizeof(db_path), "%s/%s", xdg_data_home, "cclip/db.sqlite3");
    } else {
        char* home = getenv("HOME");
        if (home == NULL) {
            err("both HOME and XDG_DATA_HOME are unset, unable to determine db file path\n");
            return NULL;
        }
        snprintf(db_path, sizeof(db_path), "%s/.local/share/%s", home, "cclip/db.sqlite3");
    }

    return db_path;
}

static int db_prepare_statements(void) {
    int rc;

    /* prepare statements in advance */
    const char* insert_stmt =
        "INSERT OR REPLACE INTO history "
        "(data, data_size, preview, mime_type, timestamp) "
        "VALUES (?, ?, ?, ?, ?)";
    rc = sqlite3_prepare_v2(db, insert_stmt, -1, &statements[STMT_INSERT], NULL);
    if (rc != SQLITE_OK) {
        err("failed to prepare sqlite statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char* delete_oldest_stmt =
        "DELETE FROM history WHERE rowid IN ("
        "   SELECT rowid FROM history "
        "   ORDER BY timestamp DESC "
        "   LIMIT -1 OFFSET ?"
        ")";
    rc = sqlite3_prepare_v2(db, delete_oldest_stmt, -1, &statements[STMT_DELETE_OLDEST], NULL);
    if (rc != SQLITE_OK) {
        err("failed to prepare sqlite statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char* begin_stmt = "BEGIN";
    rc = sqlite3_prepare_v2(db, begin_stmt, -1, &statements[STMT_BEGIN], NULL);
    if (rc != SQLITE_OK) {
        err("failed to prepare sqlite statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char* commit_stmt = "COMMIT";
    rc = sqlite3_prepare_v2(db, commit_stmt, -1, &statements[STMT_COMMIT], NULL);
    if (rc != SQLITE_OK) {
        err("failed to prepare sqlite statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char* rollback_stmt = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, rollback_stmt, -1, &statements[STMT_ROLLBACK], NULL);
    if (rc != SQLITE_OK) {
        err("failed to prepare sqlite statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    return 0;
}

int db_init(const char* const db_path, bool create_if_not_exists) {
    int rc = 0;

    if (access(db_path, F_OK) == -1) {
        if (!create_if_not_exists) {
            err("database file %s does not exist\n", db_path);
            return -1;
        } else {
            info("database file %s does not exist, attempting to create\n", db_path);
        }

        pid_t child_pid = fork();
        if (child_pid == -1) {
            warn("forking child failed: %s\n", strerror(errno));
        } else if (child_pid == 0) {
            /* child */
            execlp("install", "install", "-Dm644", "/dev/null", db_path, NULL);
            warn("execing install -Dm644 /dev/null %s failed: %s\n", db_path, strerror(errno));
            exit(1);
        } else {
            /* parent */
            if (waitpid(child_pid, NULL, 0) == -1) {
                warn("failed to wait for child: %s\n", strerror(errno));
            };
        }
    }

    debug("opening database at %s\n", db_path);
    rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        err("failed to open sqlite database: %s\n", sqlite3_errstr(rc));
        return -1;
    }

    /* enable WAL https://sqlite.org/wal.html */
    rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        err("sql: PRAGMA journal_mode=WAL failed: %s\n", sqlite3_errstr(rc));
        return -1;
    }
    rc = sqlite3_exec(db, "PRAGMA synchronous=NORMAL", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        err("sql: PRAGMA synchronous=NORMAL failed: %s\n", sqlite3_errstr(rc));
        return -1;
    }

    const char* db_create_expr =
        "CREATE TABLE IF NOT EXISTS history ("
        "    data      BLOB    NOT NULL UNIQUE,"
        "    data_size INTEGER NOT NULL,"
        "    preview   TEXT    NOT NULL,"
        "    mime_type TEXT    NOT NULL,"
        "    timestamp INTEGER NOT NULL"
        ")";
    rc = sqlite3_exec(db, db_create_expr, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        err("sql: failed to create history table: %s\n", sqlite3_errstr(rc));
        return -1;
    }

    const char* db_create_timestamp_index_expr =
        "CREATE INDEX IF NOT EXISTS idx_history_timestamp ON history(timestamp)";
    rc = sqlite3_exec(db, db_create_timestamp_index_expr, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        err("sql: failed to create timestamp index: %s\n", sqlite3_errstr(rc));
        return -1;
    }

    return db_prepare_statements();
}

int db_cleanup(void) {
    for (int i = 0; i < STMT_END; i++) {
        sqlite3_finalize(statements[i]);
        statements[i] = NULL;
    }

    int rc = sqlite3_close(db);
    if (rc != SQLITE_OK) {
        err("failed to close database connection: %s\n", sqlite3_errstr(rc));
        return -1;
    }
    db = NULL;

    return 0;
}

int insert_db_entry(const struct db_entry* const entry, int max_entries_count) {
    int rc = 0;

    /* transaction */
    trace("beginning transaction\n");
    rc = sqlite3_step(statements[STMT_BEGIN]);
    if (rc != SQLITE_DONE) {
        err("sql: failed to begin transaction: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_reset(statements[STMT_BEGIN]);

    /* insert */
    sqlite3_bind_blob(statements[STMT_INSERT], 1, entry->data, entry->data_size, SQLITE_STATIC);
    sqlite3_bind_int64(statements[STMT_INSERT], 2, entry->data_size);
    sqlite3_bind_text(statements[STMT_INSERT], 3, entry->preview, -1, SQLITE_STATIC);
    sqlite3_bind_text(statements[STMT_INSERT], 4, entry->mime_type, -1, SQLITE_STATIC);
    sqlite3_bind_int64(statements[STMT_INSERT], 5, entry->timestamp);

    rc = sqlite3_step(statements[STMT_INSERT]);
    if (rc != SQLITE_DONE) {
        err("sql: failed to insert entry into db: %s\n", sqlite3_errmsg(db));
        goto rollback;
    }
    debug("record inserted successfully\n");

    sqlite3_reset(statements[STMT_INSERT]);
    sqlite3_clear_bindings(statements[STMT_INSERT]);

    /* delete oldest entries above the limit */
    if (max_entries_count > 0) {
        sqlite3_bind_int(statements[STMT_DELETE_OLDEST], 1, max_entries_count);

        rc = sqlite3_step(statements[STMT_DELETE_OLDEST]);
        if (rc != SQLITE_DONE) {
            err("sql: failed to delete oldest entries: %s\n", sqlite3_errmsg(db));
            return -1;
        }

        sqlite3_reset(statements[STMT_DELETE_OLDEST]);
        sqlite3_clear_bindings(statements[STMT_DELETE_OLDEST]);
    }

    /* commit transaction */
    trace("ending transaction\n");
    rc = sqlite3_step(statements[STMT_COMMIT]);
    if (rc != SQLITE_DONE) {
        err("sql: failed to commit transaction: %s\n", sqlite3_errmsg(db));
        goto rollback;
    }
    sqlite3_reset(statements[STMT_COMMIT]);

    return 0;

rollback:
    rc = sqlite3_step(statements[STMT_ROLLBACK]);
    if (rc != SQLITE_DONE) {
        err("sql: failed to rollback transaction: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_reset(statements[STMT_ROLLBACK]);
    return -1;
}

