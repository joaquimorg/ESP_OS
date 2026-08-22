#include "shell_internal.h"

#include <string.h>

#include "cmd_fs_common.h"
#include "minios_fs.h"

static int cmd_echo(int argc, char **argv)
{
    char text[MINIOS_SHELL_MAX_LINE];
    const char *path;
    size_t used = 0U;
    int text_end;
    int index;
    int append = 0;
    int result;

    if (argc < 3) {
        minios_shell_write("Usage: echo <text> [>|>>] <file>\r\n");
        return -1;
    }
    path = argv[argc - 1];
    text_end = argc - 1;
    if ((argc >= 3) &&
        ((strcmp(argv[argc - 2], ">") == 0) ||
         (strcmp(argv[argc - 2], ">>") == 0))) {
        append = (argv[argc - 2][1] == '>');
        text_end = argc - 2;
    }
    for (index = 1; index < text_end; ++index) {
        size_t length = strlen(argv[index]);

        if ((used != 0U) && (used < (sizeof(text) - 1U))) {
            text[used++] = ' ';
        }
        if (length > ((sizeof(text) - 1U) - used)) {
            minios_shell_write("echo: text is too long\r\n");
            return -1;
        }
        memcpy(text + used, argv[index], length);
        used += length;
    }
    if (used == 0U) {
        minios_shell_write("Usage: echo <text> [>|>>] <file>\r\n");
        return -1;
    }
    text[used] = '\0';
    result = os_fs_write(path, text, append);
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
