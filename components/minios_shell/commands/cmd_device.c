#include "shell_internal.h"

#include <stddef.h>
#include <stdio.h>
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

static int report_device_error(const char *operation, const char *name,
                               int result)
{
    const char *reason;

    switch (result) {
    case OS_DEVICE_INVALID_ARGUMENT:
        reason = "invalid argument";
        break;
    case OS_DEVICE_NOT_FOUND:
        reason = "not found";
        break;
    case OS_DEVICE_NOT_SUPPORTED:
        reason = "operation not supported";
        break;
    case OS_DEVICE_BUSY:
        reason = "device busy";
        break;
    default:
        reason = "device error";
        break;
    }
    minios_shell_printf("device %s: %s: %s\r\n", operation, name, reason);
    return -1;
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

static int device_write(int argc, char **argv)
{
    char text[MINIOS_SHELL_MAX_LINE];
    size_t used = 0U;
    int text_start = 3;
    int index;
    int result;

    if (argc < 4) {
        minios_shell_write(
            "Usage: device write <name> [--at <x> <y>] <text>\r\n");
        return -1;
    }
    if (strcmp(argv[3], "--at") == 0) {
        char position[24];
        int written;

        if (argc < 7) {
            minios_shell_write(
                "Usage: device write <name> --at <x> <y> <text>\r\n");
            return -1;
        }
        written = snprintf(position, sizeof(position), "%s %s", argv[4],
                           argv[5]);
        if ((written < 0) || ((size_t)written >= sizeof(position))) {
            return report_device_error("position", argv[2],
                                       OS_DEVICE_INVALID_ARGUMENT);
        }
        result = os_device_control(argv[2], "position", position);
        if (result != OS_DEVICE_OK) {
            return report_device_error("position", argv[2], result);
        }
        text_start = 6;
    }
    for (index = text_start; index < argc; ++index) {
        size_t length = strlen(argv[index]);

        if (used != 0U) {
            if (used >= (sizeof(text) - 1U)) {
                minios_shell_write("device write: text is too long\r\n");
                return -1;
            }
            text[used++] = ' ';
        }
        if (length > ((sizeof(text) - 1U) - used)) {
            minios_shell_write("device write: text is too long\r\n");
            return -1;
        }
        memcpy(text + used, argv[index], length);
        used += length;
    }
    text[used] = '\0';
    result = os_device_write(argv[2], text, used);
    return (result == OS_DEVICE_OK)
               ? 0 : report_device_error("write", argv[2], result);
}

static int device_control(int argc, char **argv)
{
    char value_buffer[MINIOS_SHELL_MAX_LINE];
    const char *value = NULL;
    size_t used = 0U;
    int index;
    int result;

    if (argc < 4) {
        minios_shell_write(
            "Usage: device control <name> <operation> [value ...]\r\n");
        return -1;
    }
    for (index = 4; index < argc; ++index) {
        size_t length = strlen(argv[index]);

        if (used != 0U) {
            if (used >= (sizeof(value_buffer) - 1U)) {
                return report_device_error("control", argv[2],
                                           OS_DEVICE_INVALID_ARGUMENT);
            }
            value_buffer[used++] = ' ';
        }
        if (length > ((sizeof(value_buffer) - 1U) - used)) {
            return report_device_error("control", argv[2],
                                       OS_DEVICE_INVALID_ARGUMENT);
        }
        memcpy(value_buffer + used, argv[index], length);
        used += length;
    }
    if (used != 0U) {
        value_buffer[used] = '\0';
        value = value_buffer;
    }
    result = os_device_control(argv[2], argv[3], value);
    return (result == OS_DEVICE_OK)
               ? 0 : report_device_error("control", argv[2], result);
}

static int cmd_device(int argc, char **argv)
{
    if (argc < 2) {
        minios_shell_write(
            "Usage: device <list|info|write|control> ...\r\n");
        return -1;
    }
    if (strcmp(argv[1], "list") == 0) {
        return device_list(argc);
    }
    if (strcmp(argv[1], "info") == 0) {
        return device_info(argc, argv);
    }
    if (strcmp(argv[1], "write") == 0) {
        return device_write(argc, argv);
    }
    if (strcmp(argv[1], "control") == 0) {
        return device_control(argc, argv);
    }
    minios_shell_write(
        "Unknown device operation. Use list, info, write, or control.\r\n");
    return -1;
}

static const minios_command_t device_command = {
    .name = "device",
    .description = "Inspect registered devices",
    .usage = "device <list|info|write|control> ...",
    .handler = cmd_device,
};

int minios_cmd_device_register(void)
{
    return minios_shell_register(&device_command);
}
