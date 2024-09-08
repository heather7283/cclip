#ifndef WAYLAND_H
#define WAYLAND_H

extern struct wl_display* display;
extern struct wl_seat* seat;
extern struct wl_registry* registry;
extern struct zwlr_data_control_manager_v1* data_control_manager;
extern struct zwlr_data_control_device_v1* data_control_device;

void wayland_init(void);

#endif /* #ifndef WAYLAND_H */
