#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fnmatch.h>
#include <ctype.h> /* isspace, isprint */

#include "protocol/wlr-data-control-unstable-v1-client-protocol.h"
#include "wayland.h"
#include "common.h"
#include "db.h"
#include "config.h"

#define PREVIEW_LEN 128

#ifndef VERSION
#define VERSION "unknown_version"
#endif

int argc;
char** argv;
char* prog_name;

/* surely nobody will offer more than 32 mime types */
#define OFFERED_MIME_TYPES_LEN 32
char* offered_mime_types[OFFERED_MIME_TYPES_LEN];
int offered_mime_types_count = 0;

char* pick_mime_type(void) {
    /*
     * finds first offered mime type that matches
     * or returns NULL if none matched
     * yes it is O(n^2) I do not care
     */
    for (int i = 0; i < config.accepted_mime_types_len; i++) {
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

void free_offered_mime_types(void) {
    debug("freeing string in offered_mime_types array\n");
    while (offered_mime_types_count > 0) {
        offered_mime_types_count -= 1;
        free(offered_mime_types[offered_mime_types_count]);
    }
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

size_t lstrip(char* str) {
    /*
     * removes leading whitespace from str
     * returns modified string length
     */
    if (str == NULL) {
        return 0;
    }

    size_t read = 0;
    size_t write = 0;
    bool non_whitespace_found = false;

    while (str[read] != '\0') {
        if (isspace((unsigned char)str[read]) && !non_whitespace_found) {
            read += 1;
        } else {
            non_whitespace_found = true;

            str[write] = str[read];
            write += 1;
            read += 1;
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
        strncpy(preview, data, min(data_size, PREVIEW_LEN));
        sanitise_string(preview);
        lstrip(preview);
    } else {
        snprintf(preview, PREVIEW_LEN, "%s | %" PRIi64 " bytes", mime_type, data_size);
    }

    debug("generated preview: %s\n", preview);

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
    sqlite3_bind_int64(stmt, 5, entry->timestamp);

    /* execute the statement */
    retcode = sqlite3_step(stmt);
    if (retcode != SQLITE_DONE) {
        die("%s\n", sqlite3_errmsg(db));
    } else {
        debug("record inserted successfully\n");
    }

    /* finalize the statement */
    retcode = sqlite3_finalize(stmt);
    if (retcode != SQLITE_OK) {
        die("%s\n", sqlite3_errmsg(db));
    } else {
        debug("sqlite statement finalised\n");
        debug("============================\n");
    }
}

size_t receive_data(char** buffer, struct zwlr_data_control_offer_v1* offer, char* mime_type) {
    /* reads offer into buffer, returns number of bytes read */
    debug("start receiving offer...\n");

    int pipes[2];
    if (pipe(pipes) == -1) {
        die("failed to create pipe\n");
    }

    zwlr_data_control_offer_v1_receive(offer, mime_type, pipes[1]);
    /* AFTER THIS LINE WE WILL GET NEW OFFER!!! */
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

    mime_type = strdup(pick_mime_type());

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

    time_t timestamp = time(NULL);

    new_entry->data = buffer;
    new_entry->data_size = bytes_read;
    new_entry->mime_type = mime_type;
    new_entry->timestamp = timestamp;
    new_entry->preview = generate_preview(buffer, bytes_read, mime_type);

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

/*
 * Sent immediately after creating the wlr_data_control_offer object.
 * One event per offered MIME type.
 */
void mime_type_offer_handler(void* data, struct zwlr_data_control_offer_v1* offer,
                             const char* mime_type) {
    UNUSED(data);

    debug("got mime type offer %s for offer %p\n", mime_type, (void*)offer);

    if (offer == NULL) {
        warn("offer is NULL!\n");
        return;
    }

    if (offered_mime_types_count >= OFFERED_MIME_TYPES_LEN) {
        warn("offered_mime_types array is full, "
             "but another mime type was received! %s\n", mime_type);
    } else {
        offered_mime_types[offered_mime_types_count] = strdup(mime_type);
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
                        struct zwlr_data_control_offer_v1* offer) {
    UNUSED(data);
    UNUSED(device);

    debug("got new wlr_data_control_offer %p\n", (void*)offer);

    free_offered_mime_types();

	zwlr_data_control_offer_v1_add_listener(offer, &data_control_offer_listener, NULL);
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
                       struct zwlr_data_control_offer_v1* offer) {
    UNUSED(data);
    UNUSED(device);

    debug("got selection event for offer %p\n", (void*)offer);

    if (offer == NULL) {
        warn("offer is NULL!\n");
        return;
    }

    receive(offer);
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
                               struct zwlr_data_control_offer_v1* offer) {
    UNUSED(data);
    UNUSED(device);

    debug("got primary selection event for offer %p\n", (void*)offer);

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

    while ((opt = getopt(argc, argv, ":d:t:s:pvVh")) != -1) {
        switch (opt) {
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
    /* TODO: signal handling */
    while (wl_display_dispatch(display) != -1) {
        /* main event loop */
    }
}

