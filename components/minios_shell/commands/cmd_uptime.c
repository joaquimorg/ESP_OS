#include "shell_internal.h"

#include "minios.h"

static int cmd_uptime(int argc, char **argv)
{
    uint32_t seconds;
    uint32_t hours;
    uint32_t minutes;

    (void)argv;
    if (argc != 1) {
        minios_shell_write("Usage: uptime\r\n");
        return -1;
    }

    seconds = os_uptime_ms() / 1000U;
    hours = seconds / 3600U;
    minutes = (seconds % 3600U) / 60U;
    seconds %= 60U;
    minios_shell_printf("Uptime: %02lu:%02lu:%02lu\r\n",
                        (unsigned long)hours,
                        (unsigned long)minutes,
                        (unsigned long)seconds);
    return 0;
}

static const minios_command_t uptime_command = {
    .name = "uptime",
    .description = "Show system uptime",
    .usage = "uptime",
    .handler = cmd_uptime,
};

int minios_cmd_uptime_register(void)
{
    return minios_shell_register(&uptime_command);
}
