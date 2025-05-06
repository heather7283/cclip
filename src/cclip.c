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
#include <stdlib.h>
#include <errno.h>

#include "log.h"
#include "xmalloc.h"

unsigned int DEBUG_LEVEL = 0;

const char* db_path = NULL;
bool secure_delete = false;

struct sqlite3* db = NULL;

bool str_to_int64(const char* str, int64_t* res) {
    char *endptr = NULL;

    errno = 0;
    int64_t res_tmp = strtoll(str, &endptr, 10);

    if (errno == 0 && *endptr == '\0') {
        *res = res_tmp;
        return true;
    }
    log_print(ERR, "failed to convert %s to int64\n", str);
    return false;
}

bool int64_from_stdin(int64_t* res) {
    int64_t res_tmp;
    if (scanf("%ld", &res_tmp) != 1) {
        log_print(ERR, "failed to read a number from stdin\n");
        return false;
    };
    *res = res_tmp;
    return true;
}

/* if str is null, tries to read stdin */
bool get_id(const char* str, int64_t* res) {
    if (str == NULL) {
        return int64_from_stdin(res);
    } else {
        return str_to_int64(str, res);
    }
}

const char* get_default_db_path(void) {
    static char db_path[PATH_MAX];

    char* xdg_data_home = getenv("XDG_DATA_HOME");
    char* home = getenv("HOME");

    if (xdg_data_home != NULL) {
        snprintf(db_path, sizeof(db_path), "%s/cclip/db.sqlite3", xdg_data_home);
    } else if (home != NULL) {
        snprintf(db_path, sizeof(db_path), "%s/.local/share/cclip/db.sqlite3", home);
    } else {
        log_print(ERR, "both HOME and XDG_DATA_HOME are unset, unable to determine db file path\n");
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

int action_list(char* fields) {
    /* IMPORTANT: EDIT THIS IF YOU EVER ADD MORE THAN 3 ALIASES */
    const char* allowed_fields[][4] = {
        {     "rowid",    "id",         NULL },
        { "timestamp",  "time",         NULL },
        { "mime_type",  "mime", "type", NULL },
        {   "preview",                  NULL },
        { "data_size",  "size",         NULL },
    };
    const int allowed_fields_count = sizeof(allowed_fields) / sizeof(allowed_fields[0]);

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
                log_print(ERR, "extra field: %s, up to %d allowed", token, allowed_fields_count);
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
                log_print(ERR, "invalid field: %s", token);
                return 1;
            }

            token = strtok(NULL, ",");
        }
    }

    if (selected_fields_count < 1) {
        log_print(ERR, "no fields selected");
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
        log_print(ERR, "sqlite error: %s", errmsg);
        return 1;
    }

    return 0;
}

int action_get(int64_t id) {
    sqlite3_stmt* stmt;
    int retcode;

    const char* sql = "SELECT data FROM history WHERE rowid = ?";

    retcode = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_bind_int(stmt, 1, id);

    retcode = sqlite3_step(stmt);
    if (retcode == SQLITE_ROW) {
        int data_size = sqlite3_column_bytes(stmt, 0);
        const void* data = sqlite3_column_blob(stmt, 0);

        fwrite(data, 1, data_size, stdout);
    } else if (retcode == SQLITE_DONE) {
        log_print(ERR, "no entry found with id %li", id);
        return 1;
    } else {
        log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_finalize(stmt);

    return 0;
}

int action_delete(int64_t id) {
    sqlite3_stmt* stmt;
    int retcode;

    const char* sql = "DELETE FROM history WHERE rowid = ?";

    retcode = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_bind_int(stmt, 1, id);

    retcode = sqlite3_step(stmt);
    if (retcode == SQLITE_DONE) {
        if (sqlite3_changes(db) == 0) {
            log_print(WARN, "table was not modified, maybe entry with specified id does not exists");
        }
    } else {
        log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_finalize(stmt);

    return 0;
}

int action_wipe(void) {
    sqlite3_stmt* stmt;
    int retcode;

    const char* sql = "DELETE FROM history";

    retcode = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
        return 1;
    }

    retcode = sqlite3_step(stmt);
    if (retcode != SQLITE_DONE) {
        log_print(ERR, "sqlite error: %s", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_finalize(stmt);

    return 0;
}

int action_vacuum(void) {
    int retcode;
    char* errmsg;

    retcode = sqlite3_exec(db, "VACUUM", NULL, NULL, &errmsg);
    if (retcode != SQLITE_OK) {
        log_print(ERR, "sqlite error: %s", errmsg);
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
            log_print(ERR, "unknown option: %c", optopt);
            print_help_and_exit(1);
            break;
        case ':':
            log_print(ERR, "missing arg for %c", optopt);
            print_help_and_exit(1);
            break;
        default:
            log_print(ERR, "error while parsing command line options\n");
            return -1;
        }
    }

    return 0;
}


int main(int argc, char** argv) {
    int exit_status = 0;

    log_init(stderr, LOGLEVEL_MAX);

    if (parse_command_line(argc, argv) < 0) {
        log_print(ERR, "error while parsing command line options\n");
        exit_status = 1;
        goto cleanup;
    };

    if (db_path == NULL) {
        db_path = get_default_db_path();
    }

    int retcode;
    char* errmsg;

    if ((retcode = sqlite3_open(db_path, &db)) != SQLITE_OK) {
        log_print(ERR, "sqlite error: %s\n", sqlite3_errstr(retcode));
        exit_status = 1;
        goto cleanup;
    }

    if (secure_delete) {
        retcode = sqlite3_exec(db, "PRAGMA secure_delete = 1", NULL, NULL, &errmsg);
        if (retcode != SQLITE_OK) {
            log_print(ERR, "sqlite error: %s", errmsg);
            exit_status = 1;
            goto cleanup;
        }
    }

    if (argv[optind] == NULL) {
        log_print(ERR, "no action provided");
        exit_status = 1;
        goto cleanup;
    }
    char* action = argv[optind++];

    if (strcmp(action, "list") == 0) {
        char* output_format = NULL;
        if (argv[optind] != NULL) {
            output_format = argv[optind];
        }
        exit_status = action_list(output_format);
    } else if (strcmp(action, "get") == 0) {
        int64_t id = -1;
        if (!get_id(argv[optind], &id)) {
            log_print(ERR, "failed to get id");
            exit_status = 1;
            goto cleanup;
        }
        exit_status = action_get(id);
    } else if (strcmp(action, "delete") == 0) {
        int64_t id = -1;
        if (!get_id(argv[optind], &id)) {
            log_print(ERR, "failed to get id");
            exit_status = 1;
            goto cleanup;
        }
        exit_status = action_delete(id);
    } else if (strcmp(action, "wipe") == 0) {
        exit_status = action_wipe();
    } else if (strcmp(action, "vacuum") == 0) {
        exit_status = action_vacuum();
    } else {
        log_print(ERR, "invalid action: %s", action);
        exit_status = 1;
        goto cleanup;
    }

cleanup:
    sqlite3_close_v2(db);
    exit(exit_status);
}

