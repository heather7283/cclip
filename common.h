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

