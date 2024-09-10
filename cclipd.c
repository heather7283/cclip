#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h> /* time */
#include <fnmatch.h>
#include <ctype.h> /* isspace, isprint */

#include "protocol/wlr-data-control-unstable-v1-client-protocol.h"
#include "wayland.h"
#include "common.h"
#include "db.h"
#include "pending_offers.h"
#include "config.h"

#define PREVIEW_LEN 128

#ifndef VERSION
#define VERSION "unknown_version"
#endif

int argc;
char** argv;
char* prog_name;

char* pick_mime_type(unsigned int mime_types_len, char** mime_types) {
    /*
     * finds first offered mime type that matches
     * or returns NULL if none matched
     * yes it is O(n^2) I do not care
     */
    for (int i = 0; i < config.accepted_mime_types_len; i++) {
        for (unsigned int j = 0; j < mime_types_len; j++) {
            char* pattern = config.accepted_mime_types[i];
            char* string = mime_types[j];

            debug("matching %s against %s\n", string, pattern);
            if (fnmatch(pattern, string, 0) == 0) {
                debug("selected mime type: %s\n", string);
                return strdup(string);
            }
        }
    }
    return NULL;
}

size_t sanitise_string(char* str) {
    /*
     * makes sure garbage characters don't leak into preview
     * returns size of modified string because it can change
     */
    if (str == NULL) {
        return 0;
    }

    size_t read = 0;
    size_t write = 0;

    while (str[read] != '\0') {
        if (isprint((unsigned char)str[read]) || str[read] == ' ') {
            /* printable ASCII character or space */
            str[write] = str[read];
            write += 1;
            read += 1;
        } else if (isspace((unsigned char)str[read])) {
            /* other whitespace characters (newline, tab, etc.) */
            str[write] = ' ';
            write += 1;
            read += 1;
        } else {
            /* Check for UTF-8 multi-byte sequence */
            int utf8_len = 0;
            unsigned char first_byte = (unsigned char)str[read];

            if ((first_byte & 0x80) == 0) {
                utf8_len = 1;  /* ASCII char (0xxxxxxx) */
            } else if ((first_byte & 0xE0) == 0xC0) {
                utf8_len = 2;  /* 2-byte UTF-8 (110xxxxx) */
            } else if ((first_byte & 0xF0) == 0xE0) {
                utf8_len = 3;  /* 3-byte UTF-8 (1110xxxx) */
            } else if ((first_byte & 0xF8) == 0xF0) {
                utf8_len = 4;  /* 4-byte UTF-8 (11110xxx) */
            }

            if (utf8_len > 1 && read + utf8_len <= strlen(str)) {
                /* valid multibyte UTF-8 sequence */
                for (int i = 0; i < utf8_len; i++) {
                    str[write] = str[read];
                    write += 1;
                    read += 1;
                }
            } else {
                /* invalid or unprintable character */
                str[write] = '?';
                write += 1;
                read += 1;
            }
        }
    }

    /* dont forget the null terminator */
    str[write] = '\0';

    return write;
}

char* generate_preview(const void* const data, const int64_t data_size,
                       const char* const mime_type) {
    char* preview = calloc(PREVIEW_LEN, sizeof(char));
    if (preview == NULL) {
        die("failed to allocate memory for preview string\n");
    }

    if (fnmatch("*text*", mime_type, 0) == 0) {
        /* TODO: strip whitespace from the beginning of preview */
        strncpy(preview, data, min(data_size, PREVIEW_LEN));
        sanitise_string(preview);
    } else {
        snprintf(preview, PREVIEW_LEN, "%s | %" PRIi64 " bytes", mime_type, data_size);
    }

    return preview;
}

void insert_db_entry(struct db_entry* entry) {
    sqlite3_stmt* stmt = NULL;
    int retcode;

    /* prepare the SQL statement */
    const char* insert_statement =
        "INSERT OR REPLACE INTO history "
        "(data, data_size, preview, mime_type, timestamp) "
        "VALUES (?, ?, ?, ?, ?)";
    retcode = sqlite3_prepare_v2(db, insert_statement, -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        die("%s\n", sqlite3_errmsg(db));
    }

    /* bind parameters */
    sqlite3_bind_blob(stmt, 1, entry->data, entry->data_size, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, entry->data_size);
    sqlite3_bind_text(stmt, 3, entry->preview, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, entry->mime_type, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, entry->creation_time);

    /* execute the statement */
    retcode = sqlite3_step(stmt);
    if (retcode != SQLITE_DONE) {
        die("%s\n", sqlite3_errmsg(db));
    } else {
        debug("record inserted successfully\n");
    }

    /* finalize the statement */
    sqlite3_finalize(stmt);
}

size_t receive_data(char** buffer, struct zwlr_data_control_offer_v1* offer, char* mime_type) {
    /* reads offer into buffer, returns number of bytes read */
    debug("start receiving offer...\n");

    int pipes[2];
    if (pipe(pipes) == -1) {
        die("failed to create pipe\n");
    }

    zwlr_data_control_offer_v1_receive(offer, mime_type, pipes[1]);
    wl_display_roundtrip(display);
    close(pipes[1]);

    /* is it really a good idea to multiply buffer size by 2 every time? */
    const size_t INITIAL_BUFFER_SIZE = 1024;
    const int GROWTH_FACTOR = 2;

    *buffer = malloc(INITIAL_BUFFER_SIZE);
    if (*buffer == NULL) {
        die("failed to allocate initial buffer\n");
    }

    size_t buffer_size = INITIAL_BUFFER_SIZE;
    size_t total_read = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(pipes[0], *buffer + total_read, buffer_size - total_read)) > 0) {
        total_read += bytes_read;

        if (total_read == buffer_size) {
            buffer_size *= GROWTH_FACTOR;
            char* new_buffer = realloc(*buffer, buffer_size);
            if (new_buffer == NULL) {
                die("failed to reallocate buffer\n");
            }
            *buffer = new_buffer;
        }
    }

    if (bytes_read == -1) {
        die("error reading from pipe\n");
    }

    close(pipes[0]);

    debug("done receiving offer\n");
    debug("received %" PRIu64 " bytes\n", total_read);

    return total_read;
}

void receive(struct zwlr_data_control_offer_v1* offer) {
    char* mime_type = NULL;
    char* buffer = NULL;
    struct db_entry* new_entry = NULL;
    struct pending_offer* pending_offer = NULL;

    time_t timestamp = time(NULL);

    pending_offer = find_pending_offer(offer);
    mime_type = pick_mime_type(pending_offer->data->mime_types_len,
                               pending_offer->data->mime_types);
    delete_pending_offer(offer);

    if (mime_type == NULL) {
        debug("didn't match any mime type, not receiving this offer\n");
        goto out;
    }

    size_t bytes_read = receive_data(&buffer, offer, mime_type);

    if (bytes_read == 0) {
        warn("received 0 bytes\n");
        goto out;
    }

    if (bytes_read < config.min_data_size) {
        debug("received less bytes than min_data_size, not saving this entry\n");
        goto out;
    }

    new_entry = malloc(sizeof(struct db_entry));
    if (new_entry == NULL) {
        die("failed to allocate memory for db_entry struct\n");
    }

    new_entry->data = buffer;
    new_entry->data_size = bytes_read;
    new_entry->mime_type = mime_type;
    new_entry->creation_time = timestamp;
    new_entry->preview = generate_preview(new_entry->data,
                                          new_entry->data_size,
                                          new_entry->mime_type);

    insert_db_entry(new_entry);
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
    zwlr_data_control_offer_v1_destroy(offer);
}

void mime_type_offer_handler(void* data, struct zwlr_data_control_offer_v1* offer, const char* mime_type) {
    if (offer == NULL) {
        warn("offer is NULL!\n");
        return;
    }

    pending_offer_add_mimetype(offer, mime_type);
}

const struct zwlr_data_control_offer_v1_listener data_control_offer_listener = {
	.offer = mime_type_offer_handler,
};

void data_offer_handler(void* data, struct zwlr_data_control_device_v1* device, struct zwlr_data_control_offer_v1* offer) {
    add_pending_offer(offer);

	zwlr_data_control_offer_v1_add_listener(offer, &data_control_offer_listener, NULL);
}

void selection_handler(void* data, struct zwlr_data_control_device_v1* device, struct zwlr_data_control_offer_v1* offer) {
    if (offer == NULL) {
        warn("offer is NULL!\n");
        return;
    }

    receive(offer);
}

void primary_selection_handler(void* data, struct zwlr_data_control_device_v1* device, struct zwlr_data_control_offer_v1* offer) {
    if (!config.primary_selection) {
        return;
    }

    if (offer == NULL) {
        warn("offer is NULL!\n");
        return;
    }

    receive(offer);
}

const struct zwlr_data_control_device_v1_listener data_control_device_listener = {
	.data_offer = data_offer_handler,
	.selection = selection_handler,
	.primary_selection = primary_selection_handler,
};

void print_version_and_exit(void) {
    fprintf(stderr, "cclipd version %s\n", VERSION);
    exit(0);
}

void print_help_and_exit(int exit_status) {
    const char* help_string =
        "cclipd - clipboard manager daemon\n"
        "\n"
        "usage:\n"
        "    %s [-vVhp] [-d DB_PATH] [-t PATTERN] [-s SIZE]\n"
        "\n"
        "command line options:\n"
        "    -V            display version and exit\n"
        "    -h            print this help message and exit\n"
        "    -v            increase verbosity\n"
        "    -d DB_PATH    specify path to databse file\n"
        "    -t PATTERN    specify MIME type pattern to accept,\n"
        "                  can be supplied multiple times\n"
        "    -s SIZE       clipboard entry will only be saved if\n"
        "                  its size in bytes is not less than SIZE\n"
        "    -p            also monitor primary selection\n";

    fprintf(stderr, help_string, prog_name);
    exit(exit_status);
}

void parse_command_line(void) {
    int opt;

    while ((opt = getopt(argc, argv, ":c:d:t:s:pvVh")) != -1) {
        switch (opt) {
        case 'c':
            debug("config file path supplied on command line: %s\n", optarg);
            config.config_file_path = strdup(optarg);
            break;
        case 'd':
            debug("db file path supplied on command line: %s\n", optarg);
            config.db_path = strdup(optarg);
            break;
        case 't':
            debug("accepted mime type pattern supplied on command line: %s\n", optarg);

            char* new_mimetype = strdup(optarg);
            if (new_mimetype == NULL) {
                die("failed to allocate memory for accepted mime type pattern\n");
            }

            char** new_accepted_mime_types =
                    realloc(config.accepted_mime_types,
                            (config.accepted_mime_types_len + 1) * sizeof(char*));
            if (new_accepted_mime_types == NULL) {
                die("failed to allocate memory for accepted mime types array\n");
            }

            config.accepted_mime_types = new_accepted_mime_types;
            config.accepted_mime_types[config.accepted_mime_types_len] = new_mimetype;
            config.accepted_mime_types_len += 1;
            break;
        case 's':
            config.min_data_size = atoi(optarg);
            if (config.min_data_size < 1) {
                die("MINSIZE must be a positive integer, got %s\n", optarg);
            }
            break;
        case 'p':
            config.primary_selection = true;
            break;
        case 'v':
            config.verbose = true;
            break;
        case 'V':
            print_version_and_exit();
            break;
        case 'h':
            print_help_and_exit(0);
            break;
        case '?':
            fprintf(stderr, "unknown option: %c\n", optopt);
            print_help_and_exit(1);
            break;
        case ':':
            fprintf(stderr, "missing arg for %c\n", optopt);
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

    parse_command_line();
    config_set_default_values();

    char* db_path = config.db_path;
    db_init(db_path);

    wayland_init();

	zwlr_data_control_device_v1_add_listener(data_control_device,
                                             &data_control_device_listener,
                                             NULL);

	wl_display_roundtrip(display);
    while (wl_display_dispatch(display) != -1) {
        /* main event loop */
    }
}

