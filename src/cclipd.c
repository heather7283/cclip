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
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "wayland.h"
#include "common.h"
#include "db.h"
#include "config.h"
#include "xmalloc.h"

#define EPOLL_MAX_EVENTS 16

unsigned int DEBUG_LEVEL = 0;

void print_version_and_exit(void) {
    fprintf(stderr, "cclipd version %s, branch %s, commit %s\n",
            CCLIP_GIT_TAG, CCLIP_GIT_BRANCH, CCLIP_GIT_COMMIT_HASH);
    exit(0);
}

void print_help_and_exit(int exit_status) {
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

int parse_command_line(int argc, char** argv) {
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
                err("MINSIZE must be a positive integer, got %s\n", optarg);
                return -1;
            }
            break;
        case 'c':
            config.max_entries_count = atoi(optarg);
            if (config.max_entries_count < 1) {
                err("ENTRIES must be a positive integer, got %s\n", optarg);
                return -1;
            }
            break;
        case 'P':
            config.preview_len = atoi(optarg);
            if (config.preview_len < 1) {
                err("PREVIEW_LEN must be a positive integer, got %s\n", optarg);
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
            DEBUG_LEVEL += 1;
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
    int epoll_fd = -1;
    int signal_fd = -1;
    int wayland_fd = -1;

    int exit_status = 0;

    if (parse_command_line(argc, argv) < 0) {
        err("error while parsing command line options\n");
        exit_status = 1;
        goto cleanup;
    };

    if (config.db_path == NULL) {
        config.db_path = get_default_db_path();
    }
    if (config.db_path == NULL) {
        exit_status = 1;
        goto cleanup;
    }

    if (config.accepted_mime_types_count == 0) {
        config.accepted_mime_types[0] = xstrdup("*");
        config.accepted_mime_types_count = 1;
    }

    if (db_init(config.db_path, config.create_db_if_not_exists) < 0) {
        critical("failed to init database\n");
        exit_status = 1;
        goto cleanup;
    };

    wayland_fd = wayland_init();
    if (wayland_fd < 0) {
        critical("failed to init wayland stuff\n");
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
        critical("failed to block signals: %s\n", strerror(errno));
        exit_status = 1;
        goto cleanup;
    }

    /* set up signalfd */
    signal_fd = signalfd(-1, &mask, 0);
    if (signal_fd == -1) {
        critical("failed to set up signalfd: %s\n", strerror(errno));
        exit_status = 1;
        goto cleanup;
    }

    /* set up epoll */
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        critical("failed to set up epoll: %s\n", strerror(errno));
        exit_status = 1;
        goto cleanup;
    }

    struct epoll_event epoll_event;
    /* add wayland fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = wayland_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, wayland_fd, &epoll_event) == -1) {
        critical("failed to add wayland fd to epoll list: %s\n", strerror(errno));
        exit_status = 1;
        goto cleanup;
    }
    /* add signal fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = signal_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &epoll_event) == -1) {
        critical("failed to add signal fd to epoll list: %s\n", strerror(errno));
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
            critical("epoll_wait error: %s\n", strerror(errno));
            exit_status = 1;
            goto cleanup;
        }

        /* handle events */
        for (int n = 0; n < number_fds; n++) {
            if (events[n].data.fd == wayland_fd) {
                /* wayland events */
                if (wayland_process_events() < 0) {
                    critical("failed to process wayland events\n");
                    exit_status = 1;
                    goto cleanup;
                }
            } else if (events[n].data.fd == signal_fd) {
                /* signals */
                struct signalfd_siginfo siginfo;
                ssize_t bytes_read = read(signal_fd, &siginfo, sizeof(siginfo));
                if (bytes_read != sizeof(siginfo)) {
                    critical("failed to read signalfd_siginfo from signal_fd\n");
                    exit_status = 1;
                    goto cleanup;
                }

                uint32_t signo = siginfo.ssi_signo;
                switch (signo) {
                case SIGINT:
                case SIGTERM:
                    info("received signal %d, exiting\n", signo);
                    goto cleanup;
                case SIGUSR1:
                    info("received SIGUSR1, closing and reopening db connection\n");
                    if (db_cleanup() < 0) {
                        err("failed to deinit db\n");
                        exit_status = 1;
                        goto cleanup;
                    };
                    if (db_init(config.db_path, config.create_db_if_not_exists) < 0) {
                        err("failed to reinit db\n");
                        exit_status = 1;
                        goto cleanup;
                    };
                    break;
                }
            }
        }
    }
cleanup:
    db_cleanup();
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

