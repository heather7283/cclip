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

#include "cclip.h"
#include "action_list.h"
#include "action_get.h"
#include "action_tag.h"
#include "action_delete.h"
#include "action_wipe.h"
#include "action_vacuum.h"
#include "xmalloc.h"
#include "db_path.h"
#include "getopt.h"

struct sqlite3* db = NULL;

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
        "Actions (pass -h after action to see detailed help):\n"
        "    list [-t] [FIELDS]\n"
        "    get ID [FIELDS]\n"
        "    delete [-s] ID\n"
        "    tag ID TAG | tag -d ID\n"
        "    wipe [-ts]\n"
        "    vacuum\n"
    ;

    fprintf(stream, "%s", help);
    exit(rc);
}


int main(int argc, char** argv) {
    int exit_status = 0;
    const char* db_path = NULL;

    int opt;
    while ((opt = getopt(argc, argv, ":d:Vh")) != -1) {
        switch (opt) {
        case 'd':
            db_path = xstrdup(optarg);
            break;
        case 'V':
            print_version_and_exit();
            break;
        case 'h':
            print_help_and_exit(stdout, 0);
            break;
        case '?':
            fprintf(stderr, "unknown option: %c\n\n", optopt);
            print_help_and_exit(stderr, 1);
            break;
        case ':':
            fprintf(stderr, "missing arg for %c\n\n", optopt);
            print_help_and_exit(stderr, 1);
            break;
        default:
            fprintf(stderr, "error while parsing command line options\n\n");
            print_help_and_exit(stderr, 1);
            break;
        }
    }
    argc = argc - optind;
    argv = &argv[optind];
    if (argc < 1) {
        fprintf(stderr, "no action provided\n\n");
        print_help_and_exit(stderr, 1);
    }

    if (db_path == NULL) {
        db_path = get_default_db_path();
    }
    if (db_path == NULL) {
        fprintf(stderr, "failed to determine db path, both HOME and XDG_DATA_HOME are unset\n");
        exit_status = 1;
        goto cleanup;
    }

    int ret;
    if ((ret = sqlite3_open(db_path, &db)) != SQLITE_OK) {
        fprintf(stderr, "sqlite error: %s\n", sqlite3_errstr(ret));
        exit_status = 1;
        goto cleanup;
    }

    if (strcmp(argv[0], "list") == 0) {
        exit_status = action_list(argc, argv);
    } else if (strcmp(argv[0], "get") == 0) {
        exit_status = action_get(argc, argv);
    } else if (strcmp(argv[0], "delete") == 0) {
        exit_status = action_delete(argc, argv);
    } else if (strcmp(argv[0], "tag") == 0) {
        exit_status = action_tag(argc, argv);
    } else if (strcmp(argv[0], "wipe") == 0) {
        exit_status = action_wipe(argc, argv);
    } else if (strcmp(argv[0], "vacuum") == 0) {
        exit_status = action_vacuum(argc, argv);
    } else {
        fprintf(stderr, "invalid action: %s\n", argv[0]);
        exit_status = 1;
        goto cleanup;
    }

cleanup:
    sqlite3_close_v2(db);
    exit(exit_status);
}

