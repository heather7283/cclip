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

#include "proto_impl.h"

#define TYPECAST(to, from) (__typeof__(to))(from)

/* =========================== ext_data_control_manager_v1 ============================ */

static void ext_data_ctrl_offer_receive(struct data_ctrl_offer* _offer,
                                        const char* mime_type, int fd) {
    struct ext_data_control_offer_v1* offer = TYPECAST(offer, _offer);

    ext_data_control_offer_v1_receive(offer, mime_type, fd);
}

static int ext_data_ctrl_offer_add_listener(struct data_ctrl_offer* _offer,
                                            const struct data_ctrl_offer_listener* _listener,
                                            void *data) {
    struct ext_data_control_offer_v1* offer = TYPECAST(offer, _offer);
    const struct ext_data_control_offer_v1_listener* listener = TYPECAST(listener, _listener);

    return ext_data_control_offer_v1_add_listener(offer, listener, data);
}

static void ext_data_ctrl_offer_destroy(struct data_ctrl_offer* _offer) {
    struct ext_data_control_offer_v1* offer = TYPECAST(offer, _offer);

    ext_data_control_offer_v1_destroy(offer);
}

static int ext_data_ctrl_device_add_listener(struct data_ctrl_device* _device,
                                             const struct data_ctrl_device_listener* _listener,
                                             void* data) {
    struct ext_data_control_device_v1* device = TYPECAST(device, _device);
    const struct ext_data_control_device_v1_listener* listener = TYPECAST(listener, _listener);

    return ext_data_control_device_v1_add_listener(device, listener, data);
}

static void ext_data_ctrl_device_destroy(struct data_ctrl_device* _device) {
    struct ext_data_control_device_v1* device = TYPECAST(device, _device);

    ext_data_control_device_v1_destroy(device);
}

static struct data_ctrl_device* ext_get_data_ctrl_device(struct data_ctrl_manager* _manager,
                                                         struct wl_seat *seat) {
    struct ext_data_control_manager_v1* manager = TYPECAST(manager, _manager);

    return (struct data_ctrl_device *)ext_data_control_manager_v1_get_data_device(manager, seat);
}

static const struct protocol_implementation ext_protocol_implementation = {
    .data_ctrl_offer_receive = ext_data_ctrl_offer_receive,
    .data_ctrl_offer_add_listener = ext_data_ctrl_offer_add_listener,
    .data_ctrl_offer_destroy = ext_data_ctrl_offer_destroy,

    .data_ctrl_device_add_listener = ext_data_ctrl_device_add_listener,
    .data_ctrl_device_destroy = ext_data_ctrl_device_destroy,

    .get_data_ctrl_device = ext_get_data_ctrl_device,
};

const struct protocol_implementation *get_protocol_implementation_ext(void) {
    return &ext_protocol_implementation;
}

/* =========================== wlr_data_control_manager_v1 ============================ */

static void wlr_data_ctrl_offer_receive(struct data_ctrl_offer* _offer,
                                        const char* mime_type, int fd) {
    struct zwlr_data_control_offer_v1* offer = TYPECAST(offer, _offer);

    zwlr_data_control_offer_v1_receive(offer, mime_type, fd);
}

static int wlr_data_ctrl_offer_add_listener(struct data_ctrl_offer* _offer,
                                            const struct data_ctrl_offer_listener* _listener,
                                            void *data) {
    struct zwlr_data_control_offer_v1* offer = TYPECAST(offer, _offer);
    const struct zwlr_data_control_offer_v1_listener* listener = TYPECAST(listener, _listener);

    return zwlr_data_control_offer_v1_add_listener(offer, listener, data);
}

static void wlr_data_ctrl_offer_destroy(struct data_ctrl_offer* _offer) {
    struct zwlr_data_control_offer_v1* offer = TYPECAST(offer, _offer);

    zwlr_data_control_offer_v1_destroy(offer);
}

static int wlr_data_ctrl_device_add_listener(struct data_ctrl_device* _device,
                                             const struct data_ctrl_device_listener* _listener,
                                             void* data) {
    struct zwlr_data_control_device_v1* device = TYPECAST(device, _device);
    const struct zwlr_data_control_device_v1_listener* listener = TYPECAST(listener, _listener);

    return zwlr_data_control_device_v1_add_listener(device, listener, data);
}

static void wlr_data_ctrl_device_destroy(struct data_ctrl_device* _device) {
    struct zwlr_data_control_device_v1* device = TYPECAST(device, _device);

    zwlr_data_control_device_v1_destroy(device);
}

static struct data_ctrl_device* wlr_get_data_ctrl_device(struct data_ctrl_manager* _manager,
                                                         struct wl_seat *seat) {
    struct zwlr_data_control_manager_v1* manager = TYPECAST(manager, _manager);

    return (struct data_ctrl_device *)zwlr_data_control_manager_v1_get_data_device(manager, seat);
}

static const struct protocol_implementation wlr_protocol_implementation = {
    .data_ctrl_offer_receive = wlr_data_ctrl_offer_receive,
    .data_ctrl_offer_add_listener = wlr_data_ctrl_offer_add_listener,
    .data_ctrl_offer_destroy = wlr_data_ctrl_offer_destroy,

    .data_ctrl_device_add_listener = wlr_data_ctrl_device_add_listener,
    .data_ctrl_device_destroy = wlr_data_ctrl_device_destroy,

    .get_data_ctrl_device = wlr_get_data_ctrl_device,
};

const struct protocol_implementation *get_protocol_implementation_wlr(void) {
    return &wlr_protocol_implementation;
}

