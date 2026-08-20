#include "minios_device.h"

#include <stdbool.h>
#include <string.h>

static const minios_device_t *device_registry[OS_DEVICE_MAX];
static size_t device_count;
static bool device_initialized;

static const minios_device_t uart0_device = {
    .name = "uart0",
    .device_class = OS_DEVICE_CLASS_CHARACTER,
    .driver = "console-vfs",
    .description = "System console character device",
    .capabilities = OS_DEVICE_CAP_READ | OS_DEVICE_CAP_WRITE,
};

static const minios_device_t gpio_device = {
    .name = "gpio",
    .device_class = OS_DEVICE_CLASS_CONTROLLER,
    .driver = "gpio",
    .description = "General-purpose I/O controller",
    .capabilities = OS_DEVICE_CAP_CONTROL,
};

static bool device_name_is_valid(const char *name)
{
    size_t length;
    size_t index;

    if (name == NULL) {
        return false;
    }
    length = strnlen(name, OS_DEVICE_NAME_MAX + 1U);
    if ((length == 0U) || (length > OS_DEVICE_NAME_MAX)) {
        return false;
    }
    for (index = 0U; index < length; ++index) {
        char character = name[index];
        bool valid = ((character >= 'a') && (character <= 'z')) ||
                     ((character >= 'A') && (character <= 'Z')) ||
                     ((character >= '0') && (character <= '9')) ||
                     (character == '_') || (character == '-');
        if (!valid) {
            return false;
        }
    }
    return true;
}

static const char *device_basename(const char *name)
{
    static const char device_prefix[] = "/dev/";

    if ((name != NULL) &&
        (strncmp(name, device_prefix, sizeof(device_prefix) - 1U) == 0)) {
        return name + sizeof(device_prefix) - 1U;
    }
    return name;
}

const minios_device_t *os_device_find(const char *name)
{
    const char *basename = device_basename(name);
    size_t index;

    if (!device_initialized || !device_name_is_valid(basename)) {
        return NULL;
    }
    for (index = 0U; index < device_count; ++index) {
        if (strcmp(device_registry[index]->name, basename) == 0) {
            return device_registry[index];
        }
    }
    return NULL;
}

int os_device_register(const minios_device_t *device)
{
    if (!device_initialized || (device == NULL) ||
        !device_name_is_valid(device->name) || (device->driver == NULL) ||
        (device->description == NULL)) {
        return OS_DEVICE_INVALID_ARGUMENT;
    }
    if (os_device_find(device->name) != NULL) {
        return OS_DEVICE_ALREADY_EXISTS;
    }
    if (device_count >= OS_DEVICE_MAX) {
        return OS_DEVICE_REGISTRY_FULL;
    }
    device_registry[device_count++] = device;
    return OS_DEVICE_OK;
}

int os_device_init(void)
{
    device_count = 0U;
    device_initialized = true;

    if ((os_device_register(&uart0_device) != OS_DEVICE_OK) ||
        (os_device_register(&gpio_device) != OS_DEVICE_OK)) {
        device_count = 0U;
        device_initialized = false;
        return OS_DEVICE_ERROR;
    }
    return OS_DEVICE_OK;
}

size_t os_device_count(void)
{
    return device_initialized ? device_count : 0U;
}

const minios_device_t *os_device_at(size_t index)
{
    if (!device_initialized || (index >= device_count)) {
        return NULL;
    }
    return device_registry[index];
}
