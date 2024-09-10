#ifndef DB_H
#define DB_H

#include <stdint.h>
#include <time.h>
#include <sqlite3.h>

extern struct sqlite3* db;

struct db_entry {
    int64_t rowid; /* https://www.sqlite.org/lang_createtable.html#rowid */
    void* data; /* arbitrary data */
    int64_t data_size; /* size of data in bytes */
    char* preview; /* string */
    char* mime_type; /* string */
    time_t creation_time; /* unix seconds */
};

/* initialises empty db if db is not found */
int db_init(const char* const db_path);

#endif /* #ifndef DB_H */
