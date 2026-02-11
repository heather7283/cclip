/*
 * This file is part of cclip, clipboard manager for wayland
 * Copyright (C) 2026  heather7283
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

#include <string.h>
#include <stddef.h>

#include "actions.h"
#include "common/macros.h"

static const struct {
    const char* const name;
    action_func_t* const action;
} actions[] = {
    #define DEFINE_ACTION_TABLE(name, ...) { #name, action_##name },
    FOR_LIST_OF_ACTIONS(DEFINE_ACTION_TABLE)
};

action_func_t* match_action(const char* input) {
    action_func_t* f = NULL;

    for (size_t i = 0; i < SIZEOF_ARRAY(actions); i++) {
        if (STREQ(input, actions[i].name)) {
            f = actions[i].action;
            break;
        }
    }

    return f;
}

