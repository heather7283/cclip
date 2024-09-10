#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h> /* exit */
#include <stdio.h> /* fprintf */

#include "config.h"

#define debug(...) do { if (config.verbose) { fprintf(stderr, "DEBUG: " __VA_ARGS__); } } while(0)
#define warn(...) do { fprintf(stderr, "WARN: " __VA_ARGS__); } while(0)
#define die(...) do { fprintf(stderr, "CRITICAL: " __VA_ARGS__); exit(1); } while(0)

#define min(a, b) a < b ? a : b
#define max(a, b) a > b ? a : b

#define UNUSED(var) (void)var /* https://stackoverflow.com/a/3599170 */

#endif /* #ifndef COMMON_H */

