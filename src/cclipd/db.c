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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sqlite3.h>

#include "db.h"
#include "log.h"
#include "xxhash.h"

enum {
    STMT_INSERT,
    STMT_DELETE_OLDEST,
    STMT_BEGIN,
    STMT_COMMIT,
    STMT_ROLLBACK,
    STMT_END,
};
static sqlite3_stmt* statements[STMT_END] = {0};

enum {
    DATA_LOCATION      = 1,
    DATA_HASH_LOCATION = 2,
    DATA_SIZE_LOCATION = 3,
    PREVIEW_LOCATION   = 4,
    MIME_TYPE_LOCATION = 5,
    TIMESTAMP_LOCATION = 6,
};
static const char insert_stmt[] =
    "INSERT INTO history(data, data_hash, data_size, preview, mime_type, timestamp) "
    "VALUES(?, ?, ?, ?, ?, ?) "
    "ON CONFLICT(data_hash) DO UPDATE SET timestamp=excluded.timestamp";

static const char delete_oldest_stmt[] =
    "DELETE FROM history WHERE rowid IN ( "
       "SELECT rowid FROM history "
       "WHERE tag IS NULL "
       "ORDER BY timestamp DESC "
       "LIMIT -1 OFFSET ? "
    ")";

static const char db_create_stmt[] =
    "CREATE TABLE IF NOT EXISTS history ( "
        "data      BLOB    NOT NULL, "
        "data_hash INTEGER NOT NULL UNIQUE, "
        "data_size INTEGER NOT NULL, "
        "preview   TEXT    NOT NULL, "
        "mime_type TEXT    NOT NULL, "
        "timestamp INTEGER NOT NULL, "
        "tag       TEXT    UNIQUE "
    ")";

static const char db_create_timestamp_index_stmt[] =
    "CREATE INDEX IF NOT EXISTS idx_history_timestamp ON history(timestamp)";

static struct sqlite3* db = NULL;

static int db_prepare_statements(void) {
    int rc;

    rc = sqlite3_prepare_v2(db, insert_stmt, -1, &statements[STMT_INSERT], NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to prepare sqlite statement: %s", sqlite3_errmsg(db));
        return -1;
    }

    rc = sqlite3_prepare_v2(db, delete_oldest_stmt, -1, &statements[STMT_DELETE_OLDEST], NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to prepare sqlite statement: %s", sqlite3_errmsg(db));
        return -1;
    }

    rc = sqlite3_prepare_v2(db, "BEGIN", -1, &statements[STMT_BEGIN], NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to prepare sqlite statement: %s", sqlite3_errmsg(db));
        return -1;
    }

    rc = sqlite3_prepare_v2(db, "COMMIT", -1, &statements[STMT_COMMIT], NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to prepare sqlite statement: %s", sqlite3_errmsg(db));
        return -1;
    }

    rc = sqlite3_prepare_v2(db, "ROLLBACK", -1, &statements[STMT_ROLLBACK], NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to prepare sqlite statement: %s", sqlite3_errmsg(db));
        return -1;
    }

    return 0;
}

int db_init(const char* const db_path, bool create_if_not_exists) {
    int rc = 0;

    if (access(db_path, F_OK) == -1) {
        if (!create_if_not_exists) {
            log_print(ERR, "database file %s does not exist", db_path);
            return -1;
        } else {
            log_print(INFO, "database file %s does not exist, attempting to create", db_path);
        }

        pid_t child_pid = fork();
        if (child_pid == -1) {
            log_print(WARN, "forking child failed: %s", strerror(errno));
        } else if (child_pid == 0) {
            /* child */
            execlp("install", "install", "-Dm644", "/dev/null", db_path, NULL);
            log_print(WARN, "execing install -Dm644 /dev/null %s failed: %s",
                      db_path, strerror(errno));
            exit(1);
        } else {
            /* parent */
            if (waitpid(child_pid, NULL, 0) == -1) {
                log_print(WARN, "failed to wait for child: %s", strerror(errno));
            };
        }
    }

    log_print(DEBUG, "opening database at %s", db_path);
    rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to open sqlite database: %s", sqlite3_errstr(rc));
        return -1;
    }

    /* enable WAL https://sqlite.org/wal.html */
    rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "sql: PRAGMA journal_mode=WAL failed: %s", sqlite3_errstr(rc));
        return -1;
    }
    rc = sqlite3_exec(db, "PRAGMA synchronous=NORMAL", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "sql: PRAGMA synchronous=NORMAL failed: %s", sqlite3_errstr(rc));
        return -1;
    }

    rc = sqlite3_exec(db, db_create_stmt, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "sql: failed to create history table: %s", sqlite3_errstr(rc));
        return -1;
    }

    rc = sqlite3_exec(db, db_create_timestamp_index_stmt, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "sql: failed to create timestamp index: %s", sqlite3_errstr(rc));
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
        log_print(ERR, "failed to close database connection: %s", sqlite3_errstr(rc));
        return -1;
    }
    db = NULL;

    return 0;
}

int insert_db_entry(const struct db_entry* const entry, int max_entries_count) {
    int rc = 0;

    uint64_t data_hash = XXH3_64bits(entry->data, entry->data_size);
    log_print(TRACE, "entry hash: %016lX", data_hash);

    /* transaction */
    log_print(TRACE, "beginning transaction");
    rc = sqlite3_step(statements[STMT_BEGIN]);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to begin transaction: %s", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_reset(statements[STMT_BEGIN]);

    /* insert */
    sqlite3_bind_blob(statements[STMT_INSERT], DATA_LOCATION,
                      entry->data, entry->data_size, SQLITE_STATIC);
    sqlite3_bind_int64(statements[STMT_INSERT], DATA_HASH_LOCATION, *(int64_t *)&data_hash);
    sqlite3_bind_int64(statements[STMT_INSERT], DATA_SIZE_LOCATION, entry->data_size);
    sqlite3_bind_text(statements[STMT_INSERT], PREVIEW_LOCATION,
                      entry->preview, -1, SQLITE_STATIC);
    sqlite3_bind_text(statements[STMT_INSERT], MIME_TYPE_LOCATION,
                      entry->mime_type, -1, SQLITE_STATIC);
    sqlite3_bind_int64(statements[STMT_INSERT], TIMESTAMP_LOCATION, entry->timestamp);

    rc = sqlite3_step(statements[STMT_INSERT]);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to insert entry into db: %s", sqlite3_errmsg(db));
        goto rollback;
    }
    log_print(DEBUG, "record inserted successfully");

    sqlite3_reset(statements[STMT_INSERT]);
    sqlite3_clear_bindings(statements[STMT_INSERT]);

    /* delete oldest entries above the limit */
    if (max_entries_count > 0) {
        sqlite3_bind_int(statements[STMT_DELETE_OLDEST], 1, max_entries_count);

        rc = sqlite3_step(statements[STMT_DELETE_OLDEST]);
        if (rc != SQLITE_DONE) {
            log_print(ERR, "sql: failed to delete oldest entries: %s", sqlite3_errmsg(db));
            return -1;
        }

        sqlite3_reset(statements[STMT_DELETE_OLDEST]);
        sqlite3_clear_bindings(statements[STMT_DELETE_OLDEST]);
    }

    /* commit transaction */
    log_print(TRACE, "ending transaction");
    rc = sqlite3_step(statements[STMT_COMMIT]);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to commit transaction: %s", sqlite3_errmsg(db));
        goto rollback;
    }
    sqlite3_reset(statements[STMT_COMMIT]);

    return 0;

rollback:
    rc = sqlite3_step(statements[STMT_ROLLBACK]);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to rollback transaction: %s", sqlite3_errmsg(db));
    }
    sqlite3_reset(statements[STMT_ROLLBACK]);
    return -1;
}

