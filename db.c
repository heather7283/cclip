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

char* get_db_path() {
    /* default path is XDG_DATA_HOME/cclip/db */
    char* db_path = NULL;

    char* prefix = NULL;
    size_t prefix_len = -1;

    const char* infix = "/.local/share";
    const size_t infix_len = strlen(infix);

    const char* const postfix = "/cclip/db";
    const size_t postfix_len = strlen(postfix);

    prefix = getenv("XDG_DATA_HOME");
    if (prefix == NULL) {
        debug("XDG_DATA_HOME is unset, using default location");

        const struct passwd* const pw = getpwuid(getuid());
        if (pw == NULL) {
            die("unable to determine current user's home directory and XDG_DATA_HOME is not set");
        }
        const char* const homedir = pw->pw_dir;
        const size_t homedir_len = strlen(homedir);

        prefix = malloc(homedir_len + infix_len + 1);
        snprintf(prefix, homedir_len + infix_len, "%s%s", homedir, infix);
    }
    prefix_len = strlen(prefix);

    const size_t db_path_len = prefix_len + postfix_len;
    db_path = malloc(db_path_len + 1);
    if (db_path == NULL) {
        die("failed to allocate memory for database path string");
    }
    snprintf(db_path, db_path_len + 1, "%s%s", prefix, postfix);

    debug("database file path:");
    debug(db_path);

    return db_path;
}

sqlite3* db_init(const char* const db_path) {
    bool is_newly_created = false;

    if (access(db_path, F_OK) == -1) {
        warn("database file does not exist");
        /* TODO: also recursively create all needed directories */
        FILE* db_file = fopen(db_path, "w+");
        if (db_file == NULL) {
            die("unable to create database file");
        }
        fclose(db_file);
    }

    int ret_code = sqlite3_open(db_path, &db);
    if (ret_code != SQLITE_OK) {
        die(sqlite3_errstr(ret_code));
    }

    char* db_create_expr =
        "CREATE TABLE IF NOT EXISTS history ("
        "    id        INTEGER PRIMARY KEY,"
        "    data      BLOB    NOT NULL"
        "                      UNIQUE,"
        "    mime_type TEXT    NOT NULL,"
        "    timestamp INTEGER NOT NULL"
        ")";

    char* errmsg;
    ret_code = sqlite3_exec(db, db_create_expr, NULL, NULL, &errmsg);
    if (ret_code != SQLITE_OK) {
        die(errmsg);
    }

    return db;
}

