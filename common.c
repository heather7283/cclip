#define _POSIX_C_SOURCE 2 // getopt
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

struct config config = {
    .debug_level = 2,
    .db_path = NULL
};

void die(const char* const message) {
	fprintf(stderr, "critical: %s\n", message);
	exit(1);
}

void warn(const char* const message) {
    if (config.debug_level >= 1) {
        fprintf(stderr, "warn: %s\n", message);
    }
}

void debug(const char* const message) {
    if (config.debug_level >= 2) {
        fprintf(stderr, "debug: %s\n", message);
    }
}

