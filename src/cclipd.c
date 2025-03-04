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
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fnmatch.h>
#include <signal.h>
#include <errno.h>

#include <wlr-data-control-unstable-v1-client-protocol.h>
#include "wayland.h"
#include "common.h"
#include "db.h"
#include "preview.h"
#include "xmalloc.h"

#define EPOLL_MAX_EVENTS 16

#ifndef CCLIP_VERSION
#define CCLIP_VERSION "uknown_version"
#endif

unsigned int DEBUG_LEVEL = 0;

int argc;
char** argv;
char* prog_name;

struct zwlr_data_control_offer_v1* offer = NULL;

/* surely nobody will offer more than 32 mime types */
#define MAX_OFFERED_MIME_TYPES 32
#define MAX_MIME_TYPE_LEN 256
char offered_mime_types[MAX_OFFERED_MIME_TYPES][MAX_MIME_TYPE_LEN];
int offered_mime_types_count = 0;

#define MAX_ACCEPTED_MIME_TYPES 32
struct {
    int accepted_mime_types_count;
    char* accepted_mime_types[MAX_ACCEPTED_MIME_TYPES];
    size_t min_data_size;
    const char* db_path;
    bool primary_selection;
    int max_entries_count;
    bool create_db_if_not_exists;
    size_t preview_len;
} config = {
    .accepted_mime_types_count = 0,
    .accepted_mime_types = {0},
    .min_data_size = 1,
    .db_path = NULL,
    .primary_selection = false,
    .max_entries_count = 1000,
    .create_db_if_not_exists = true,
    .preview_len = 128,
};

char* pick_mime_type(void) {
    /*
     * finds first offered mime type that matches
     * or returns NULL if none matched
     * yes it is O(n^2) I do not care
     */
    for (int i = 0; i < config.accepted_mime_types_count; i++) {
        for (int j = 0; j < offered_mime_types_count; j++) {
            char* pattern = config.accepted_mime_types[i];
            char* type = offered_mime_types[j];

            if (fnmatch(pattern, type, 0) == 0) {
                debug("selected mime type: %s\n", type);
                return type;
            }
        }
    }
    return NULL;
}

size_t receive_data(char** buffer, char* mime_type) {
    /* reads offer into buffer, returns number of bytes read */
    trace("start receiving offer...\n");

    int pipes[2];
    if (pipe(pipes) == -1) {
        die("failed to create pipe\n");
    }

    zwlr_data_control_offer_v1_receive(offer, mime_type, pipes[1]);
    /* AFTER THIS LINE offer IS NO LONGER VALID!!! */
    wl_display_roundtrip(display);
    close(pipes[1]);

    /* is it really a good idea to multiply buffer size by 2 every time? */
    const size_t INITIAL_BUFFER_SIZE = 1024;
    const int GROWTH_FACTOR = 2;

    *buffer = xmalloc(INITIAL_BUFFER_SIZE);

    size_t buffer_size = INITIAL_BUFFER_SIZE;
    size_t total_read = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(pipes[0], *buffer + total_read, buffer_size - total_read)) > 0) {
        total_read += bytes_read;

        if (total_read == buffer_size) {
            buffer_size *= GROWTH_FACTOR;
            *buffer = xrealloc(*buffer, buffer_size);
        }
    }

    if (bytes_read == -1) {
        die("error reading from pipe: %s\n", strerror(errno));
    }

    close(pipes[0]);

    trace("done receiving offer\n");
    debug("received %" PRIu64 " bytes\n", total_read);

    return total_read;
}

void receive_offer(void) {
    char* mime_type = NULL;
    char* buffer = NULL;
    struct db_entry* new_entry = NULL;

    mime_type = xstrdup(pick_mime_type());

    if (mime_type == NULL) {
        debug("didn't match any mime type, not receiving this offer\n");
        goto out;
    }

    size_t bytes_read = receive_data(&buffer, mime_type);

    if (bytes_read == 0) {
        warn("received 0 bytes\n");
        goto out;
    }

    if (bytes_read < config.min_data_size) {
        debug("received less bytes than min_data_size, not saving this entry\n");
        goto out;
    }

    new_entry = xmalloc(sizeof(struct db_entry));

    time_t timestamp = time(NULL);

    new_entry->data = buffer;
    new_entry->data_size = bytes_read;
    new_entry->mime_type = mime_type;
    new_entry->timestamp = timestamp;
    new_entry->preview = generate_preview(buffer, config.preview_len, bytes_read, mime_type);

    if (insert_db_entry(new_entry, config.max_entries_count) != 0) {
        die("failed to insert entry into database!\n");
    };
out:
    if (mime_type != NULL) {
        free(mime_type);
    }
    if (buffer != NULL) {
        free(buffer);
    }
    if (new_entry != NULL) {
        free(new_entry->preview);
        free(new_entry);
    }
}

/*
 * Sent immediately after creating the wlr_data_control_offer object.
 * One event per offered MIME type.
 */
void mime_type_offer_handler(void* data, struct zwlr_data_control_offer_v1* offer,
                             const char* mime_type) {
    UNUSED(data);

    trace("got mime type offer %s for offer %p\n", mime_type, (void*)offer);

    if (offer == NULL) {
        warn("offer is NULL!\n");
        return;
    }

    if (offered_mime_types_count >= MAX_OFFERED_MIME_TYPES) {
        warn("offered_mime_types array is full, "
             "but another mime type was received! %s\n", mime_type);
    } else {
        snprintf(offered_mime_types[offered_mime_types_count],
                 sizeof(offered_mime_types[offered_mime_types_count]), "%s", mime_type);
        offered_mime_types_count += 1;
    }
}

const struct zwlr_data_control_offer_v1_listener data_control_offer_listener = {
    .offer = mime_type_offer_handler,
};

/*
 * The data_offer event introduces a new wlr_data_control_offer object,
 * which will subsequently be used in either the
 * wlr_data_control_device.selection event (for the regular clipboard
 * selections) or the wlr_data_control_device.primary_selection event (for
 * the primary clipboard selections). Immediately following the
 * wlr_data_control_device.data_offer event, the new data_offer object
 * will send out wlr_data_control_offer.offer events to describe the MIME
 * types it offers.
 */
void data_offer_handler(void* data, struct zwlr_data_control_device_v1* device,
                        struct zwlr_data_control_offer_v1* new_offer) {
    UNUSED(data);
    UNUSED(device);

    debug("got new wlr_data_control_offer %p\n", (void*)new_offer);

    offered_mime_types_count = 0;

    zwlr_data_control_offer_v1_add_listener(new_offer, &data_control_offer_listener, NULL);
}

/*
 * The selection event is sent out to notify the client of a new
 * wlr_data_control_offer for the selection for this device. The
 * wlr_data_control_device.data_offer and the wlr_data_control_offer.offer
 * events are sent out immediately before this event to introduce the data
 * offer object. The selection event is sent to a client when a new
 * selection is set. The wlr_data_control_offer is valid until a new
 * wlr_data_control_offer or NULL is received. The client must destroy the
 * previous selection wlr_data_control_offer, if any, upon receiving this
 * event.
 *
 * The first selection event is sent upon binding the
 * wlr_data_control_device object.
 */
void selection_handler(void* data, struct zwlr_data_control_device_v1* device,
                       struct zwlr_data_control_offer_v1* new_offer) {
    UNUSED(data);
    UNUSED(device);

    debug("got selection event for offer %p\n", (void*)new_offer);

    if (offer != NULL) {
        trace("destroying previous offer %p\n", (void*)offer);
        zwlr_data_control_offer_v1_destroy(offer);
    }
    offer = new_offer;

    if (offer != NULL) {
        receive_offer();
    }
}

/*
 * The primary_selection event is sent out to notify the client of a new
 * wlr_data_control_offer for the primary selection for this device. The
 * wlr_data_control_device.data_offer and the wlr_data_control_offer.offer
 * events are sent out immediately before this event to introduce the data
 * offer object. The primary_selection event is sent to a client when a
 * new primary selection is set. The wlr_data_control_offer is valid until
 * a new wlr_data_control_offer or NULL is received. The client must
 * destroy the previous primary selection wlr_data_control_offer, if any,
 * upon receiving this event.
 *
 * If the compositor supports primary selection, the first
 * primary_selection event is sent upon binding the
 * wlr_data_control_device object.
 */
void primary_selection_handler(void* data, struct zwlr_data_control_device_v1* device,
                               struct zwlr_data_control_offer_v1* new_offer) {
    UNUSED(data);
    UNUSED(device);

    if (config.primary_selection) {
        debug("got primary selection event for offer %p\n", (void*)new_offer);
    } else {
        debug("ignoring primary selection event for offer %p\n", (void*)new_offer);
    }

    if (offer != NULL) {
        trace("destroying previous offer %p\n", (void*)offer);
        zwlr_data_control_offer_v1_destroy(offer);
    }
    offer = new_offer;

    if (config.primary_selection && offer != NULL) {
        receive_offer();
    }
}

const struct zwlr_data_control_device_v1_listener data_control_device_listener = {
    .data_offer = data_offer_handler,
    .selection = selection_handler,
    .primary_selection = primary_selection_handler,
};

void print_version_and_exit(void) {
    fprintf(stderr, "cclipd version %s\n", CCLIP_VERSION);
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

void parse_command_line(void) {
    int opt;

    while ((opt = getopt(argc, argv, ":d:t:s:c:P:pevVh")) != -1) {
        switch (opt) {
        case 'd':
            debug("db file path supplied on command line: %s\n", optarg);
            config.db_path = optarg;
            break;
        case 't':
            debug("accepted mime type pattern supplied on command line: %s\n", optarg);

            config.accepted_mime_types[config.accepted_mime_types_count] = xstrdup(optarg);
            config.accepted_mime_types_count += 1;
            break;
        case 's':
            config.min_data_size = atoi(optarg);
            if (config.min_data_size < 1) {
                die("MINSIZE must be a positive integer, got %s\n", optarg);
            }
            break;
        case 'c':
            config.max_entries_count = atoi(optarg);
            if (config.max_entries_count < 1) {
                die("ENTRIES must be a positive integer, got %s\n", optarg);
            }
            break;
        case 'P':
            config.preview_len = atoi(optarg);
            if (config.preview_len < 1) {
                die("PREVIEW_LEN must be a positive integer, got %s\n", optarg);
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
            die("error while parsing command line options\n");
        }
    }
}

int main(int _argc, char** _argv) {
    argc = _argc;
    argv = _argv;
    prog_name = argc > 0 ? argv[0] : "cclipd";

    int epoll_fd = -1;
    int signal_fd = -1;

    int exit_status = 0;

    parse_command_line();
    if (config.db_path == NULL) {
        config.db_path = get_default_db_path();
    }
    if (config.accepted_mime_types_count == 0) {
        config.accepted_mime_types[0] = xstrdup("*");
        config.accepted_mime_types_count = 1;
    }

    debug("opening database at %s\n", config.db_path);
    db_init(config.db_path, config.create_db_if_not_exists);

    wayland_init();

    zwlr_data_control_device_v1_add_listener(data_control_device,
                                             &data_control_device_listener,
                                             NULL);

    wl_display_roundtrip(display);

    /* block signals so we can catch them later */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        critical("failed to block signals: %s\n", strerror(errno));
        goto cleanup;
    }

    /* set up signalfd */
    signal_fd = signalfd(-1, &mask, 0);
    if (signal_fd == -1) {
        critical("failed to set up signalfd: %s\n", strerror(errno));
        goto cleanup;
    }

    /* set up epoll */
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        critical("failed to set up epoll: %s\n", strerror(errno));
        goto cleanup;
    }

    struct epoll_event epoll_event;
    /* add wayland fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = wayland_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, wayland_fd, &epoll_event) == -1) {
        critical("failed to add wayland fd to epoll list: %s\n", strerror(errno));
        goto cleanup;
    }
    /* add signal fd to epoll interest list */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = signal_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &epoll_event) == -1) {
        critical("failed to add signal fd to epoll list: %s\n", strerror(errno));
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
                if (wl_display_dispatch(display) == -1) {
                    critical("wl_display_dispatch failed\n");
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
                    sqlite3_close_v2(db);
                    db_init(config.db_path, false);
                    break;
                }
            }
        }
    }
cleanup:
    sqlite3_close_v2(db);
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

