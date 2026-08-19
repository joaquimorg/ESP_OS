#include "shell_internal.h"

#include "minios.h"

static int cmd_version(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        minios_shell_write("Usage: version\r\n");
        return -1;
    }

    minios_shell_printf("%s %s\r\nAPI version %d\r\n",
                        MINIOS_NAME, MINIOS_VERSION, MINIOS_API_VERSION);
    return 0;
}

static const minios_command_t version_command = {
    .name = "version",
    .description = "Show MiniOS version",
    .usage = "version",
    .handler = cmd_version,
};

int minios_cmd_version_register(void)
{
    return minios_shell_register(&version_command);
}
