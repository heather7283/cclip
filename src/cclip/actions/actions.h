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

#pragma once

#include <sqlite3.h>

#define FOR_LIST_OF_ACTIONS(DO) \
    DO(list) \
    DO(get) \
    DO(copy) \
    DO(delete) \
    DO(tag) \
    DO(tags) \
    DO(vacuum) \
    DO(wipe) \

typedef void action_func_t(int argc, char** argv, struct sqlite3* db) __attribute__((noreturn));

#define DEFINE_ACTION_FUNCTION(name, ...) action_func_t action_##name;
FOR_LIST_OF_ACTIONS(DEFINE_ACTION_FUNCTION)

action_func_t* match_action(const char* input);

/* some helper macros */
#define RESET_GETOPT() ({ optreset = 1; optind = 0; })
#define OUT(rc) ({ retcode = (rc); goto out; })

