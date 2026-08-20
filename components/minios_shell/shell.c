#include "minios_shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "minios.h"
#include "minios_fs.h"
#include "shell_internal.h"

#define MINIOS_SHELL_FORMAT_BUFFER 192

static minios_console_t *shell_console;
static const minios_command_t *command_registry[MINIOS_SHELL_MAX_COMMANDS];
static size_t command_count;

static const minios_command_t *find_command(const char *name)
{
    size_t index;

    for (index = 0U; index < command_count; ++index) {
        if (strcmp(command_registry[index]->name, name) == 0) {
            return command_registry[index];
        }
    }
    return NULL;
}

static int register_builtin_commands(void)
{
    if ((minios_cmd_help_register() != 0) ||
        (minios_cmd_config_register() != 0) ||
        (minios_cmd_version_register() != 0) ||
        (minios_cmd_info_register() != 0) ||
        (minios_cmd_mem_register() != 0) ||
        (minios_cmd_uptime_register() != 0) ||
        (minios_cmd_reboot_register() != 0) ||
        (minios_cmd_clear_register() != 0) ||
        (minios_cmd_device_register() != 0) ||
        (minios_cmd_gpio_register() != 0) ||
        (minios_cmd_i2c_register() != 0) ||
        (minios_cmd_spi_register() != 0) ||
        (minios_cmd_ls_register() != 0) ||
        (minios_cmd_cd_register() != 0) ||
        (minios_cmd_pwd_register() != 0) ||
        (minios_cmd_cat_register() != 0) ||
        (minios_cmd_echo_register() != 0) ||
        (minios_cmd_mkdir_register() != 0) ||
        (minios_cmd_rm_register() != 0)) {
        return -1;
    }
    return 0;
}

int minios_shell_init(minios_console_t *console)
{
    if (console == NULL) {
        return -1;
    }

    shell_console = console;
    command_count = 0U;
    return register_builtin_commands();
}

int minios_shell_register(const minios_command_t *command)
{
    if ((command == NULL) || (command->name == NULL) ||
        (command->name[0] == '\0') || (command->handler == NULL) ||
        (command_count >= MINIOS_SHELL_MAX_COMMANDS) ||
        (find_command(command->name) != NULL)) {
        return -1;
    }

    command_registry[command_count] = command;
    ++command_count;
    return 0;
}

size_t minios_shell_command_count(void)
{
    return command_count;
}

const minios_command_t *minios_shell_command_at(size_t index)
{
    if (index >= command_count) {
        return NULL;
    }
    return command_registry[index];
}

int minios_shell_write(const char *text)
{
    return minios_console_write_text(shell_console, text);
}

int minios_shell_write_bytes(const char *data, size_t length)
{
    return minios_console_write(shell_console, data, length);
}

int minios_shell_printf(const char *format, ...)
{
    char buffer[MINIOS_SHELL_FORMAT_BUFFER];
    va_list arguments;
    int length;

    if (format == NULL) {
        return -1;
    }

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (length < 0) {
        return -1;
    }
    if ((size_t)length >= sizeof(buffer)) {
        length = (int)(sizeof(buffer) - 1U);
    }
    return minios_console_write(shell_console, buffer, (size_t)length);
}

static void execute_line(char *line)
{
    char *argv[MINIOS_SHELL_MAX_ARGS];
    const minios_command_t *command;
    int argc;

    argc = minios_shell_parse(line, argv, MINIOS_SHELL_MAX_ARGS);
    if (argc == -2) {
        minios_shell_write("Error: too many arguments\r\n");
        return;
    }
    if (argc <= 0) {
        return;
    }

    command = find_command(argv[0]);
    if (command == NULL) {
        minios_shell_printf("Unknown command: %s\r\n", argv[0]);
        return;
    }

    (void)command->handler(argc, argv);
}

void minios_shell_run(void)
{
    char line[MINIOS_SHELL_MAX_LINE];
    size_t length = 0U;
    int discard_line = 0;
    int ignore_lf = 0;

    minios_shell_write("Type 'help' for available commands.\r\n\r\nminios:/> ");

    for (;;) {
        char character;
        int received = minios_console_read(shell_console, &character, 1U);

        if (received <= 0) {
            os_sleep(10U);
            continue;
        }

        if ((character == '\n') && ignore_lf) {
            ignore_lf = 0;
            continue;
        }
        ignore_lf = 0;

        if ((character == '\r') || (character == '\n')) {
            ignore_lf = (character == '\r');
            minios_shell_write("\r\n");
            if (discard_line) {
                minios_shell_write("Error: command line too long\r\n");
            } else {
                line[length] = '\0';
                execute_line(line);
            }
            length = 0U;
            discard_line = 0;
            {
                char cwd[OS_FS_PATH_MAX];
                if (os_fs_getcwd(cwd, sizeof(cwd)) == OS_FS_OK) {
                    minios_shell_printf("minios:%s> ", cwd);
                } else {
                    minios_shell_write("minios:?> ");
                }
            }
            continue;
        }

        if ((character == '\b') || (character == 0x7f)) {
            if (!discard_line && (length > 0U)) {
                --length;
                minios_shell_write("\b \b");
            }
            continue;
        }

        if ((unsigned char)character < 0x20U) {
            continue;
        }

        if (discard_line || (length >= (sizeof(line) - 1U))) {
            discard_line = 1;
            continue;
        }

        line[length] = character;
        ++length;
        (void)minios_console_write(shell_console, &character, 1U);
    }
}
