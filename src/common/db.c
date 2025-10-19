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
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "db.h"
#include "macros.h"
#include "log.h"

/*
 * History: schema versions
 *
 * Schema version 1: cclip 3.0.0
 *
 * CREATE TABLE history (
 *     data      BLOB    NOT NULL,
 *     data_hash INTEGER NOT NULL UNIQUE,
 *     data_size INTEGER NOT NULL,
 *     preview   TEXT    NOT NULL,
 *     mime_type TEXT    NOT NULL,
 *     timestamp INTEGER NOT NULL
 * );
 * CREATE INDEX idx_history_timestamp ON history (timestamp);
 *
 * Schema version 2: cclip 3.0.0-next (tag column added)
 *
 * CREATE TABLE history (
 *     data      BLOB    NOT NULL,
 *     data_hash INTEGER NOT NULL UNIQUE,
 *     data_size INTEGER NOT NULL,
 *     preview   TEXT    NOT NULL,
 *     mime_type TEXT    NOT NULL,
 *     timestamp INTEGER NOT NULL,
 *     tag       TEXT    UNIQUE
 * );
 * CREATE INDEX idx_history_timestamp ON history (timestamp);
 *
 * Schema version 3: cclip 3.1.0
 *
 * CREATE TABLE history (
 *     id        INTEGER PRIMARY KEY,
 *     data      BLOB    NOT NULL,
 *     data_hash INTEGER NOT NULL UNIQUE,
 *     data_size INTEGER NOT NULL,
 *     preview   TEXT    NOT NULL,
 *     mime_type TEXT    NOT NULL,
 *     timestamp INTEGER NOT NULL
 * );
 * CREATE INDEX idx_history_timestamp ON history (timestamp);
 *
 * CREATE TABLE tags (
 *     id   INTEGER PRIMARY KEY,
 *     name TEXT    NOT NULL UNIQUE
 * );
 *
 * CREATE TABLE history_tags (
 *     tag_id   INTEGER,
 *     entry_id INTEGER,
 *
 *     PRIMARY KEY ( tag_id, entry_id ),
 *     FOREIGN KEY ( entry_id ) REFERENCES history ( id ) ON DELETE CASCADE,
 *     FOREIGN KEY ( tag_id ) REFERENCES tags ( id ) ON DELETE RESTRICT
 * ) WITHOUT ROWID;
 *
 * CREATE TRIGGER cleanup_orphaned_tags AFTER DELETE ON history_tags FOR EACH ROW BEGIN
 *     DELETE FROM tags
 *     WHERE id = OLD.tag_id
 *     AND NOT EXISTS ( SELECT 1 FROM history_tags WHERE tag_id = OLD.tag_id );
 * END;
 *
 */

static const char* get_default_db_path(void) {
    static char db_path[PATH_MAX];

    const char* xdg_data_home = getenv("XDG_DATA_HOME");
    const char* home = getenv("HOME");

    if (xdg_data_home != NULL) {
        snprintf(db_path, sizeof(db_path), "%s/cclip/db.sqlite3", xdg_data_home);
    } else if (home != NULL) {
        log_print(WARN, "XDG_DATA_HOME is not set");
        snprintf(db_path, sizeof(db_path), "%s/.local/share/cclip/db.sqlite3", home);
    } else {
        log_print(WARN, "HOME is not set");
        return NULL;
    }

    return db_path;
}

struct sqlite3* db_open(const char *_path, bool create_if_not_exists) {
    const char* path = (_path == NULL) ? get_default_db_path() : _path;
    if (path == NULL) {
        log_print(ERR, "failed to determine database path");
        return NULL;
    }

    if (access(path, F_OK) == -1) {
        if (!create_if_not_exists) {
            log_print(ERR, "database file %s does not exist", path);
            return NULL;
        } else {
            log_print(INFO, "database file %s does not exist, attempting to create", path);
        }

        pid_t child_pid = fork();
        if (child_pid == -1) {
            log_print(WARN, "forking child failed: %s", strerror(errno));
        } else if (child_pid == 0) {
            /* child */
            execlp("install", "install", "-Dm644", "/dev/null", path, NULL);
            log_print(WARN, "exec install -Dm644 /dev/null %s failed: %s", path, strerror(errno));
            exit(1);
        } else {
            /* parent */
            if (waitpid(child_pid, NULL, 0) == -1) {
                log_print(WARN, "failed to wait for child: %s", strerror(errno));
            };
        }
    }

    log_print(DEBUG, "opening database at %s", path);
    struct sqlite3* db = NULL;
    int rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK) {
        log_print(ERR, "failed to open database at %s: %s", path, sqlite3_errstr(rc));
        return NULL;
    }

    /* must be enabled explicitly per connection */
    if (sqlite3_exec(db, "PRAGMA foreign_keys = 1", NULL, NULL, NULL) != SQLITE_OK) {
        log_print(ERR, "failed to enable foreign key support: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    return db;
}

bool db_close(struct sqlite3* db) {
    if (sqlite3_close(db) != SQLITE_OK) {
        log_print(ERR, "failed to close database, report this as a bug");
        return false;
    }

    return true;
}

bool db_init(struct sqlite3* db) {
    static const char sql[] = TOSTRING(
        PRAGMA journal_mode = WAL;

        CREATE TABLE history (
            id        INTEGER PRIMARY KEY,
            data      BLOB    NOT NULL,
            data_size INTEGER NOT NULL,
            data_hash INTEGER NOT NULL UNIQUE,
            preview   TEXT    NOT NULL,
            mime_type TEXT    NOT NULL,
            timestamp INTEGER NOT NULL,
        );

        CREATE INDEX idx_history_timestamp ON history (timestamp);

        CREATE TABLE tags (
            id   INTEGER PRIMARY KEY,
            name TEXT    NOT NULL UNIQUE
        );

        CREATE TABLE history_tags (
            tag_id   INTEGER,
            entry_id INTEGER,

            PRIMARY KEY ( tag_id, entry_id ),
            FOREIGN KEY ( entry_id ) REFERENCES history ( id ) ON DELETE CASCADE,
            FOREIGN KEY ( tag_id ) REFERENCES tags ( id ) ON DELETE RESTRICT
        ) WITHOUT ROWID;

        CREATE TRIGGER cleanup_orphaned_tags AFTER DELETE ON history_tags FOR EACH ROW BEGIN
            DELETE FROM tags
            WHERE id = OLD.tag_id
            AND NOT EXISTS ( SELECT 1 FROM history_tags WHERE tag_id = OLD.tag_id );
        END;

        PRAGMA user_version = 3;
    );

    int rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "sql: failed to initialise database: %s", sqlite3_errmsg(db));
        return false;
    }

    return true;
}

int32_t db_get_user_version(struct sqlite3* db) {
    int rc;
    int32_t res = INT32_MAX;

    struct sqlite3_stmt* stmt = NULL;
    sqlite3_prepare(db, "PRAGMA user_version", -1, &stmt, NULL);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        res = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);

    if (res == 0) {
        struct sqlite3_stmt* stmt = NULL;
        sqlite3_prepare(db, "SELECT * FROM sqlite_schema WHERE type='table' AND name='history'",
                        -1, &stmt, NULL);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            /* 'history' table was found, now check whether 'tag' column exists */
            const char *history_sql = (const char *)sqlite3_column_text(stmt, 4);
            if (strstr(history_sql, "tag") != NULL) {
                /* found 'tag', return 2 */
                res = 2;
            } else {
                res = 1;
            }
        }

        sqlite3_finalize(stmt);
    }

    return res;
}

bool db_set_user_version(struct sqlite3* db, int32_t version) {
    int rc;
    bool ret = true;

    struct sqlite3_stmt* stmt = NULL;
    sqlite3_prepare(db, "PRAGMA user_version = ?", -1, &stmt, NULL);

    sqlite3_bind_int64(stmt, 0, version);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        log_print(ERR, "failed to set user_version to %d: %s", version, sqlite3_errmsg(db));
        ret = false;
    }

    sqlite3_finalize(stmt);
    return ret;
}

bool db_set_secure_delete(struct sqlite3* db, bool enable) {
    int rc;
    bool ret = true;

    struct sqlite3_stmt* stmt = NULL;
    sqlite3_prepare(db, "PRAGMA secure_delete = ?", -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 0, (int)enable);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        log_print(ERR, "failed to set secure_delete to %d: %s", enable, sqlite3_errmsg(db));
        ret = false;
    }

    sqlite3_finalize(stmt);
    return ret;
}

static bool migrate_from_2_to_3(struct sqlite3* db) {
    /* it is not possible to add a PRIMARY KEY column to a sqlite table */
    static const char sql[] = TOSTRING(
        CREATE TABLE new_history (
            id        INTEGER PRIMARY KEY,
            data      BLOB    NOT NULL,
            data_size INTEGER NOT NULL,
            data_hash INTEGER NOT NULL UNIQUE,
            preview   TEXT    NOT NULL,
            mime_type TEXT    NOT NULL,
            timestamp INTEGER NOT NULL
        );

        CREATE TABLE tags (
            id   INTEGER PRIMARY KEY,
            name TEXT    NOT NULL UNIQUE
        );

        CREATE TABLE history_tags (
            tag_id   INTEGER,
            entry_id INTEGER,

            PRIMARY KEY ( tag_id, entry_id ),
            FOREIGN KEY ( entry_id ) REFERENCES new_history ( id ) ON DELETE CASCADE,
            FOREIGN KEY ( tag_id ) REFERENCES tags ( id ) ON DELETE RESTRICT
        ) WITHOUT ROWID;

        CREATE TRIGGER cleanup_orphaned_tags AFTER DELETE ON history_tags FOR EACH ROW BEGIN
            DELETE FROM tags
            WHERE id = OLD.tag_id
            AND NOT EXISTS ( SELECT 1 FROM history_tags WHERE tag_id = OLD.tag_id );
        END;

        INSERT INTO new_history (
            id, data, data_hash, data_size, preview, mime_type, timestamp
        ) SELECT
            rowid, data, data_hash, data_size, preview, mime_type, timestamp
        FROM history;

        INSERT INTO tags ( name ) SELECT tag FROM history WHERE tag IS NOT NULL;

        INSERT INTO history_tags ( tag_id, entry_id )
        SELECT tags.id, history.rowid
        FROM history
        JOIN tags ON history.tag = tags.name
        WHERE history.tag IS NOT NULL;

        DROP TABLE history;
        ALTER TABLE new_history RENAME TO history;

        CREATE INDEX idx_history_timestamp ON history ( timestamp );

        PRAGMA user_version = 3;
    );

    int rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "migration: %s", sqlite3_errmsg(db));
        return false;
    }

    return true;
}

static bool migrate_from_1_to_2(struct sqlite3* db) {
    /* it is not possible to add a UNIQUE column to a sqlite table */
    static const char sql[] = TOSTRING(
        CREATE TABLE new_history (
            data      BLOB    NOT NULL,
            data_hash INTEGER NOT NULL UNIQUE,
            data_size INTEGER NOT NULL,
            preview   TEXT    NOT NULL,
            mime_type TEXT    NOT NULL,
            timestamp INTEGER NOT NULL,
            tag       TEXT    UNIQUE
        );

        INSERT INTO new_history (
            rowid, data, data_hash, data_size, preview, mime_type, timestamp
        ) SELECT
            rowid, data, data_hash, data_size, preview, mime_type, timestamp
        FROM history;

        DROP TABLE history;
        ALTER TABLE new_history RENAME TO history;

        CREATE INDEX idx_history_timestamp ON history ( timestamp );
    );

    int rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "migration: %s", sqlite3_errmsg(db));
        return false;
    }

    return true;
}

typedef bool (*migration_function_t)(struct sqlite3* db);

/* each function only handles migration from (i) to (i + 1) */
static const migration_function_t migration_functions[] = {
    [1] = migrate_from_1_to_2,
    [2] = migrate_from_2_to_3,
};

bool db_migrate(struct sqlite3 *db, int32_t from, int32_t to) {
    log_print(INFO, "migration: need to migrate from %d to %d", from, to);

    int rc;

    rc = sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "migration: failed to start transaction");
        return false;
    }

    while (from < to) {
        if (!migration_functions[from++](db)) {
            log_print(ERR, "migration: failed to migrate to version %d", from);
            goto rollback;
        } else {
            log_print(INFO, "migration: migration to version %d completed", from);
        }
    }

    rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        log_print(ERR, "migration: failed to commit transaction");
        goto rollback;
    }

    return true;

rollback:
    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    return false;
}

