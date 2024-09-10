#include <alloca.h>
#include <sqlite3.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "config.h"
#include "db.h"

int argc;
char** argv;
char* prog_name;

int print_row(void* data, int argc, char** argv, char** column_names) {
    UNUSED(data);
    UNUSED(argc);
    UNUSED(column_names);

    printf("%s\t%s\t%s\n", argv[0], argv[1], argv[2]);
    return 0;
}

void list(void) {
    /* TODO: maybe allow user to specify which fields to output? */
    char* errmsg = NULL;
    int retcode;
    const char* sql = "SELECT rowid,mime_type,preview FROM history ORDER BY rowid DESC";

    retcode = sqlite3_exec(db, sql, print_row, NULL, &errmsg);
    if (retcode != SQLITE_OK) {
        die("sqlite error: %s\n", errmsg);
    }
}

void get(int64_t id) {
    sqlite3_stmt* stmt;
    int retcode;

    const char* sql = "SELECT data FROM history WHERE rowid = ?";

    retcode = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        die("sqlite error: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_bind_int(stmt, 1, id);

    retcode = sqlite3_step(stmt);
    if (retcode == SQLITE_ROW) {
        int data_size = sqlite3_column_bytes(stmt, 0);
        const void* data = sqlite3_column_blob(stmt, 0);

        fwrite(data, 1, data_size, stdout);
    } else if (retcode == SQLITE_DONE) {
        die("no entry found with specified id");
    } else {
        die("sqlite error: %s\n", sqlite3_errmsg(db));
    }
}

void print_help_and_exit(int exit_status) {
    const char* help_string =
        "cclip - command line interface for cclipd database\n"
        "\n"
        "usage:\n"
        "    %s list|get|delete|wipe|help [id]\n"
        "\n"
        "actions description:\n"
        "    list       print list of all entries to stdout\n"
        "    get        print entry with specified id to stdout\n"
        "    delete     delete entry with specified id (not implemented)\n"
        "    wipe       delete all entries from database (not implemented)\n"
        "    help       display this help message\n";

    fprintf(stderr, help_string, prog_name);
    exit(exit_status);
}

int main(int _argc, char** _argv) {
    argc = _argc;
    argv = _argv;
    prog_name = argc > 0 ? argv[0] : "cclip";

    if (argc < 2) {
        print_help_and_exit(1);
    }

    config_set_default_values();
    db_init(config.db_path);

    if (strcmp(argv[1], "list") == 0) {
        list();
    } else if (strcmp(argv[1], "get") == 0) {
        if (argc < 3) {
            die("missing id\n");
        }
        int64_t id = atoll(argv[2]);
        if (id == 0) {
            die("failed converting %s to integer\n", argv[2]);
        }
        get(id);
    } else if (strcmp(argv[1], "delete") == 0) {
        die("not implemented\n");
    } else if (strcmp(argv[1], "wipe") == 0) {
        die("not implemented\n");
    } else if (strcmp(argv[1], "help") == 0) {
        print_help_and_exit(0);
    } else {
        print_help_and_exit(1);
    }
}
