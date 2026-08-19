#include "shell_internal.h"

#include "minios.h"

static int cmd_info(int argc, char **argv)
{
    os_system_info_t system;

    (void)argv;
    if (argc != 1) {
        minios_shell_write("Usage: info\r\n");
        return -1;
    }

    os_get_system_info(&system);
    minios_shell_printf("MiniOS       : %s\r\n", MINIOS_VERSION);
    minios_shell_printf("API          : %d\r\n", MINIOS_API_VERSION);
    minios_shell_printf("Target       : %s\r\n", system.target);
    minios_shell_printf("CPU cores    : %lu\r\n", (unsigned long)system.cpu_cores);
    minios_shell_printf("Free memory  : %lu KB\r\n",
                        (unsigned long)(os_free_memory() / 1024U));
    return 0;
}

static const minios_command_t info_command = {
    .name = "info",
    .description = "Show system information",
    .usage = "info",
    .handler = cmd_info,
};

int minios_cmd_info_register(void)
{
    return minios_shell_register(&info_command);
}
