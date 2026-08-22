#include "minios_shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "minios.h"
#include "minios_fs.h"
#include "shell_internal.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MINIOS_SHELL_FORMAT_BUFFER 192

static minios_console_t *uart_console;
static minios_console_t *command_console;
static const minios_command_t *command_registry[MINIOS_SHELL_MAX_COMMANDS];
static size_t command_count;
static StaticSemaphore_t command_mutex_storage;
static SemaphoreHandle_t command_mutex;

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
        (minios_cmd_module_register() != 0) ||
#if CONFIG_MINIOS_ENABLE_NETWORK
        (minios_cmd_wifi_register() != 0) ||
        (minios_cmd_ifconfig_register() != 0) ||
        (minios_cmd_ping_register() != 0) ||
#endif
        (minios_cmd_gpio_register() != 0) ||
        (minios_cmd_i2c_register() != 0) ||
        (minios_cmd_spi_register() != 0) ||
        (minios_cmd_ls_register() != 0) ||
        (minios_cmd_cd_register() != 0) ||
        (minios_cmd_pwd_register() != 0) ||
        (minios_cmd_cat_register() != 0) ||
        (minios_cmd_echo_register() != 0) ||
        (minios_cmd_mkdir_register() != 0) ||
        (minios_cmd_rm_register() != 0) ||
        (minios_cmd_run_register() != 0) ||
        (minios_cmd_source_register() != 0)) {
        return -1;
    }
    return 0;
}

int minios_shell_init(minios_console_t *console)
{
    if (console == NULL) {
        return -1;
    }

    uart_console = console;
    command_console = console;
    command_mutex = xSemaphoreCreateMutexStatic(&command_mutex_storage);
    if (command_mutex == NULL) {
        return -1;
    }
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
    return minios_console_write_text(command_console, text);
}

int minios_shell_write_bytes(const char *data, size_t length)
{
    return minios_console_write(command_console, data, length);
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
    return minios_console_write(command_console, buffer, (size_t)length);
}

int minios_shell_execute_line_locked(char *line)
{
    char *argv[MINIOS_SHELL_MAX_ARGS];
    const minios_command_t *command;
    int argc;

    argc = minios_shell_parse(line, argv, MINIOS_SHELL_MAX_ARGS);
    if (argc == -2) {
        minios_shell_write("Error: too many arguments\r\n");
        return -1;
    }
    if (argc <= 0) {
        return (argc == 0) ? 0 : -1;
    }

    command = find_command(argv[0]);
    if (command == NULL) {
        minios_shell_printf("Unknown command: %s\r\n", argv[0]);
        return -1;
    }
    return command->handler(argc, argv);
}

static void execute_line(minios_console_t *console, char *line)
{
    if (xSemaphoreTake(command_mutex, portMAX_DELAY) != pdTRUE) {
        minios_console_write_text(console, "Error: shell unavailable\r\n");
        return;
    }
    command_console = console;
    (void)minios_shell_execute_line_locked(line);
    command_console = uart_console;
    xSemaphoreGive(command_mutex);
}

int minios_shell_run_startup(void)
{
    int result;

    if ((command_mutex == NULL) ||
        (xSemaphoreTake(command_mutex, portMAX_DELAY) != pdTRUE)) {
        return MINIOS_SCRIPT_ERROR;
    }
    command_console = uart_console;
    result = minios_script_execute("/boot/startup.rc", -1);
    command_console = uart_console;
    xSemaphoreGive(command_mutex);
    return result;
}

static void write_prompt(minios_console_t *console)
{
    char cwd[OS_FS_PATH_MAX];

    if (xSemaphoreTake(command_mutex, portMAX_DELAY) != pdTRUE) {
        minios_console_write_text(console, "minios:?> ");
        return;
    }
    if (os_fs_getcwd(cwd, sizeof(cwd)) == OS_FS_OK) {
        char prompt[OS_FS_PATH_MAX + 12U];
        int prompt_length = snprintf(prompt, sizeof(prompt),
                                     "minios:%s> ", cwd);

        if (prompt_length > 0) {
            minios_console_write(
                console, prompt,
                (size_t)prompt_length < sizeof(prompt)
                    ? (size_t)prompt_length
                    : sizeof(prompt) - 1U);
        }
    } else {
        minios_console_write_text(console, "minios:?> ");
    }
    xSemaphoreGive(command_mutex);
}

void minios_shell_run_console(minios_console_t *console)
{
    char line[MINIOS_SHELL_MAX_LINE];
    size_t length = 0U;
    int discard_line = 0;
    int ignore_lf = 0;

    if (console == NULL) {
        return;
    }
    minios_console_write_text(
        console, "Type 'help' for available commands.\r\n\r\n");
    write_prompt(console);

    for (;;) {
        char character;
        int received = minios_console_read(console, &character, 1U);

        if (received < 0) {
            break;
        }
        if (received == 0) {
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
            minios_console_write_text(console, "\r\n");
            if (discard_line) {
                minios_console_write_text(
                    console, "Error: command line too long\r\n");
            } else {
                line[length] = '\0';
                execute_line(console, line);
            }
            length = 0U;
            discard_line = 0;
            write_prompt(console);
            continue;
        }

        if ((character == '\b') || (character == 0x7f)) {
            if (!discard_line && (length > 0U)) {
                --length;
                minios_console_write_text(console, "\b \b");
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
        (void)minios_console_write(console, &character, 1U);
    }
}

void minios_shell_run(void)
{
    minios_shell_run_console(uart_console);
}
