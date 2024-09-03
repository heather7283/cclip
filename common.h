extern struct config {
    int debug_level; /* 0: err, 1: warn, 2: debug */
    const char* db_path;
} config;

void die(const char* const message);
void warn(const char* const message);
void debug(const char* const message);
