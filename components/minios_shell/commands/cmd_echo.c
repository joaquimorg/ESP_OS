#include "shell_internal.h"

#include <string.h>

#include "cmd_fs_common.h"
#include "minios_fs.h"

static int cmd_echo(int argc, char **argv)
{
    const char *path;
    int append = 0;
    int result;

    if (argc == 3) {
        path = argv[2];
    } else if ((argc == 4) &&
               ((strcmp(argv[2], ">") == 0) ||
                (strcmp(argv[2], ">>") == 0))) {
        path = argv[3];
        append = (argv[2][1] == '>');
    } else {
        minios_shell_write("Usage: echo <text> [>|>>] <file>\r\n");
        return -1;
    }
    result = os_fs_write(path, argv[1], append);
    return (result == OS_FS_OK)
               ? 0
               : minios_cmd_fs_report_error("echo", path, result);
}

static const minios_command_t echo_command = {
    .name = "echo",
    .description = "Write text to a file",
    .usage = "echo <text> [>|>>] <file>",
    .handler = cmd_echo,
};

int minios_cmd_echo_register(void)
{
    return minios_shell_register(&echo_command);
}
