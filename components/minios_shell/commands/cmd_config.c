#include "shell_internal.h"

#include <stddef.h>
#include <string.h>

#include "minios_config.h"

static int report_config_error(int result, const char *key)
{
    if (result == OS_CONFIG_NOT_FOUND) {
        minios_shell_printf("Config key not found: %s\r\n", key);
    } else if (result == OS_CONFIG_INVALID_ARGUMENT) {
        minios_shell_write("Error: invalid configuration key or value\r\n");
    } else if (result == OS_CONFIG_BUFFER_TOO_SMALL) {
        minios_shell_write("Error: configuration value is too long\r\n");
    } else if (result == OS_CONFIG_KEY_COLLISION) {
        minios_shell_write("Error: configuration key collision\r\n");
    } else {
        minios_shell_write("Error: configuration storage failure\r\n");
    }
    return -1;
}

static int list_config(const char *key, const char *value, void *context)
{
    size_t *count = (size_t *)context;

    minios_shell_printf("%s=%s\r\n", key, value);
    ++(*count);
    return 0;
}

static int cmd_config_get(int argc, char **argv)
{
    char value[OS_CONFIG_VALUE_MAX_LENGTH + 1U];
    int result;

    if (argc != 3) {
        minios_shell_write("Usage: config get <key>\r\n");
        return -1;
    }

    result = os_config_get(argv[2], value, sizeof(value));
    if (result != OS_CONFIG_OK) {
        return report_config_error(result, argv[2]);
    }
    minios_shell_printf("%s=%s\r\n", argv[2], value);
    return 0;
}

static int cmd_config_set(int argc, char **argv)
{
    int result;

    if (argc != 4) {
        minios_shell_write("Usage: config set <key> <value>\r\n");
        return -1;
    }

    result = os_config_set(argv[2], argv[3]);
    if (result != OS_CONFIG_OK) {
        return report_config_error(result, argv[2]);
    }
    minios_shell_write("OK\r\n");
    return 0;
}

static int cmd_config_list(int argc)
{
    size_t count = 0U;
    int result;

    if (argc != 2) {
        minios_shell_write("Usage: config list\r\n");
        return -1;
    }

    result = os_config_list(list_config, &count);
    if (result != OS_CONFIG_OK) {
        return report_config_error(result, "");
    }
    if (count == 0U) {
        minios_shell_write("(empty)\r\n");
    }
    return 0;
}

static int cmd_config_delete(int argc, char **argv)
{
    int result;

    if (argc != 3) {
        minios_shell_write("Usage: config delete <key>\r\n");
        return -1;
    }

    result = os_config_delete(argv[2]);
    if (result != OS_CONFIG_OK) {
        return report_config_error(result, argv[2]);
    }
    minios_shell_write("OK\r\n");
    return 0;
}

static int cmd_config(int argc, char **argv)
{
    if (argc < 2) {
        minios_shell_write(
            "Usage: config <get|set|list|delete> [key] [value]\r\n");
        return -1;
    }
    if (strcmp(argv[1], "get") == 0) {
        return cmd_config_get(argc, argv);
    }
    if (strcmp(argv[1], "set") == 0) {
        return cmd_config_set(argc, argv);
    }
    if (strcmp(argv[1], "list") == 0) {
        return cmd_config_list(argc);
    }
    if (strcmp(argv[1], "delete") == 0) {
        return cmd_config_delete(argc, argv);
    }

    minios_shell_write("Unknown config operation. Use get, set, list, or delete.\r\n");
    return -1;
}

static const minios_command_t config_command = {
    .name = "config",
    .description = "Manage persistent configuration",
    .usage = "config <get|set|list|delete> [key] [value]",
    .handler = cmd_config,
};

int minios_cmd_config_register(void)
{
    return minios_shell_register(&config_command);
}
