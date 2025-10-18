#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "collections/string.h"

#define GROWTH_FACTOR 1.5

void string_clear(struct string *str) {
    str->len = 0;
    if (str->str != NULL) {
        str->str[0] = '\0';
    }
}

void string_free(struct string *str) {
    str->len = str->capacity = 0;
    free(str->str);
    str->str = NULL;
}

static bool string_ensure_capacity(struct string *str, size_t cap) {
    if (str->capacity < cap) {
        size_t new_cap;
        if (str->capacity * GROWTH_FACTOR > cap) {
            new_cap = str->capacity * GROWTH_FACTOR;
        } else {
            new_cap = cap;
        }

        char* new_str = realloc(str->str, new_cap);
        if (new_str == NULL) {
            return false;
        }

        str->capacity = new_cap;
        str->str = new_str;
    }

    return true;
}

bool string_reserve(struct string* str, size_t len) {
    return string_ensure_capacity(str, len + 1);
}

bool string_append(struct string *str, const char *suffix) {
    return string_appendn(str, suffix, strlen(suffix));
}

bool string_appendn(struct string *str, const char *suffix, size_t suffix_len) {
    if (!string_ensure_capacity(str, str->len + suffix_len + 1)) {
        return false;
    }

    memcpy(str->str + str->len, suffix, suffix_len);
    str->len += suffix_len;
    str->str[str->len] = '\0';

    return true;
}

bool string_appendf(struct string *str, const char *fmt, ...) {
    va_list args, args_copy;
    va_start(args, fmt);

    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (len < 0) {
        goto err;
    }

    if (!string_ensure_capacity(str, str->len + len + 1)) {
        goto err;
    }

    int written = vsnprintf(str->str + str->len, len + 1, fmt, args);
    if (written < 0) {
        goto err;
    }

    str->len += written;

    va_end(args);
    return len;

err:
    va_end(args);
    return false;
}

bool string_appendfn(struct string *str, size_t maxlen, const char *fmt, ...) {
    if (!string_ensure_capacity(str, str->len + maxlen + 1)) {
        return false;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(str->str + str->len, maxlen + 1, fmt, args);
    va_end(args);

    if (written < 0) {
        return false;
    }

    str->len += written;

    return true;
}

