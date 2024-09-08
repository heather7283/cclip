#ifndef PENDING_OFFERS_H
#define PENDING_OFFERS_H

#include <stdint.h>

struct pending_offer_data {
    struct zwlr_data_control_offer_v1* offer;
    unsigned int mime_types_len;
    char** mime_types;
};

struct pending_offer {
    struct pending_offer* previous;
    struct pending_offer_data* data;
    struct pending_offer* next;
};

extern struct pending_offer* pending_offers;

struct pending_offer* find_pending_offer(const struct zwlr_data_control_offer_v1* offer);

void add_pending_offer(struct zwlr_data_control_offer_v1* offer);

void delete_pending_offer(const struct zwlr_data_control_offer_v1* offer);

void pending_offer_add_mimetype(const struct zwlr_data_control_offer_v1* offer,
                                const char* mime_type);

#endif /* #ifndef PENDING_OFFERS_H */

