extern struct wl_seat* seat;
extern struct zwlr_data_control_manager_v1* data_control_manager;
extern const struct wl_registry_listener registry_listener;

extern struct config {
    int debug_level; /* 0: err, 1: warn, 2: debug */
    const char* db_path;
} config;

void die(const char* const message);
void warn(const char* const message);
void debug(const char* const message);
