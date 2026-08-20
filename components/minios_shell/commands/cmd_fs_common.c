#include "cmd_fs_common.h"

#include "minios_fs.h"
#include "shell_internal.h"

int minios_cmd_fs_report_error(const char *operation, const char *path,
                               int result)
{
    const char *reason;

    switch (result) {
    case OS_FS_INVALID_ARGUMENT:
        reason = "invalid argument";
        break;
    case OS_FS_NOT_FOUND:
        reason = "not found";
        break;
    case OS_FS_NOT_DIRECTORY:
        reason = "not a directory";
        break;
    case OS_FS_IS_DIRECTORY:
        reason = "is a directory";
        break;
    case OS_FS_ALREADY_EXISTS:
        reason = "already exists";
        break;
    case OS_FS_NOT_EMPTY:
        reason = "directory not empty";
        break;
    case OS_FS_PATH_TOO_LONG:
        reason = "path too long";
        break;
    case OS_FS_IS_DEVICE:
        reason = "device path is not a regular file";
        break;
    default:
        reason = "filesystem error";
        break;
    }
    minios_shell_printf("%s: %s: %s\r\n", operation,
                        (path == NULL) ? "" : path, reason);
    return -1;
}
