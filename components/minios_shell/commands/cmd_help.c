#include "shell_internal.h"

static int cmd_help(int argc, char **argv)
{
    size_t index;

    (void)argv;
    if (argc != 1) {
        minios_shell_write("Usage: help\r\n");
        return -1;
    }

    minios_shell_write("Available commands:\r\n\r\n");
    for (index = 0U; index < minios_shell_command_count(); ++index) {
        const minios_command_t *command = minios_shell_command_at(index);
        minios_shell_printf("%-10s %s\r\n", command->name, command->description);
    }
    return 0;
}

static const minios_command_t help_command = {
    .name = "help",
    .description = "Show available commands",
    .usage = "help",
    .handler = cmd_help,
};

int minios_cmd_help_register(void)
{
    return minios_shell_register(&help_command);
}
