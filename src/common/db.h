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

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <sqlite3.h>

#define DB_USER_SCHEMA_VERSION 2

/* opens the database at path (or default path is NULL) */
struct sqlite3* db_open(const char *path, bool create_if_not_exists);

/* close the db connection */
void db_close(struct sqlite3* db);

/*
 * Initialise the db (create tables, indices, etc)
 * Meant to be called when user_version == 0 (aka on a freshly created db)
 */
bool db_init(struct sqlite3* db);

/*
 * This is my first serious programming project, and obviously when I started
 * working on it I didn't know anything about migrations and didn't care much
 * about backwards compatibility. Now though I hate myself for it.
 *
 * Versions <=3.0.0 did not set user_version, so there's no way to reliably
 * detect schema version. However, this function will try to detect 'history'
 * table being present, and if it is, return 1 instead of 0. Additionally, if
 * the 'tag' column is present as well, then 2 will be returned.
 *
 * So:
 * Empty, just created database: 0
 * Database created with cclip 3.0.0: 1
 * Database created with cclip 3.0.0-next (with tag column): 2
 */
int32_t db_get_user_version(struct sqlite3* db);
bool db_set_user_version(struct sqlite3* db, int32_t version);

bool db_set_secure_delete(struct sqlite3* db, bool enable);

/* perform migration */
bool db_migrate(struct sqlite3* db, int32_t from, int32_t to);

