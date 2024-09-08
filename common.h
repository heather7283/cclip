#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h> /* exit */
#include <stdio.h> /* fprintf */

#define debug(...) do { fprintf(stderr, "DEBUG: " __VA_ARGS__); } while(0)
#define warn(...) do { fprintf(stderr, "WARN: " __VA_ARGS__); } while(0)
#define die(...) do { fprintf(stderr, "CRITICAL: " __VA_ARGS__); exit(1); } while(0)

#endif /* #ifndef COMMON_H */

