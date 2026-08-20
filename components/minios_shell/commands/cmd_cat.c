#include "shell_internal.h"

#include <stddef.h>

#include "cmd_fs_common.h"
#include "minios_fs.h"

static int write_file_data(const char *data, size_t length, void *context)
{
    (void)context;
    return (minios_shell_write_bytes(data, length) < 0) ? -1 : 0;
}

static int cmd_cat(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        minios_shell_write("Usage: cat <file>\r\n");
        return -1;
    }
    result = os_fs_read(argv[1], write_file_data, NULL);
    if (result != OS_FS_OK) {
        return minios_cmd_fs_report_error("cat", argv[1], result);
    }
    return 0;
}

static const minios_command_t cat_command = {
    .name = "cat",
    .description = "Display a file",
    .usage = "cat <file>",
    .handler = cmd_cat,
};

int minios_cmd_cat_register(void)
{
    return minios_shell_register(&cat_command);
}
