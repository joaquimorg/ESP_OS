#include "shell_internal.h"

#include "cmd_fs_common.h"
#include "minios_fs.h"

static int cmd_pwd(int argc, char **argv)
{
    char path[OS_FS_PATH_MAX];
    int result;

    (void)argv;
    if (argc != 1) {
        minios_shell_write("Usage: pwd\r\n");
        return -1;
    }
    result = os_fs_getcwd(path, sizeof(path));
    if (result != OS_FS_OK) {
        return minios_cmd_fs_report_error("pwd", NULL, result);
    }
    minios_shell_printf("%s\r\n", path);
    return 0;
}

static const minios_command_t pwd_command = {
    .name = "pwd",
    .description = "Print working directory",
    .usage = "pwd",
    .handler = cmd_pwd,
};

int minios_cmd_pwd_register(void)
{
    return minios_shell_register(&pwd_command);
}
