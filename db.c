#include <stdio.h> /* snprintf */
#include <stdlib.h> /* getenv */
#include <unistd.h> /* getuid, access */
#include <sys/types.h> /* getpwuid */
#include <pwd.h> /* getpwuid */
#include <string.h> /* strlen */
#include <sqlite3.h> /* all the sqlite stuff */
#include <stdbool.h>

#include "common.h"

struct sqlite3* db;

sqlite3* db_init(const char* const db_path) {
    if (access(db_path, F_OK) == -1) {
        warn("database file does not exist\n");
        /* TODO: also recursively create all needed directories */
        FILE* db_file = fopen(db_path, "w+");
        if (db_file == NULL) {
            die("unable to create database file\n");
        }
        fclose(db_file);
    }

    int ret_code = sqlite3_open(db_path, &db);
    if (ret_code != SQLITE_OK) {
        die("%s\n", sqlite3_errstr(ret_code));
    }

    const char* db_create_expr =
        "CREATE TABLE IF NOT EXISTS history ("
        "    id        INTEGER PRIMARY KEY,"
        "    data      BLOB    NOT NULL UNIQUE,"
        "    data_size INTEGER NOT NULL,"
        "    preview   TEXT    NOT NULL,"
        "    mime_type TEXT    NOT NULL,"
        "    timestamp INTEGER NOT NULL"
        ")";

    char* errmsg;
    ret_code = sqlite3_exec(db, db_create_expr, NULL, NULL, &errmsg);
    if (ret_code != SQLITE_OK) {
        die("%s\n", errmsg);
    }

    return db;
}
