#include "shell_internal.h"

#include <stddef.h>

#include "cmd_fs_common.h"
#include "minios_fs.h"

static int list_entry(const char *name, int is_directory, size_t size,
                      void *context)
{
    size_t *count = (size_t *)context;

    if (is_directory) {
        minios_shell_printf("d          %s/\r\n", name);
    } else {
        minios_shell_printf("- %8u %s\r\n", (unsigned int)size, name);
    }
    ++(*count);
    return 0;
}

static int cmd_ls(int argc, char **argv)
{
    const char *path;
    size_t count = 0U;
    int result;

    if (argc > 2) {
        minios_shell_write("Usage: ls [path]\r\n");
        return -1;
    }
    path = (argc == 2) ? argv[1] : ".";
    result = os_fs_list(path, list_entry, &count);
    if (result != OS_FS_OK) {
        return minios_cmd_fs_report_error("ls", path, result);
    }
    if (count == 0U) {
        minios_shell_write("(empty)\r\n");
    }
    return 0;
}

static const minios_command_t ls_command = {
    .name = "ls",
    .description = "List directory contents",
    .usage = "ls [path]",
    .handler = cmd_ls,
};

int minios_cmd_ls_register(void)
{
    return minios_shell_register(&ls_command);
}
