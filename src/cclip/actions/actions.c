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

