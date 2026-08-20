#include "shell_internal.h"

#include "cmd_fs_common.h"
#include "minios_fs.h"

static int cmd_rm(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        minios_shell_write("Usage: rm <path>\r\n");
        return -1;
    }
    result = os_fs_remove(argv[1]);
    return (result == OS_FS_OK)
               ? 0
               : minios_cmd_fs_report_error("rm", argv[1], result);
}

static const minios_command_t rm_command = {
    .name = "rm",
    .description = "Remove a file or empty directory",
    .usage = "rm <path>",
    .handler = cmd_rm,
};

int minios_cmd_rm_register(void)
{
    return minios_shell_register(&rm_command);
}
