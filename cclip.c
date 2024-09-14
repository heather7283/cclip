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
#include <sqlite3.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "common.h"
#include "config.h"
#include "db.h"

#define LIST_MAX_SELECTED_FIELDS 5
const char* list_allowed_fields[LIST_MAX_SELECTED_FIELDS] = {
    "rowid", "timestamp", "mime_type", "preview", "data_size"
};

int argc;
char** argv;
char* prog_name;

int print_row(void* data, int argc, char** argv, char** column_names) {
    UNUSED(data);
    UNUSED(column_names);

    for (int i = 0; i < argc; i++) {
        printf("%s\t", argv[i]);
    }
    printf("\n");
    return 0;
}

void list(char* fields) {
    char* selected_fields[LIST_MAX_SELECTED_FIELDS];
    int selected_fields_count = 0;

    if (fields == NULL) {
        selected_fields[0] = "rowid";
        selected_fields[1] = "mime_type";
        selected_fields[2] = "preview";
        selected_fields_count = 3;
    } else {
        char* token = strtok(fields, ",");
        while (token != NULL && selected_fields_count < LIST_MAX_SELECTED_FIELDS) {
            bool is_valid_token = false;
            for (int i = 0; i < LIST_MAX_SELECTED_FIELDS; i++) {
                if (strcmp(token, list_allowed_fields[i]) == 0) {
                    selected_fields[selected_fields_count] = token;
                    selected_fields_count += 1;
                    is_valid_token = true;
                    break;
                }
            }

            if (!is_valid_token) {
                die("invalid field: %s\n", token);
            }

            token = strtok(NULL, ",");
        }
    }

    if (selected_fields_count < 1) {
        die("no fields selected\n");
    }

    char* errmsg = NULL;
    int retcode;

    char sql[256] = "SELECT ";
    for (int i = 0; i < selected_fields_count; i++) {
        strcat(sql, selected_fields[i]);
        if (i < selected_fields_count - 1) {
            strcat(sql, ",");
        }
    }
    strcat(sql, " FROM history ORDER BY timestamp DESC");

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
        die("no entry found with specified id\n");
    } else {
        die("sqlite error: %s\n", sqlite3_errmsg(db));
    }
}

void print_help_and_exit(int exit_status) {
    const char* help_string =
        "cclip - command line interface for cclipd database\n"
        "\n"
        "usage:\n"
        "    %s get|delete id\n"
        "    %s list [rowid,timestamp,mime_type,preview,data_size]\n"
        "    %s wipe\n"
        "    %s help\n"
        "\n"
        "actions description:\n"
        "    list      print list of all saved entries to stdout\n"
        "              up to %d comma-separated field names are accepted\n"
        "    get       print entry with specified id to stdout\n"
        "    delete    delete entry with specified id (not implemented)\n"
        "    wipe      delete all entries from database (not implemented)\n"
        "    help      display this help message\n";

    fprintf(stderr, help_string,
            prog_name, prog_name, prog_name, prog_name,
            LIST_MAX_SELECTED_FIELDS);
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
        if (argc < 3) {
            list(NULL);
        } else {
            list(argv[2]);
        }
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
