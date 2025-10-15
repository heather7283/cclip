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

#include "cclipd.h"
#include "sql.h"
#include "config.h"
#include "preview.h"
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

bool prepare_statements(void) {
    int rc;

    rc = sqlite3_prepare_v2(db, insert_stmt, -1, &statements[STMT_INSERT], NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to prepare sqlite statement: %s", sqlite3_errmsg(db));
        return false;
    }

    rc = sqlite3_prepare_v2(db, delete_oldest_stmt, -1, &statements[STMT_DELETE_OLDEST], NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to prepare sqlite statement: %s", sqlite3_errmsg(db));
        return false;
    }

    rc = sqlite3_prepare_v2(db, "BEGIN", -1, &statements[STMT_BEGIN], NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to prepare sqlite statement: %s", sqlite3_errmsg(db));
        return false;
    }

    rc = sqlite3_prepare_v2(db, "COMMIT", -1, &statements[STMT_COMMIT], NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to prepare sqlite statement: %s", sqlite3_errmsg(db));
        return false;
    }

    rc = sqlite3_prepare_v2(db, "ROLLBACK", -1, &statements[STMT_ROLLBACK], NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to prepare sqlite statement: %s", sqlite3_errmsg(db));
        return false;
    }

    return true;
}

void cleanup_statements(void) {
    for (int i = 0; i < STMT_END; i++) {
        sqlite3_finalize(statements[i]);
        statements[i] = NULL;
    }
}

bool insert_db_entry(const void* data, size_t data_size, const char* mime) {
    int rc = 0;
    char* preview = NULL;

    uint64_t data_hash = XXH3_64bits(data, data_size);
    log_print(TRACE, "entry hash: %016lX", data_hash);

    time_t timestamp = time(NULL);

    preview = generate_preview(data, data_size, mime);

    /* transaction */
    log_print(TRACE, "beginning transaction");
    sqlite3_reset(statements[STMT_BEGIN]);
    rc = sqlite3_step(statements[STMT_BEGIN]);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to begin transaction: %s", sqlite3_errmsg(db));
        goto out;
    }

    /* insert */
    sqlite3_reset(statements[STMT_INSERT]);
    sqlite3_clear_bindings(statements[STMT_INSERT]);

    sqlite3_bind_blob(statements[STMT_INSERT], DATA_LOCATION, data, data_size, SQLITE_STATIC);
    sqlite3_bind_int64(statements[STMT_INSERT], DATA_HASH_LOCATION, *(int64_t *)&data_hash);
    sqlite3_bind_int64(statements[STMT_INSERT], DATA_SIZE_LOCATION, data_size);
    sqlite3_bind_text(statements[STMT_INSERT], PREVIEW_LOCATION, preview, -1, SQLITE_STATIC);
    sqlite3_bind_text(statements[STMT_INSERT], MIME_TYPE_LOCATION, mime, -1, SQLITE_STATIC);
    sqlite3_bind_int64(statements[STMT_INSERT], TIMESTAMP_LOCATION, timestamp);

    rc = sqlite3_step(statements[STMT_INSERT]);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to insert entry into db: %s", sqlite3_errmsg(db));
        goto rollback;
    }
    log_print(DEBUG, "record inserted successfully");

    /* delete oldest entries above the limit */
    if (config.max_entries_count > 0) {
        sqlite3_reset(statements[STMT_DELETE_OLDEST]);
        sqlite3_clear_bindings(statements[STMT_DELETE_OLDEST]);

        sqlite3_bind_int(statements[STMT_DELETE_OLDEST], 1, config.max_entries_count);

        rc = sqlite3_step(statements[STMT_DELETE_OLDEST]);
        if (rc != SQLITE_DONE) {
            log_print(ERR, "sql: failed to delete oldest entries: %s", sqlite3_errmsg(db));
            goto rollback;
        }
    }

    /* commit transaction */
    log_print(TRACE, "ending transaction");
    sqlite3_reset(statements[STMT_COMMIT]);
    rc = sqlite3_step(statements[STMT_COMMIT]);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to commit transaction: %s", sqlite3_errmsg(db));
        goto rollback;
    }

out:
    free(preview);
    return true;

rollback:
    free(preview);
    sqlite3_reset(statements[STMT_ROLLBACK]);
    rc = sqlite3_step(statements[STMT_ROLLBACK]);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to rollback transaction: %s", sqlite3_errmsg(db));
    }
    return false;
}

