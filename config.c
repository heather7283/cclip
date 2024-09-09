#include <unistd.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <stdio.h>

#include "common.h"
#include "config.h"

#define MAX_PATH_LENGTH 1024

struct config config = {
    .verbose = false,
    .accepted_mime_types_len = 0,
    .accepted_mime_types = NULL,
    .min_data_size = 1,
    .db_path = NULL,
    .config_file_path = NULL,
    .primary_selection = false,
};

static char* get_default_db_path(void) {
    char* db_path = malloc(MAX_PATH_LENGTH * sizeof(char));
    if (db_path == NULL) {
        die("failed to allocate memory for db path string\n");
    }

    char* xdg_data_home = getenv("XDG_DATA_HOME");
    if (xdg_data_home != NULL) {
        snprintf(db_path, MAX_PATH_LENGTH, "%s/%s", xdg_data_home, "cclip/db.sqlite3");
    } else {
        char* home = getenv("HOME");
        if (home == NULL) {
            die("both HOME and XDG_DATA_HOME are unset, unable to determine db file path\n");
        }
        snprintf(db_path, MAX_PATH_LENGTH, "%s/.local/share/%s", home, "cclip/db.sqlite3");
    }

    debug("setting default db path: %s\n", db_path);
    return db_path;
}

static char* get_default_config_path(void) {
    char* config_path = malloc(MAX_PATH_LENGTH * sizeof(char));
    if (config_path == NULL) {
        die("failed to allocate memory for config path string\n");
    }

    char* xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (xdg_config_home != NULL) {
        snprintf(config_path, MAX_PATH_LENGTH, "%s/%s", xdg_config_home, "cclip/cclipd.ini");
    } else {
        char* home = getenv("HOME");
        if (home == NULL) {
            die("both HOME and XDG_CONFIG_HOME are unset, unable to determine config file path\n");
        }
        snprintf(config_path, MAX_PATH_LENGTH, "%s/.config/%s", home, "cclip/cclipd.ini");
    }

    debug("setting default config path: %s\n", config_path);
    return config_path;
}

void config_set_default_values(void) {
    if (config.db_path == NULL) {
        config.db_path = get_default_db_path();
    }
    if (config.config_file_path == NULL) {
        config.config_file_path = get_default_config_path();
    }
    if (config.accepted_mime_types == NULL) {
        config.accepted_mime_types = malloc(sizeof(char*) * 1);
        config.accepted_mime_types[0] = "*";

        config.accepted_mime_types_len = 1;
    }
}

