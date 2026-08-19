#pragma once

#include <stddef.h>

#include "minios_console.h"

#define MINIOS_SHELL_MAX_LINE 128
#define MINIOS_SHELL_MAX_ARGS 12
#define MINIOS_SHELL_MAX_COMMANDS 16

typedef int (*minios_command_handler_t)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *description;
    const char *usage;
    minios_command_handler_t handler;
} minios_command_t;

int minios_shell_init(minios_console_t *console);
int minios_shell_register(const minios_command_t *command);
size_t minios_shell_command_count(void);
const minios_command_t *minios_shell_command_at(size_t index);
int minios_shell_write(const char *text);
int minios_shell_printf(const char *format, ...);
void minios_shell_run(void);
