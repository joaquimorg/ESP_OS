#include "shell_internal.h"

#include "cmd_fs_common.h"
#include "minios_fs.h"

static int cmd_cd(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        minios_shell_write("Usage: cd <path>\r\n");
        return -1;
    }
    result = os_fs_chdir(argv[1]);
    return (result == OS_FS_OK)
               ? 0
               : minios_cmd_fs_report_error("cd", argv[1], result);
}

static const minios_command_t cd_command = {
    .name = "cd",
    .description = "Change working directory",
    .usage = "cd <path>",
    .handler = cmd_cd,
};

int minios_cmd_cd_register(void)
{
    return minios_shell_register(&cd_command);
}
