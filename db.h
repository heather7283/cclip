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
#ifndef DB_H
#define DB_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <sqlite3.h>

extern struct sqlite3* db;

struct db_entry {
    int64_t rowid; /* https://www.sqlite.org/lang_createtable.html#rowid */
    void* data; /* arbitrary data */
    int64_t data_size; /* size of data in bytes */
    char* preview; /* string */
    char* mime_type; /* string */
    time_t timestamp; /* unix seconds */
};

/* initialises empty db if db is not found */
void db_init(const char* const db_path, bool create_if_not_exists);

#endif /* #ifndef DB_H */
