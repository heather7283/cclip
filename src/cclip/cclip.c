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

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include <sqlite3.h>

#include "xmalloc.h"
#include "db.h"
#include "getopt.h"
#include "macros.h"
#include "log.h"

#define FOR_LIST_OF_ACTIONS(DO) \
    DO(delete) \
    DO(get) \
    DO(list) \
    DO(tag) \
    DO(vacuum) \
    DO(wipe) \

typedef int (action_func_t)(int argc, char** argv, struct sqlite3* db);

#define DEFINE_ACTION_FUNCTION(name, ...) action_func_t action_##name;
FOR_LIST_OF_ACTIONS(DEFINE_ACTION_FUNCTION)

static const struct {
    const char* const name;
    action_func_t* const action;
} actions[] = {
    #define DEFINE_ACTION_TABLE(name, ...) { #name, action_##name },
    FOR_LIST_OF_ACTIONS(DEFINE_ACTION_TABLE)
};

static action_func_t* match_action(const char* input) {
    for (size_t i = 0; i < SIZEOF_ARRAY(actions); i++) {
        if (STREQ(input, actions[i].name)) {
            return actions[i].action;
        }
    }

    return NULL;
}

void print_version_and_exit(void) {
    fprintf(stderr, "cclip version %s, branch %s, commit %s\n",
            CCLIP_GIT_TAG, CCLIP_GIT_BRANCH, CCLIP_GIT_COMMIT_HASH);
    exit(0);
}

void print_help_and_exit(FILE *stream, int rc) {
    const char* help =
        "cclip - command line interface for cclip database\n"
        "\n"
        "Usage:\n"
        "    cclip [-Vh] [-d DB_PATH] ACTION ACTION_ARGS\n"
        "\n"
        "Command line options:\n"
        "    -d DB_PATH    specify path to database file\n"
        "    -V            display version and exit\n"
        "    -h            print this help message and exit\n"
        "\n"
        "Available actions (pass -h after action to see detailed help):\n"
        #define DEFINE_ACTION_STRING(name, ...) "    "#name"\n"
        FOR_LIST_OF_ACTIONS(DEFINE_ACTION_STRING)
    ;

    fprintf(stream, "%s", help);
    exit(rc);
}


int main(int argc, char** argv) {
    int exit_status = 0;
    const char* db_path = NULL;
    enum loglevel loglevel = WARN;
    struct sqlite3* db = NULL;

    int opt;
    while ((opt = getopt(argc, argv, ":d:vVh")) != -1) {
        switch (opt) {
        case 'd':
            db_path = xstrdup(optarg);
            break;
        case 'v':
            loglevel += 1;
            break;
        case 'V':
            print_version_and_exit();
            break;
        case 'h':
            print_help_and_exit(stdout, 0);
            break;
        case '?':
            log_print(ERR, "unknown option: %c", optopt);
            break;
        case ':':
            log_print(ERR, "missing arg for %c", optopt);
            break;
        default:
            log_print(ERR, "error while parsing command line options");
            break;
        }
    }

    log_init(stderr, loglevel);

    argc = argc - optind;
    argv = &argv[optind];
    if (argc < 1) {
        log_print(ERR, "no action provided");
        exit_status = 1;
        goto cleanup;
    }

    db = db_open(db_path, false);
    if (db == NULL) {
        log_print(ERR, "failed to open database");
        exit_status = 1;
        goto cleanup;
    };

    const int user_version = db_get_user_version(db);
    if (user_version < DB_USER_SCHEMA_VERSION) {
        log_print(ERR, "db version %d is older than the version this cclip can work with (%d)",
                  user_version, DB_USER_SCHEMA_VERSION);
        exit_status = 1;
        goto cleanup;
    } else if (user_version > DB_USER_SCHEMA_VERSION) {
        log_print(ERR, "db version %d is newer than the version this cclip can work with (%d)",
                  user_version, DB_USER_SCHEMA_VERSION);
        exit_status = 1;
        goto cleanup;
    }

    action_func_t* action = match_action(argv[0]);
    if (action == NULL) {
        log_print(ERR, "invalid action: %s", argv[0]);
        exit_status = 1;
        goto cleanup;
    }

    exit_status = action(argc, argv, db);

cleanup:
    db_close(db);
    exit(exit_status);
}

