#include <stdint.h>
#include <sqlite3.h>

extern struct sqlite3* db;

struct clipboard_entry {
    int64_t rowid; /* https://www.sqlite.org/lang_createtable.html#rowid */
    void* data; /* arbitrary data */
    char* mime_type; /* string */
    int64_t creation_time; /* unix seconds */
};

/* returns path to database file */
char* get_db_path();

/* initialises empty db if db is not found */
int db_init(char* db_path);

