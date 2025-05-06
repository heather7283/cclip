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
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "action_list.h"
#include "cclip.h"
#include "utils.h"

static void print_help_and_exit(FILE *stream, int rc) {
    const char *help =
        "Usage:\n"
        "    cclip list [-t] ROWS\n"
        "\n"
        "Command line options:\n"
        "    -t              Only list entries with non-empty tag\n"
        "    ROWS            Comma-separated list of rows to print\n"
    ;

    fprintf(stream, "%s", help);
    exit(rc);
}

static int print_row(void* data, int argc, char** argv, char** column_names) {
    for (int i = 0; i < argc - 1; i++) {
        printf("%s\t", argv[i]);
    }
    printf("%s\n", argv[argc - 1]);
    return 0;
}

int action_list(int argc, char** argv) {
    bool only_tagged = false;

    int opt;
    optind = 0;
    while ((opt = getopt(argc, argv, ":th")) != -1) {
        switch (opt) {
        case 't':
            only_tagged = true;
            break;
        case 'h':
            print_help_and_exit(stdout, 0);
            break;
        case '?':
            fprintf(stderr, "unknown option: %c\n\n", optopt);
            print_help_and_exit(stderr, 1);
            break;
        case ':':
            fprintf(stderr, "missing arg for %c\n\n", optopt);
            print_help_and_exit(stderr, 1);
            break;
        default:
            fprintf(stderr, "error while parsing command line options\n\n");
            print_help_and_exit(stderr, 1);
            break;
        }
    }
    argc = argc - optind;
    argv = &argv[optind];

    const char* fields = NULL;
    if (argc < 1) {
        fields = "rowid,mime_type,preview";
    } else if (argc == 1) {
        fields = build_field_list(argv[0]);
        if (fields == NULL) {
            return 1;
        }
    } else {
        fprintf(stderr, "extra arguments on the command line\n");
        return 1;
    }

    const char sql1[] = "SELECT ";
    const char sql2[] = " FROM history";
    const char sql3[] = " WHERE tag != ''";
    const char sql4[] = " ORDER BY timestamp DESC";
    char sql[MAX_FIELD_LIST_SIZE + sizeof(sql1) + sizeof(sql2) + sizeof(sql3) + sizeof(sql4)];
    snprintf(sql, sizeof(sql), "%s%s%s%s%s", sql1, fields, sql2, only_tagged ? sql3 : "", sql4);

    char* errmsg;
    if (sqlite3_exec(db, sql, print_row, NULL, &errmsg) != SQLITE_OK) {
        fprintf(stderr, "sqlite error: %s\n", errmsg);
        return 1;
    }

    return 0;
}

