#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

struct config {
    bool verbose;
    int accepted_mime_types_len;
    char** accepted_mime_types;
    size_t min_data_size;
    char* db_path;
    char* config_file_path;
    bool primary_selection;
};
extern struct config config;

void config_set_default_values(void);

#endif /* #ifndef CONFIG_H */
