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

#include "db.h"
#include "sql.h"
#include "config.h"
#include "preview.h"
#include "log.h"
#include "macros.h"
#include "xxhash.h"

#define RING_BUFFER_SIZE (16 + 1)

struct queue_entry {
    void* data;
    size_t size;
    char* mime;
};

static struct thread_state {
    struct queue {
        struct queue_entry ring[RING_BUFFER_SIZE];
        unsigned read, write, size;
    } buf;

    bool should_exit;

    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} thread_state = {
    .buf = {
        .size = RING_BUFFER_SIZE,
    },

    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
};

struct db_entry {
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
        VALUES ( @data, @data_hash, @data_size, @preview, @mime_type, @timestamp )
        ON CONFLICT ( data_hash ) DO UPDATE SET timestamp=excluded.timestamp
    )},
    [STMT_DELETE_OLDEST] = { .src = TOSTRING(
        DELETE FROM history
        WHERE id IN (
            SELECT id FROM history
            WHERE id NOT IN (
                SELECT entry_id FROM history_tags
            )
            ORDER BY timestamp DESC
            LIMIT -1 OFFSET @keep_count
        );
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

static bool prepare_statements(struct sqlite3* db) {
    for (size_t i = 0; i < SIZEOF_ARRAY(statements); i++) {
        if (!db_prepare_stmt(db, statements[i].src, &statements[i].stmt)) {
            return false;
        }
    }

    return true;
}

static void cleanup_statements(void) {
    for (size_t i = 0; i < SIZEOF_ARRAY(statements); i++) {
        sqlite3_finalize(statements[i].stmt);
        statements[i].stmt = NULL;
    }
}

static bool begin_transaction(struct sqlite3* db) {
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

static bool rollback_transaction(struct sqlite3* db) {
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

static bool commit_transaction(struct sqlite3* db) {
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

static bool do_insert(struct sqlite3* db, const struct db_entry* e) {
    struct sqlite3_stmt* const stmt = statements[STMT_INSERT].stmt;
    bool ret = true;

    STMT_BIND(stmt, blob, "@data", e->data, e->data_size, SQLITE_STATIC);
    STMT_BIND(stmt, int64, "@data_hash", *(int64_t *)&e->data_hash);
    STMT_BIND(stmt, int64, "@data_size", e->data_size);
    STMT_BIND(stmt, text, "@preview", e->preview, -1, SQLITE_STATIC);
    STMT_BIND(stmt, text, "@mime_type", e->mime_type, -1, SQLITE_STATIC);
    STMT_BIND(stmt, int64, "@timestamp", e->timestamp);

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

static bool do_delete_oldest(struct sqlite3* db, int keep_count) {
    struct sqlite3_stmt* const stmt = statements[STMT_DELETE_OLDEST].stmt;
    bool ret = true;

    STMT_BIND(stmt, int, "@keep_count", keep_count);

    log_print(TRACE, "sql: deleting oldest entries");
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_print(ERR, "sql: failed to delete oldest entries: %s", sqlite3_errmsg(db));
        ret = false;
    }
    log_print(TRACE, "sql: %d oldest entries deleted", sqlite3_changes(db));

    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    return ret;
}

static bool process_queue_entry(struct sqlite3* db, struct queue_entry* e) {
    const uint64_t hash = XXH3_64bits(e->data, e->size);
    const time_t timestamp = time(NULL);
    char* const preview = generate_preview(e->data, e->size, e->mime);

    if (!begin_transaction(db)) {
        free(preview);
        return false;
    }

    const struct db_entry entry = {
        .data = e->data,
        .data_size = e->size,
        .mime_type = e->mime,
        .data_hash = hash,
        .preview = preview,
        .timestamp = timestamp,
    };
    if (!do_insert(db, &entry)) {
        goto rollback;
    } else if (config.max_entries_count > 0) {
        /* only run cleanup every `period` insertions */
        const int period = 10;
        static int counter = 0;
        if (counter++ % period == 0) {
            if (!do_delete_oldest(db, config.max_entries_count)) {
                goto rollback;
            }
        }
    }

    if (!commit_transaction(db)) {
        goto rollback;
    }

    free(preview);
    return true;

rollback:
    free(preview);
    rollback_transaction(db);
    return false;
}

static void queue_entry_free_contents(struct queue_entry* e) {
    free(e->data);
    free(e->mime);
}

static void queue_push(struct queue_entry entry) {
    struct queue* q = &thread_state.buf;

    if ((q->write + 1) % q->size == q->read) {
        /* buffer is full, discard oldest entry */
        log_print(WARN, "ring buffer is full, discarding oldest entry!");

        struct queue_entry* old = &q->ring[q->write];
        queue_entry_free_contents(old);

        q->read = (q->read + 1) % q->size;
    }

    q->ring[q->write] = entry;
    q->write = (q->write + 1) % q->size;

    log_print(TRACE, "added entry to queue, write %d read %d", q->write, q->read);
}

static bool queue_pop(struct queue_entry* entry) {
    struct queue* q = &thread_state.buf;

    if (q->read == q->write) {
        return false;
    }

    *entry = q->ring[q->read];
    q->read = (q->read + 1) % q->size;

    log_print(TRACE, "removed entry from queue, write %d read %d", q->write, q->read);

    return true;
}

static void* thread_entrypoint(void* data) {
    struct sqlite3* db = data;

    pthread_mutex_lock(&thread_state.mutex);
    while (!thread_state.should_exit) {
        /* wait for new entry to be queued (mutex must be held by us) */
        pthread_cond_wait(&thread_state.cond, &thread_state.mutex);

        /* we are now holding the mutex */

        struct queue_entry entry;
        if (!queue_pop(&entry)) {
            /* if we woke up but queue is empty we most likely were asked to die. */
            continue;
        }

        do {
            /* release the mutex so that other thread can keep feeding data */
            pthread_mutex_unlock(&thread_state.mutex);

            process_queue_entry(db, &entry);
            queue_entry_free_contents(&entry);

            /* check for more entries in the buffer */
            pthread_mutex_lock(&thread_state.mutex);
        } while (queue_pop(&entry));
    }

    pthread_mutex_unlock(&thread_state.mutex);
    cleanup_statements();

    return NULL;
}

bool start_db_thread(struct sqlite3* db) {
    if (!prepare_statements(db)) {
        goto err;
    }

    log_print(DEBUG, "starting db thread");
    thread_state.should_exit = false;
    int ret = pthread_create(&thread_state.thread, NULL, thread_entrypoint, db);
    if (ret != 0) {
        log_print(ERR, "failed to create thread: %s", strerror(ret));
        goto err;
    }

    return true;

err:
    thread_state.should_exit = true;
    cleanup_statements();
    return false;
}

void stop_db_thread(void) {
    if (thread_state.should_exit) {
        /* already dead */
        return;
    }

    log_print(DEBUG, "stopping db thread");

    thread_state.should_exit = true;
    pthread_cond_signal(&thread_state.cond);
    pthread_join(thread_state.thread, NULL);
}

void queue_for_insertion(void *data, size_t size, char *mime) {
    pthread_mutex_lock(&thread_state.mutex);
    queue_push((struct queue_entry){
        .data = data,
        .size = size,
        .mime = mime,
    });
    pthread_mutex_unlock(&thread_state.mutex);

    pthread_cond_signal(&thread_state.cond);
}

