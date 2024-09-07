#include <stdint.h>
#define _XOPEN_SOURCE 500 /* pread */
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h> /* time */

#include "protocol/wlr-data-control-unstable-v1-client-protocol.h"
#include "wayland.h"
#include "common.h"
#include "db.h"
#include "pending_offers.h"

void receive(struct zwlr_data_control_offer_v1* offer) {
    fprintf(stderr, "listing all available MIME types:\n");
    struct pending_offer* pending_offer = find_pending_offer(offer);
    char* mime_type = strdup(pending_offer->data->mime_types[0]);
    for (unsigned int i = 0; i < pending_offer->data->mime_types_len; i++) {
        fprintf(stderr, "%d\t%s\n", i, pending_offer->data->mime_types[i]);
    }
    fprintf(stderr, "done listing all available MIME types.\n");
    delete_pending_offer(offer);

    time_t timestamp = time(NULL);

    debug("start receiving offer...");

    int pipes[2];
    if (pipe(pipes) == -1) {
        die("failed to create pipe");
    }

    zwlr_data_control_offer_v1_receive(offer, mime_type, pipes[1]);
    wl_display_roundtrip(display);
    close(pipes[1]);

    /* TODO: rewrite this reading from pipe code */
    const size_t INITIAL_BUFFER_SIZE = 1024;
    const int GROWTH_FACTOR = 2;

    char* buffer = malloc(INITIAL_BUFFER_SIZE);
    if (!buffer) {
        die("failed to allocate initial buffer");
    }

    size_t buffer_size = INITIAL_BUFFER_SIZE;
    size_t total_read = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(pipes[0], buffer + total_read, buffer_size - total_read)) > 0) {
        total_read += bytes_read;

        if (total_read == buffer_size) {
            buffer_size *= GROWTH_FACTOR;
            char* new_buffer = realloc(buffer, buffer_size);
            if (new_buffer == NULL) {
                die("failed to reallocate buffer");
            }
            buffer = new_buffer;
        }
    }

    if (bytes_read == -1) {
        die("error reading from pipe");
    }

    close(pipes[0]);

    debug("done receiving offer");

    if (total_read == 0) {
        warn("received 0 bytes");
        goto out;
    }

    /* Prepare the SQL statement */
    const char* insert_statement =
        "INSERT INTO history (data, mime_type, timestamp) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;
    int ret_code = sqlite3_prepare_v2(db, insert_statement, -1, &stmt, NULL);
    if (ret_code != SQLITE_OK) {
        die(sqlite3_errmsg(db));
    }

    /* Bind parameters */
    sqlite3_bind_blob(stmt, 1, buffer, total_read, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, mime_type, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, timestamp);

    /* Execute the statement */
    ret_code = sqlite3_step(stmt);
    if (ret_code != SQLITE_DONE) {
        die(sqlite3_errmsg(db));
    } else {
        debug("record inserted successfully");
    }

    /* Finalize the statement */
    sqlite3_finalize(stmt);

out:
    free(mime_type);
    free(buffer);
    zwlr_data_control_offer_v1_destroy(offer);
}

void mime_type_offer_handler(void* data, struct zwlr_data_control_offer_v1* offer, const char* mime_type) {
    if (offer == NULL) {
        return;
    }

    pending_offer_add_mimetype(offer, mime_type);

    fprintf(stderr, "Got MIME type offer: %s\n", mime_type);
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
        return;
    }

    receive(offer);
}

void primary_selection_handler(void* data, struct zwlr_data_control_device_v1* device, struct zwlr_data_control_offer_v1* offer) {
    if (offer == NULL) {
        return;
    }

    /* receive(offer); */
    /* ignore primary selection for now, will be a config option later */
    return;
}

const struct zwlr_data_control_device_v1_listener data_control_device_listener = {
	.data_offer = data_offer_handler,
	.selection = selection_handler,
	.primary_selection = primary_selection_handler,
};

int main(int argc, char** argv) {
    char* db_path = get_db_path();
    db_init(db_path);
    free(db_path);

    wayland_init();

	zwlr_data_control_device_v1_add_listener(data_control_device,
                                             &data_control_device_listener,
                                             NULL);

	wl_display_roundtrip(display);
    while (wl_display_dispatch(display) != -1) {
        /* main event loop */
    }
}
