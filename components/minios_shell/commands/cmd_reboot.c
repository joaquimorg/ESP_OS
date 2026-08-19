#include "shell_internal.h"

#include "minios.h"

static int cmd_reboot(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        minios_shell_write("Usage: reboot\r\n");
        return -1;
    }

    minios_shell_write("Restarting...\r\n");
    os_sleep(100U);
    os_reboot();
    return 0;
}

static const minios_command_t reboot_command = {
    .name = "reboot",
    .description = "Restart system",
    .usage = "reboot",
    .handler = cmd_reboot,
};

int minios_cmd_reboot_register(void)
{
    return minios_shell_register(&reboot_command);
}
