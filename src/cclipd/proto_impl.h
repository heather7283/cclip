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

#include <assert.h>
#include <stdbool.h>

#include <wayland-client.h>

#include "ext-data-control-v1.h"
#include "wlr-data-control-unstable-v1.h"

struct data_ctrl_manager;
struct data_ctrl_device;
struct data_ctrl_offer;

struct data_ctrl_offer_listener {
    void (*offer)(void* data, struct data_ctrl_offer* offer, const char* type);
};

#define CHECK_OFFSET(type, field) \
    static_assert(offsetof(type, field) == \
                  offsetof(struct zwlr_data_control_offer_v1_listener, field) \
                  && \
                  offsetof(type, field) == \
                  offsetof(struct ext_data_control_offer_v1_listener, field), \
                  "offset mismatch of field '" #field "' between " #type ", " \
                  "zwlr_data_control_offer_v1_listener and ext_data_control_offer_v1_listener")

CHECK_OFFSET(struct data_ctrl_offer_listener, offer);

#undef CHECK_OFFSET

struct data_ctrl_device_listener {
    void (*data_offer)(void *data,
                       struct data_ctrl_device* data_ctrl_device,
                       struct data_ctrl_offer* offer);

    void (*selection)(void *data,
                      struct data_ctrl_device* data_ctrl_device,
                      struct data_ctrl_offer* offer);

    void (*finished)(void *data,
                     struct data_ctrl_device* data_ctrl_device);

    void (*primary_selection)(void *data,
                              struct data_ctrl_device* data_ctrl_device,
                              struct data_ctrl_offer* offer);
};

#define CHECK_OFFSET(type, field) \
    static_assert(offsetof(type, field) == \
                  offsetof(struct zwlr_data_control_device_v1_listener, field) \
                  && \
                  offsetof(type, field) == \
                  offsetof(struct ext_data_control_device_v1_listener, field), \
                  "offset mismatch of field '" #field"' between " #type ", " \
                  "zwlr_data_control_device_v1_listener and ext_data_control_device_v1_listener")

CHECK_OFFSET(struct data_ctrl_device_listener, data_offer);
CHECK_OFFSET(struct data_ctrl_device_listener, selection);
CHECK_OFFSET(struct data_ctrl_device_listener, finished);
CHECK_OFFSET(struct data_ctrl_device_listener, primary_selection);

#undef CHECK_OFFSET

struct protocol_implementation {
    void (*const data_ctrl_offer_receive)(struct data_ctrl_offer* data_ctrl_offer,
                                          const char* mime_type, int32_t fd);
    int (*const data_ctrl_offer_add_listener)(struct data_ctrl_offer* offer,
                                              const struct data_ctrl_offer_listener* listener,
                                              void* data);
    void (*const data_ctrl_offer_destroy)(struct data_ctrl_offer* offer);

    int (*const data_ctrl_device_add_listener)(struct data_ctrl_device* data_ctrl_device,
                                               const struct data_ctrl_device_listener* listener,
                                               void* data);
    void (*const data_ctrl_device_destroy)(struct data_ctrl_device* device);

    struct data_ctrl_device* (*const get_data_ctrl_device)(struct data_ctrl_manager* manager,
                                                           struct wl_seat *seat);
};

const struct protocol_implementation *get_protocol_implementation_wlr(void);
const struct protocol_implementation *get_protocol_implementation_ext(void);

