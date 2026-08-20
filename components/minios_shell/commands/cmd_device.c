#include "shell_internal.h"

#include <stddef.h>
#include <string.h>

#include "minios_device.h"

static const char *device_class_name(os_device_class_t device_class)
{
    switch (device_class) {
    case OS_DEVICE_CLASS_CHARACTER:
        return "character";
    case OS_DEVICE_CLASS_CONTROLLER:
        return "controller";
    default:
        return "unknown";
    }
}

static void print_capabilities(uint32_t capabilities)
{
    int separator = 0;

    if ((capabilities & OS_DEVICE_CAP_READ) != 0U) {
        minios_shell_write("read");
        separator = 1;
    }
    if ((capabilities & OS_DEVICE_CAP_WRITE) != 0U) {
        minios_shell_write(separator ? ", write" : "write");
        separator = 1;
    }
    if ((capabilities & OS_DEVICE_CAP_CONTROL) != 0U) {
        minios_shell_write(separator ? ", control" : "control");
        separator = 1;
    }
    if (!separator) {
        minios_shell_write("none");
    }
    minios_shell_write("\r\n");
}

static int device_list(int argc)
{
    size_t index;

    if (argc != 2) {
        minios_shell_write("Usage: device list\r\n");
        return -1;
    }
    minios_shell_write("PATH             CLASS       DRIVER\r\n");
    for (index = 0U; index < os_device_count(); ++index) {
        const minios_device_t *device = os_device_at(index);

        minios_shell_printf("/dev/%-10s %-11s %s\r\n", device->name,
                            device_class_name(device->device_class),
                            device->driver);
    }
    return 0;
}

static int device_info(int argc, char **argv)
{
    const minios_device_t *device;

    if (argc != 3) {
        minios_shell_write("Usage: device info <name>\r\n");
        return -1;
    }
    device = os_device_find(argv[2]);
    if (device == NULL) {
        minios_shell_printf("device: %s: not found\r\n", argv[2]);
        return -1;
    }
    minios_shell_printf("Name:         %s\r\n", device->name);
    minios_shell_printf("Path:         /dev/%s\r\n", device->name);
    minios_shell_printf("Class:        %s\r\n",
                        device_class_name(device->device_class));
    minios_shell_printf("Driver:       %s\r\n", device->driver);
    minios_shell_printf("Description:  %s\r\n", device->description);
    minios_shell_write("Capabilities: ");
    print_capabilities(device->capabilities);
    return 0;
}

static int cmd_device(int argc, char **argv)
{
    if (argc < 2) {
        minios_shell_write("Usage: device <list|info> [name]\r\n");
        return -1;
    }
    if (strcmp(argv[1], "list") == 0) {
        return device_list(argc);
    }
    if (strcmp(argv[1], "info") == 0) {
        return device_info(argc, argv);
    }
    minios_shell_write("Unknown device operation. Use list or info.\r\n");
    return -1;
}

static const minios_command_t device_command = {
    .name = "device",
    .description = "Inspect registered devices",
    .usage = "device <list|info> [name]",
    .handler = cmd_device,
};

int minios_cmd_device_register(void)
{
    return minios_shell_register(&device_command);
}
