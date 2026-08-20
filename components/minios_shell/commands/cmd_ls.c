#include "shell_internal.h"

#include <stddef.h>
#include <string.h>

#include "cmd_fs_common.h"
#include "minios_device.h"
#include "minios_fs.h"

typedef struct {
    size_t count;
    int is_device_directory;
} list_context_t;

static int list_entry(const char *name, int is_directory, size_t size,
                      void *context)
{
    list_context_t *list = (list_context_t *)context;

    if (list->is_device_directory && (os_device_find(name) != NULL)) {
        return 0;
    }

    if (is_directory) {
        minios_shell_printf("d          %s/\r\n", name);
    } else {
        minios_shell_printf("- %8u %s\r\n", (unsigned int)size, name);
    }
    ++list->count;
    return 0;
}

static void list_devices(list_context_t *context)
{
    size_t index;

    for (index = 0U; index < os_device_count(); ++index) {
        const minios_device_t *device = os_device_at(index);

        minios_shell_printf("c          %s\r\n", device->name);
        ++context->count;
    }
}

static int cmd_ls(int argc, char **argv)
{
    const char *path;
    char resolved[OS_FS_PATH_MAX];
    list_context_t context = {0};
    int result;

    if (argc > 2) {
        minios_shell_write("Usage: ls [path]\r\n");
        return -1;
    }
    path = (argc == 2) ? argv[1] : ".";
    result = os_fs_resolve_path(path, resolved, sizeof(resolved));
    if (result != OS_FS_OK) {
        return minios_cmd_fs_report_error("ls", path, result);
    }
    context.is_device_directory = (strcmp(resolved, "/dev") == 0);
    result = os_fs_list(path, list_entry, &context);
    if (result != OS_FS_OK) {
        return minios_cmd_fs_report_error("ls", path, result);
    }
    if (context.is_device_directory) {
        list_devices(&context);
    }
    if (context.count == 0U) {
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
