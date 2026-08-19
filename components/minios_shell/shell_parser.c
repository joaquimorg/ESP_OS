#include "shell_internal.h"

#include <ctype.h>
#include <stddef.h>

int minios_shell_parse(char *line, char **argv, int max_args)
{
    char *cursor;
    int argc = 0;

    if ((line == NULL) || (argv == NULL) || (max_args <= 0)) {
        return -1;
    }

    cursor = line;
    while (*cursor != '\0') {
        while (isspace((unsigned char)*cursor) != 0) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        if (argc >= max_args) {
            return -2;
        }

        argv[argc] = cursor;
        ++argc;
        while ((*cursor != '\0') && (isspace((unsigned char)*cursor) == 0)) {
            ++cursor;
        }
        if (*cursor != '\0') {
            *cursor = '\0';
            ++cursor;
        }
    }

    return argc;
}
