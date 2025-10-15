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
#include <stdlib.h>

#include <sqlite3.h>

#include "cclipd.h"
#include "sql.h"
#include "config.h"
#include "preview.h"
#include "log.h"
#include "macros.h"
#include "xxhash.h"

struct db_entry {
    int64_t rowid; /* https://www.sqlite.org/lang_createtable.html#rowid */
    const void* data; /* arbitrary data */
    int64_t data_size; /* size of data in bytes */
    uint64_t data_hash; /* xxhash3 */
    char* preview; /* string */
    const char* mime_type; /* string */
    time_t timestamp; /* unix seconds */
};

enum {
    STMT_INSERT,
    STMT_DELETE_OLDEST,
    STMT_BEGIN,
    STMT_COMMIT,
    STMT_ROLLBACK,
};

static struct {
    const char *src;
    struct sqlite3_stmt* stmt;
} statements[] = {
    [STMT_INSERT] = { .src = TOSTRING(
        INSERT INTO history ( data, data_hash, data_size, preview, mime_type, timestamp )
        VALUES ( ?, ?, ?, ?, ?, ? )
        ON CONFLICT (data_hash) DO UPDATE SET timestamp=excluded.timestamp
    )},
    [STMT_DELETE_OLDEST] = { .src = TOSTRING(
        DELETE FROM history WHERE rowid IN (
            SELECT rowid FROM history
            WHERE tag IS NULL
            ORDER BY timestamp DESC
            LIMIT -1 OFFSET ?
        )
    )},
    [STMT_BEGIN] = { .src = TOSTRING(
        BEGIN
    )},
    [STMT_COMMIT] = { .src = TOSTRING(
        COMMIT
    )},
    [STMT_ROLLBACK] = { .src = TOSTRING(
        ROLLBACK
    )},
};

/* TODO: use named parameters instead of this nonsense */
enum {
    DATA_LOCATION      = 1,
    DATA_HASH_LOCATION = 2,
    DATA_SIZE_LOCATION = 3,
    PREVIEW_LOCATION   = 4,
    MIME_TYPE_LOCATION = 5,
    TIMESTAMP_LOCATION = 6,
};

bool prepare_statements(void) {
    int rc;

    for (size_t i = 0; i < SIZEOF_ARRAY(statements); i++) {
        rc = sqlite3_prepare_v2(db, statements[i].src, -1, &statements[i].stmt, NULL);
        if (rc != SQLITE_OK) {
            log_print(ERR, "failed to prepare sqlite statement:");
            log_print(ERR, "%s", statements[i].src);
            log_print(ERR, "reason: %s", sqlite3_errmsg(db));
            return false;
        }
    }

    return true;
}

void cleanup_statements(void) {
    for (size_t i = 0; i < SIZEOF_ARRAY(statements); i++) {
        sqlite3_finalize(statements[i].stmt);
        statements[i].stmt = NULL;
    }
}

static bool begin_transaction(void) {
    struct sqlite3_stmt* const stmt = statements[STMT_BEGIN].stmt;
    bool ret = true;

    log_print(TRACE, "beginning transaction");
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to begin transaction: %s", sqlite3_errmsg(db));
        ret = false;
    }

    sqlite3_reset(stmt);
    return ret;
}

static bool rollback_transaction(void) {
    struct sqlite3_stmt* const stmt = statements[STMT_ROLLBACK].stmt;
    bool ret = true;

    log_print(TRACE, "rolling back transaction");
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to rollback transaction: %s", sqlite3_errmsg(db));
        ret = false;
    }

    sqlite3_reset(stmt);
    return ret;
}

static bool commit_transaction(void) {
    struct sqlite3_stmt* const stmt = statements[STMT_COMMIT].stmt;
    bool ret = true;

    log_print(TRACE, "committing transaction");
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to commit transaction: %s", sqlite3_errmsg(db));
        ret = false;
    }

    sqlite3_reset(stmt);
    return ret;
}

static bool do_insert(const struct db_entry* e) {
    struct sqlite3_stmt* const stmt = statements[STMT_INSERT].stmt;
    bool ret = true;

    sqlite3_bind_blob(stmt, DATA_LOCATION, e->data, e->data_size, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, DATA_HASH_LOCATION, *(int64_t *)&e->data_hash);
    sqlite3_bind_int64(stmt, DATA_SIZE_LOCATION, e->data_size);
    sqlite3_bind_text(stmt, PREVIEW_LOCATION, e->preview, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, MIME_TYPE_LOCATION, e->mime_type, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, TIMESTAMP_LOCATION, e->timestamp);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to insert entry into db: %s", sqlite3_errmsg(db));
        ret = false;
    } else {
        log_print(DEBUG, "record inserted successfully");
    }

    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    return ret;
}

static bool do_delete_oldest(int leave_count) {
    struct sqlite3_stmt* const stmt = statements[STMT_DELETE_OLDEST].stmt;
    bool ret = true;

    sqlite3_bind_int(stmt, 1, leave_count);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to delete oldest entries: %s", sqlite3_errmsg(db));
        ret = false;
    }

    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    return ret;
}

bool insert_db_entry(const void* data, size_t data_size, const char* mime) {
    const uint64_t data_hash = XXH3_64bits(data, data_size);
    const time_t timestamp = time(NULL);
    char* const preview = generate_preview(data, data_size, mime);

    if (!begin_transaction()) {
        free(preview);
        return false;
    }

    const struct db_entry entry = {
        .data = data,
        .data_size = data_size,
        .data_hash = data_hash,
        .mime_type = mime,
        .preview = preview,
        .timestamp = timestamp,
    };
    if (!do_insert(&entry)) {
        goto rollback;
    } else if (config.max_entries_count > 0 && !do_delete_oldest(config.max_entries_count)) {
        goto rollback;
    }

    if (!commit_transaction()) {
        goto rollback;
    }

    free(preview);
    return true;

rollback:
    free(preview);
    rollback_transaction();
    return false;
}

