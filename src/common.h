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

extern unsigned int DEBUG_LEVEL;

#define trace(__fmt, ...) do { if (DEBUG_LEVEL >= 2) { fprintf(stderr, "TRACE %s:%d: " __fmt, __FILE__, __LINE__, ##__VA_ARGS__); } } while(0)
#define debug(__fmt, ...) do { if (DEBUG_LEVEL >= 1) { fprintf(stderr, "DEBUG %s:%d: " __fmt, __FILE__, __LINE__, ##__VA_ARGS__); } } while(0)
#define info(__fmt, ...) do { fprintf(stderr, "INFO %s:%d: " __fmt, __FILE__, __LINE__, ##__VA_ARGS__); } while(0)
#define warn(__fmt, ...) do { fprintf(stderr, "WARN %s:%d: " __fmt, __FILE__, __LINE__, ##__VA_ARGS__); } while(0)
#define critical(__fmt, ...) do { fprintf(stderr, "CRITICAL %s:%d: " __fmt, __FILE__, __LINE__, ##__VA_ARGS__); } while(0)
#define die(__fmt, ...) do { fprintf(stderr, "CRITICAL %s:%d: " __fmt, __FILE__, __LINE__, ##__VA_ARGS__); exit(1); } while(0)

#endif /* #ifndef COMMON_H */

