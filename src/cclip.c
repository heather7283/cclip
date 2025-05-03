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
#include <inttypes.h>
#include <limits.h>

#include "common.h"
#include "xmalloc.h"

unsigned int DEBUG_LEVEL = 0;

const char* db_path = NULL;
bool secure_delete = false;

struct sqlite3* db = NULL;

const char* get_default_db_path(void) {
    static char db_path[PATH_MAX];

    char* xdg_data_home = getenv("XDG_DATA_HOME");
    char* home = getenv("HOME");

    if (xdg_data_home != NULL) {
        snprintf(db_path, sizeof(db_path), "%s/cclip/db.sqlite3", xdg_data_home);
    } else if (home != NULL) {
        snprintf(db_path, sizeof(db_path), "%s/.local/share/cclip/db.sqlite3", home);
    } else {
        err("both HOME and XDG_DATA_HOME are unset, unable to determine db file path\n");
        return NULL;
    }

    return db_path;
}

int print_row(void* data, int argc, char** argv, char** column_names) {
    for (int i = 0; i < argc - 1; i++) {
        printf("%s\t", argv[i]);
    }
    printf("%s\n", argv[argc - 1]);
    return 0;
}

int list(char* fields) {
    /* IMPORTANT: EDIT THIS IF YOU EVER ADD MORE THAN 3 ALIASES */
    static const char* allowed_fields[][4] = {
        {     "rowid",    "id",         NULL },
        { "timestamp",  "time",         NULL },
        { "mime_type",  "mime", "type", NULL },
        {   "preview",                  NULL },
        { "data_size",  "size",         NULL },
    };
    static const int allowed_fields_count = sizeof(allowed_fields) / sizeof(allowed_fields[0]);

    const char* selected_fields[allowed_fields_count];
    int selected_fields_count = 0;

    if (fields == NULL) {
        selected_fields[0] = "rowid";
        selected_fields[1] = "mime_type";
        selected_fields[2] = "preview";
        selected_fields_count = 3;
    } else {
        char* token = strtok(fields, ",");
        while (token != NULL) {
            if (selected_fields_count >= allowed_fields_count) {
                critical("extra field: %s, up to %d allowed\n", token, allowed_fields_count);
                return 1;
            }

            bool is_valid_token = false;
            for (int i = 0; i < allowed_fields_count; i++) {
                for (int j = 0; allowed_fields[i][j] != NULL; j++) {
                    if (strcmp(token, allowed_fields[i][j]) == 0) {
                        selected_fields[selected_fields_count] = allowed_fields[i][0];
                        selected_fields_count += 1;
                        is_valid_token = true;
                        goto loop_out;
                    }
                }
            } loop_out:


            if (!is_valid_token) {
                critical("invalid field: %s\n", token);
                return 1;
            }

            token = strtok(NULL, ",");
        }
    }

    if (selected_fields_count < 1) {
        critical("no fields selected\n");
        return 1;
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
        critical("sqlite error: %s\n", errmsg);
        return 1;
    }

    return 0;
}

int get(int64_t id) {
    sqlite3_stmt* stmt;
    int retcode;

    const char* sql = "SELECT data FROM history WHERE rowid = ?";

    retcode = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        critical("sqlite error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_bind_int(stmt, 1, id);

    retcode = sqlite3_step(stmt);
    if (retcode == SQLITE_ROW) {
        int data_size = sqlite3_column_bytes(stmt, 0);
        const void* data = sqlite3_column_blob(stmt, 0);

        fwrite(data, 1, data_size, stdout);
    } else if (retcode == SQLITE_DONE) {
        critical("no entry found with specified id\n");
        return 1;
    } else {
        critical("sqlite error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_finalize(stmt);

    return 0;
}

int delete(int64_t id) {
    sqlite3_stmt* stmt;
    int retcode;

    const char* sql = "DELETE FROM history WHERE rowid = ?";

    retcode = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        critical("sqlite error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_bind_int(stmt, 1, id);

    retcode = sqlite3_step(stmt);
    if (retcode == SQLITE_DONE) {
        if (sqlite3_changes(db) == 0) {
            warn("table was not modified, maybe entry with specified id does not exists\n");
        }
    } else {
        critical("sqlite error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_finalize(stmt);

    return 0;
}

int wipe(void) {
    sqlite3_stmt* stmt;
    int retcode;

    const char* sql = "DELETE FROM history";

    retcode = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        critical("sqlite error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    retcode = sqlite3_step(stmt);
    if (retcode != SQLITE_DONE) {
        critical("sqlite error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_finalize(stmt);

    return 0;
}

int vacuum(void) {
    int retcode;
    char* errmsg;

    retcode = sqlite3_exec(db, "VACUUM", NULL, NULL, &errmsg);
    if (retcode != SQLITE_OK) {
        critical("sqlite error: %s\n", errmsg);
        return 1;
    }

    return 0;
}

void print_version_and_exit(void) {
    fprintf(stderr, "cclip version %s, branch %s, commit %s\n",
            CCLIP_GIT_TAG, CCLIP_GIT_BRANCH, CCLIP_GIT_COMMIT_HASH);
    exit(0);
}

void print_help_and_exit(int exit_status) {
    const char* help_string =
        "cclip - command line interface for cclip database\n"
        "\n"
        "usage:\n"
        "    cclip [-sVh] [-d DB_PATH] ACTION ACTION_ARG\n"
        "\n"
        "command line options:\n"
        "    -d DB_PATH    specify path to database file\n"
        "    -s            enable secure_delete pragma\n"
        "    -V            display version and exit\n"
        "    -h            print this help message and exit\n"
        "\n"
        "actions:\n"
        "    list [FIELDS] print list of all saved entries to stdout\n"
        "    get ID        print entry with specified ID to stdout\n"
        "    delete ID     delete entry with specified ID from database\n"
        "    wipe          delete all entries from database\n"
        "    vacuum        repack database into minimal amount of space\n";

    fputs(help_string, stderr);
    exit(exit_status);
}

int parse_command_line(int argc, char** argv) {
    int opt;

    while ((opt = getopt(argc, argv, ":d:sVh")) != -1) {
        switch (opt) {
        case 'd':
            db_path = xstrdup(optarg);
            break;
        case 's':
            secure_delete = true;
            break;
        case 'V':
            print_version_and_exit();
            break;
        case 'h':
            print_help_and_exit(0);
            break;
        case '?':
            critical("unknown option: %c\n", optopt);
            print_help_and_exit(1);
            break;
        case ':':
            critical("missing arg for %c\n", optopt);
            print_help_and_exit(1);
            break;
        default:
            err("error while parsing command line options\n");
            return -1;
        }
    }

    return 0;
}


int main(int argc, char** argv) {
    int exit_status = 0;

    if (parse_command_line(argc, argv) < 0) {
        err("error while parsing command line options\n");
        exit_status = 1;
        goto cleanup;
    };

    if (db_path == NULL) {
        db_path = get_default_db_path();
    }

    int retcode;
    char* errmsg;

    if ((retcode = sqlite3_open(db_path, &db)) != SQLITE_OK) {
        err("sqlite error: %s\n", sqlite3_errstr(retcode));
        exit_status = 1;
        goto cleanup;
    }

    if (secure_delete) {
        retcode = sqlite3_exec(db, "PRAGMA secure_delete = 1", NULL, NULL, &errmsg);
        if (retcode != SQLITE_OK) {
            critical("sqlite error: %s\n", errmsg);
            exit_status = 1;
            goto cleanup;
        }
    }

    if (argv[optind] == NULL) {
        critical("no action provided\n");
        exit_status = 1;
        goto cleanup;
    }
    char* action = argv[optind];

    if (strcmp(action, "list") == 0) {
        char* output_format = NULL;
        if (argv[optind + 1] != NULL) {
            output_format = argv[optind + 1];
        }
        exit_status = list(output_format);
    } else if (strcmp(action, "get") == 0) {
        int64_t id = -1;
        if (argv[optind + 1] == NULL) {
            if (scanf("%" SCNd64 "\n", &id) != 1) {
                critical("no id provided\n");
                exit_status = 1;
                goto cleanup;
            }
        } else {
            id = atoll(argv[optind + 1]);
        }
        if (id <= 0) {
            critical("id should be a positive integer, got %s\n", argv[optind + 1]);
            exit_status = 1;
            goto cleanup;
        }
        exit_status = get(id);
    } else if (strcmp(action, "delete") == 0) {
        int64_t id = -1;
        if (argv[optind + 1] == NULL) {
            if (scanf("%" SCNd64 "\n", &id) != 1) {
                critical("no id provided\n");
                exit_status = 1;
                goto cleanup;
            }
        } else {
            id = atoll(argv[optind + 1]);
        }
        if (id <= 0) {
            critical("id should be a positive integer, got %s\n", argv[optind + 1]);
            exit_status = 1;
            goto cleanup;
        }
        exit_status = delete(id);
    } else if (strcmp(action, "wipe") == 0) {
        exit_status = wipe();
    } else if (strcmp(action, "vacuum") == 0) {
        exit_status = vacuum();
    } else {
        critical("invalid action: %s\n", action);
        exit_status = 1;
        goto cleanup;
    }

cleanup:
    sqlite3_close_v2(db);
    exit(exit_status);
}

