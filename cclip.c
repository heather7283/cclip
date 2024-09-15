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
#include <unistd.h>

#include "common.h"
#include "config.h"
#include "db.h"

#ifndef CCLIP_VERSION
#define CCLIP_VERSION "uknown_version"
#endif

#define LIST_MAX_SELECTED_FIELDS 5
const char* list_allowed_fields[LIST_MAX_SELECTED_FIELDS] = {
    "rowid", "timestamp", "mime_type", "preview", "data_size"
};

int argc;
char** argv;
char* prog_name;

char* db_path = NULL;

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

void delete(int64_t id) {
    sqlite3_stmt* stmt;
    int retcode;

    const char* sql = "DELETE FROM history WHERE rowid = ?";

    retcode = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        die("sqlite error: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_bind_int(stmt, 1, id);

    retcode = sqlite3_step(stmt);
    if (retcode == SQLITE_DONE) {
        if (sqlite3_changes(db) == 0) {
            warn("table was not modified, maybe entry with specified id does not exists\n");
        }
    } else {
        die("sqlite error: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
}

void print_version_and_exit(void) {
    fprintf(stderr, "cclip version %s\n", CCLIP_VERSION);
    exit(0);
}

void print_help_and_exit(int exit_status) {
    const char* help_string =
        "cclip - command line interface for cclip database\n"
        "\n"
        "usage:\n"
        "    cclip [-Vh] [-d DB_PATH] ACTION ACTION_ARG\n"
        "\n"
        "command line options:\n"
        "    -d DB_PATH    specify path to database file\n"
        "    -V            display version and exit\n"
        "    -h            print this help message and exit\n"
        "\n"
        "actions:\n"
        "    list [FIELDS] print list of all saved entries to stdout\n"
        "    get ID        print entry with specified ID to stdout\n"
        "    remove ID     delete entry with specified id (NOT IMPLEMENTED)\n"
        "    wipe          delete all entries from database (NOT IMPLEMENTED)\n";

    fprintf(stderr, help_string, LIST_MAX_SELECTED_FIELDS);
    exit(exit_status);
}

void parse_command_line(void) {
    int opt;

    while ((opt = getopt(argc, argv, ":d:Vh")) != -1) {
        switch (opt) {
        case 'd':
            db_path = strdup(optarg);
            break;
        case 'V':
            print_version_and_exit();
            break;
        case 'h':
            print_help_and_exit(0);
            break;
        case '?':
            critical("unknown option: %c\n", optopt);
            break;
        case ':':
            critical("missing arg for %c\n", optopt);
            break;
        default:
            die("error while parsing command line options\n");
        }
    }
}


int main(int _argc, char** _argv) {
    argc = _argc;
    argv = _argv;
    prog_name = argc > 0 ? argv[0] : "cclip";

    parse_command_line();

    if (db_path == NULL) {
        db_path = get_default_db_path();
    }
    db_init(db_path, false);

    if (argv[optind] == NULL) {
        die("no action provided\n");
    }
    char* action = argv[optind];

    if (strcmp(action, "list") == 0) {
        char* output_format = NULL;
        if (argv[optind + 1] != NULL) {
            output_format = argv[optind + 1];
        }
        list(output_format);
    } else if (strcmp(action, "get") == 0) {
        if (argv[optind + 1] == NULL) {
            die("no id provided\n");
        }
        int64_t id = atoll(argv[optind + 1]);
        if (id <= 0) {
            die("id should be a positive integer, got %s\n", argv[optind + 1]);
        }
        get(id);
    } else if (strcmp(action, "delete") == 0) {
        if (argv[optind + 1] == NULL) {
            die("no id provided\n");
        }
        int64_t id = atoll(argv[optind + 1]);
        if (id <= 0) {
            die("id should be a positive integer, got %s\n", argv[optind + 1]);
        }
        delete(id);
    } else if (strcmp(action, "wipe") == 0) {
        die("wipe not implemented yet\n");
    } else {
        die("invalid action: %s\n", action);
    }
}
