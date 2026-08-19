#include "shell_internal.h"

static int cmd_clear(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        minios_shell_write("Usage: clear\r\n");
        return -1;
    }

    minios_shell_write("\033[2J\033[H");
    return 0;
}

static const minios_command_t clear_command = {
    .name = "clear",
    .description = "Clear terminal",
    .usage = "clear",
    .handler = cmd_clear,
};

int minios_cmd_clear_register(void)
{
    return minios_shell_register(&clear_command);
}
