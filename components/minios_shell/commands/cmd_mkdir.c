#include "shell_internal.h"

#include "cmd_fs_common.h"
#include "minios_fs.h"

static int cmd_mkdir(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        minios_shell_write("Usage: mkdir <path>\r\n");
        return -1;
    }
    result = os_fs_mkdir(argv[1]);
    return (result == OS_FS_OK)
               ? 0
               : minios_cmd_fs_report_error("mkdir", argv[1], result);
}

static const minios_command_t mkdir_command = {
    .name = "mkdir",
    .description = "Create a directory",
    .usage = "mkdir <path>",
    .handler = cmd_mkdir,
};

int minios_cmd_mkdir_register(void)
{
    return minios_shell_register(&mkdir_command);
}
