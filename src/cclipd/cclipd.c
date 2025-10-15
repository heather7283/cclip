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

#include <sys/signalfd.h>
#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>

#include "cclipd.h"
#include "wayland.h"
#include "log.h"
#include "db_utils.h"
#include "sql.h"
#include "config.h"
#include "xmalloc.h"
#include "db_path.h"
#include "getopt.h"

struct sqlite3* db = NULL;

#define EPOLL_MAX_EVENTS 16

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
            config.accepted_mime_types[config.accepted_mime_types_count] = xstrdup(optarg);
            config.accepted_mime_types_count += 1;
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

static bool reopen_database(void) {
    cleanup_statements();
    db_close(db);

    db = db_open(config.db_path, config.create_db_if_not_exists);
    if (db == NULL) {
        return false;
    }
    return prepare_statements();
}

int main(int argc, char** argv) {
    int epoll_fd = -1;
    int signal_fd = -1;
    int wayland_fd = -1;

    int exit_status = 0;

    log_init(stderr, ERR);

    if (parse_command_line(argc, argv) < 0) {
        log_print(ERR, "error while parsing command line options");
        exit_status = 1;
        goto cleanup;
    };

    log_init(stderr, config.loglevel);

    if (config.db_path == NULL) {
        config.db_path = get_default_db_path();
    }
    if (config.db_path == NULL) {
        log_print(ERR, "failed to determine db file path, both HOME and XDG_DATA_HOME are unset");
        exit_status = 1;
        goto cleanup;
    }

    if (config.accepted_mime_types_count == 0) {
        config.accepted_mime_types[0] = xstrdup("*");
        config.accepted_mime_types_count = 1;
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

    if (!prepare_statements()) {
        log_print(ERR, "failed to prepare sql statements");
        exit_status = 1;
        goto cleanup;
    }

    wayland_fd = wayland_init();
    if (wayland_fd < 0) {
        log_print(ERR, "failed to init wayland stuff");
        exit_status = 1;
        goto cleanup;
    };

    /* block signals so we can catch them later */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        log_print(ERR, "failed to block signals: %s", strerror(errno));
        exit_status = 1;
        goto cleanup;
    }

    /* set up signalfd */
    signal_fd = signalfd(-1, &mask, 0);
    if (signal_fd == -1) {
        log_print(ERR, "failed to set up signalfd: %s", strerror(errno));
        exit_status = 1;
        goto cleanup;
    }

    /* set up epoll */
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        log_print(ERR, "failed to set up epoll: %s", strerror(errno));
        exit_status = 1;
        goto cleanup;
    }

    struct epoll_event epoll_event;
    /* add wayland fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = wayland_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, wayland_fd, &epoll_event) == -1) {
        log_print(ERR, "failed to add wayland fd to epoll list: %s", strerror(errno));
        exit_status = 1;
        goto cleanup;
    }
    /* add signal fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = signal_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &epoll_event) == -1) {
        log_print(ERR, "failed to add signal fd to epoll list: %s", strerror(errno));
        exit_status = 1;
        goto cleanup;
    }

    int number_fds = -1;
    struct epoll_event events[EPOLL_MAX_EVENTS];
    while (true) {
        /* main event loop */
        do {
            number_fds = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, -1);
        } while (number_fds == -1 && errno == EINTR); /* epoll_wait failing with EINTR is normal */

        if (number_fds == -1) {
            log_print(ERR, "epoll_wait error: %s", strerror(errno));
            exit_status = 1;
            goto cleanup;
        }

        /* handle events */
        for (int n = 0; n < number_fds; n++) {
            if (events[n].data.fd == wayland_fd) {
                /* wayland events */
                if (wayland_process_events() < 0) {
                    log_print(ERR, "failed to process wayland events");
                    exit_status = 1;
                    goto cleanup;
                }
            } else if (events[n].data.fd == signal_fd) {
                /* signals */
                struct signalfd_siginfo siginfo;
                ssize_t bytes_read = read(signal_fd, &siginfo, sizeof(siginfo));
                if (bytes_read != sizeof(siginfo)) {
                    log_print(ERR, "failed to read signalfd_siginfo from signal_fd");
                    exit_status = 1;
                    goto cleanup;
                }

                uint32_t signo = siginfo.ssi_signo;
                switch (signo) {
                case SIGINT:
                case SIGTERM:
                    log_print(INFO, "received signal %d, exiting", signo);
                    goto cleanup;
                case SIGUSR1:
                    log_print(INFO, "received SIGUSR1, closing and reopening db connection");
                    if (!reopen_database()) {
                        log_print(ERR, "failed to reopen database");
                        exit_status = 1;
                        goto cleanup;
                    };
                    break;
                }
            }
        }
    }
cleanup:
    cleanup_statements();
    db_close(db);

    wayland_cleanup();

    if (signal_fd > 0) {
        close(signal_fd);
    }
    if (epoll_fd > 0) {
        close(epoll_fd);
    }

    /* some unnecessary frees to make valgrind shut up */
    for (int i = 0; i < config.accepted_mime_types_count; i++) {
        free(config.accepted_mime_types[i]);
    }

    exit(exit_status);
}

