#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

struct config {
    bool verbose;
    int accepted_mime_types_len;
    char** accepted_mime_types;
    char* db_path;
    char* config_file_path;
};
extern struct config config;

void config_set_default_values(void);

#endif /* #ifndef CONFIG_H */
