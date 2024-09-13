#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "pending_offers.h"
#include "common.h"

/*
 * THIS IS A HORRIBLE MESS
 * I hate this so much lol, this needs to be redone from scratch
 * but I guess if it works it works :haha
 */

struct pending_offer* pending_offers = NULL;

struct pending_offer* find_pending_offer(const struct zwlr_data_control_offer_v1* offer) {
    struct pending_offer* pending_offer = pending_offers;
    bool match_found = false;

    while (pending_offer != NULL) {
        if (pending_offer->data->offer == offer) {
            match_found = true;
            break;
        }
        pending_offer = pending_offer->previous;
    }

    if (!match_found) {
        die("did not find matching offer in pending offers list, "
            "this is most likely a bug in cclipd\n");
    }

    return pending_offer;
}

static struct pending_offer* find_last_pending_offer(void) {
    struct pending_offer* pending_offer = pending_offers;
    bool found = false;

    int counter = 0;

    while (pending_offer != NULL) {
        if (pending_offer->previous == NULL) {
            found = true;
            break;
        }
        pending_offer = pending_offer->previous;
        counter += 1;
    }

    debug("count of pending offers: %d\n", counter);

    if (found == false) {
        die("did not find last offer in pending offers list, "
            "this is most likely a bug in cclipd\n");
    }

    return pending_offer;
}


void add_pending_offer(struct zwlr_data_control_offer_v1* offer) {
    struct pending_offer* new_pending_offer = malloc(sizeof(struct pending_offer));
    struct pending_offer_data* new_pending_offer_data = malloc(sizeof(struct pending_offer_data));

    new_pending_offer_data->offer = offer;
    new_pending_offer_data->mime_types = NULL;
    new_pending_offer_data->mime_types_len = 0;
    new_pending_offer->data = new_pending_offer_data;

    if (pending_offers == NULL) {
        pending_offers = new_pending_offer;

        new_pending_offer->next = NULL;
        new_pending_offer->previous = NULL;
    } else {
        struct pending_offer* last_pending_offer = find_last_pending_offer();

        last_pending_offer->previous = new_pending_offer;
        new_pending_offer->next = last_pending_offer;
        new_pending_offer->previous = NULL;
    }
}

void delete_pending_offer(const struct zwlr_data_control_offer_v1* offer) {
    struct pending_offer* pending_offer = find_pending_offer(offer);

    if (pending_offer == pending_offers) {
        pending_offers = pending_offer->previous;
    } else if (pending_offer->previous == NULL) {
        pending_offer->next->previous = NULL;
    } else {
        pending_offer->next->previous = pending_offer->previous;
        pending_offer->previous->next = pending_offer->next;
    }

    for (unsigned int i = 0; i < pending_offer->data->mime_types_len; i++) {
        free(pending_offer->data->mime_types[i]);
    }
    free(pending_offer->data->mime_types);
    free(pending_offer->data);
    free(pending_offer);
}

void pending_offer_add_mimetype(const struct zwlr_data_control_offer_v1* offer,
                                const char* mime_type) {
    struct pending_offer* pending_offer = find_pending_offer(offer);
    struct pending_offer_data* data = pending_offer->data;

    /* allocate memory for the new MIME type */
    char* new_mime_type = strdup(mime_type);
    if (new_mime_type == NULL) {
        die("failed to allocate memory for new mime type\n");
    }

    /* resize the mime_types array */
    char** new_mime_types = realloc(data->mime_types, (data->mime_types_len + 1) * sizeof(char*));
    if (new_mime_types == NULL) {
        die("failed to allocate memory for new mime type\n");
    }

    data->mime_types = new_mime_types;
    data->mime_types[data->mime_types_len] = new_mime_type;
    data->mime_types_len += 1;
}

