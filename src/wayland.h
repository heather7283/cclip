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
#ifndef WAYLAND_H
#define WAYLAND_H

extern struct wl_display* display;
extern struct wl_seat* seat;
extern struct wl_registry* registry;
extern struct zwlr_data_control_manager_v1* data_control_manager;
extern struct zwlr_data_control_device_v1* data_control_device;
extern int wayland_fd;

void wayland_init(void);
void wayland_cleanup(void);

#endif /* #ifndef WAYLAND_H */

