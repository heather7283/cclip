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

#include <stdio.h>

#include "wayland.h"
#include "log.h"
#include "db.h"
#include "sql.h"
#include "config.h"
#include "eventloop.h"
#include "xmalloc.h"
#include "getopt.h"

static void print_version_and_exit(void) {
    fprintf(stderr, "cclipd version %s, branch %s, commit %s\n",
            CCLIP_GIT_TAG, CCLIP_GIT_BRANCH, CCLIP_GIT_COMMIT_HASH);
    exit(0);
}

static void print_help_and_exit(int exit_status) {
    const char* help_string =
        "cclipd - clipboard manager daemon\n"
        "\n"
        "usage:\n"
        "    cclipd [OPTIONS]\n"
        "\n"
        "command line options:\n"
        "    -V             display version and exit\n"
        "    -h             print this help message and exit\n"
        "    -v             increase verbosity\n"
        "    -d DB_PATH     specify path to databse file\n"
        "    -t PATTERN     specify MIME type pattern to accept,\n"
        "                   can be supplied multiple times\n"
        "    -s SIZE        clipboard entry will only be saved if\n"
        "                   its size in bytes is not less than SIZE\n"
        "    -c ENTRIES     max count of entries to keep in database\n"
        "    -P PREVIEW_LEN max length of preview to generate in bytes\n"
        "    -p             also monitor primary selection\n"
        "    -e             error out if database file does not exist\n";

    fputs(help_string, stderr);
    exit(exit_status);
}

static int parse_command_line(int argc, char** argv) {
    int opt;

    while ((opt = getopt(argc, argv, ":d:t:s:c:P:pevVh")) != -1) {
        switch (opt) {
        case 'd':
            config.db_path = optarg;
            break;
        case 't':
            VEC_APPEND(&config.accepted_mime_types, &(char *){ xstrdup(optarg) });
            break;
        case 's':
            config.min_data_size = atoi(optarg);
            if (config.min_data_size < 1) {
                log_print(ERR, "MINSIZE must be a positive integer, got %s", optarg);
                return -1;
            }
            break;
        case 'c':
            config.max_entries_count = atoi(optarg);
            if (config.max_entries_count < 1) {
                log_print(ERR, "ENTRIES must be a positive integer, got %s", optarg);
                return -1;
            }
            break;
        case 'P':
            config.preview_len = atoi(optarg);
            if (config.preview_len < 1) {
                log_print(ERR, "PREVIEW_LEN must be a positive integer, got %s", optarg);
                return -1;
            }
            break;
        case 'p':
            config.primary_selection = true;
            break;
        case 'e':
            config.create_db_if_not_exists = false;
            break;
        case 'v':
            config.loglevel += 1;
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
            log_print(ERR, "error while parsing command line options");
            return -1;
        }
    }

    return 0;
}

static int on_sigint_sigterm(struct pollen_event_source* src, int sig, void* data) {
    pollen_loop_quit(eventloop, 0);
    return 0;
}

static int on_sigusr1(struct pollen_event_source* src, int sig, void* data) {
    struct sqlite3** pdb = data;

    log_print(INFO, "received SIGUSR1, closing and reopening db connection");
    stop_db_thread();
    db_close(*pdb);

    if ((*pdb = db_open(config.db_path, config.create_db_if_not_exists)) == NULL) {
        log_print(ERR, "failed to reopen database");
        return -1;
    };
    if (!start_db_thread(*pdb)) {
        log_print(ERR, "failed to start db thread");
        return -1;
    }

    return 0;
}

static int on_wayland_events(struct pollen_event_source* src, int fd, uint32_t ev, void* data) {
    if (wayland_process_events() < 0) {
        log_print(ERR, "failed to process wayland events");
        return -1;
    }

    return 0;
}

int main(int argc, char** argv) {
    struct sqlite3* db = NULL;
    int wayland_fd = -1;
    int exit_status = 0;

    log_init(2 /* stderr */, ERR);

    if (parse_command_line(argc, argv) < 0) {
        log_print(ERR, "error while parsing command line options");
        exit_status = 1;
        goto cleanup;
    };

    log_init(2 /* stderr */, config.loglevel);

    if (VEC_SIZE(&config.accepted_mime_types) == 0) {
        VEC_APPEND(&config.accepted_mime_types, &(char *){ "*" });
    }

    db = db_open(config.db_path, config.create_db_if_not_exists);
    if (db == NULL) {
        log_print(ERR, "failed to open database");
        exit_status = 1;
        goto cleanup;
    };

    const int32_t user_version = db_get_user_version(db);
    if (user_version == 0) {
        log_print(INFO, "db schema version is 0, initialising empty database");
        if (!db_init(db)) {
            log_print(ERR, "failed to initialise database!");
            exit_status = 1;
            goto cleanup;
        }
    } else if (user_version < DB_USER_SCHEMA_VERSION) {
        log_print(INFO, "db schema version is %d (%d expected), migrating",
                  user_version, DB_USER_SCHEMA_VERSION);
        if (!db_migrate(db, user_version, DB_USER_SCHEMA_VERSION)) {
            log_print(ERR, "failed to perform migration");
            exit_status = 1;
            goto cleanup;
        }
    } else if (user_version > DB_USER_SCHEMA_VERSION) {
        log_print(ERR, "db schema version is %d which is more than "
                  "the maximum version this build of cclipd supports (%d)",
                  user_version, DB_USER_SCHEMA_VERSION);
        exit_status = 1;
        goto cleanup;
    } else /* if (user_version == DB_USER_SCHEMA_VERSION) */ {
        log_print(INFO, "opened database version %d", user_version);
    }

    eventloop = pollen_loop_create();
    if (eventloop == NULL) {
        exit_status = 1;
        goto cleanup;
    }

    pollen_loop_add_signal(eventloop, SIGINT, on_sigint_sigterm, NULL);
    pollen_loop_add_signal(eventloop, SIGTERM, on_sigint_sigterm, NULL);

    pollen_loop_add_signal(eventloop, SIGUSR1, on_sigusr1, &db);

    /* important to start db thread after blocking signals */
    if (!start_db_thread(db)) {
        log_print(ERR, "failed to start db thread");
        exit_status = 1;
        goto cleanup;
    }

    wayland_fd = wayland_init();
    if (wayland_fd < 0) {
        log_print(ERR, "failed to init wayland stuff");
        exit_status = 1;
        goto cleanup;
    };

    pollen_loop_add_fd(eventloop, wayland_fd, EPOLLIN, false, on_wayland_events, NULL);

    exit_status = pollen_loop_run(eventloop);

cleanup:
    stop_db_thread();
    db_close(db);

    wayland_cleanup();

    return exit_status;
}

