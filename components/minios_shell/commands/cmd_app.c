#include "shell_internal.h"

#include <stdint.h>
#include <string.h>

#include "cmd_hw_common.h"
#include "minios_app.h"

static const char *process_state_name(os_process_state_t state)
{
    switch (state) {
    case OS_PROCESS_STARTING:
        return "starting";
    case OS_PROCESS_RUNNING:
        return "running";
    case OS_PROCESS_STOPPING:
        return "stopping";
    case OS_PROCESS_EXITED:
        return "exited";
    default:
        return "unknown";
    }
}

static int app_list(int argc)
{
    size_t index;

    if (argc != 2) {
        minios_shell_write("Usage: app list\r\n");
        return -1;
    }
    minios_shell_write("NAME             DESCRIPTION\r\n");
    for (index = 0U; index < os_app_count(); ++index) {
        const os_app_descriptor_t *application = os_app_at(index);

        minios_shell_printf("%-16s %s\r\n", application->name,
                            application->description);
    }
    return 0;
}

static int app_info(int argc, char **argv)
{
    const os_app_descriptor_t *application;

    if (argc != 3) {
        minios_shell_write("Usage: app info <name>\r\n");
        return -1;
    }
    application = os_app_find(argv[2]);
    if (application == NULL) {
        minios_shell_printf("app: %s: not found\r\n", argv[2]);
        return -1;
    }
    minios_shell_printf("Name:         %s\r\n", application->name);
    minios_shell_printf("Description:  %s\r\n", application->description);
    minios_shell_printf("API:          %u\r\n", MINIOS_API_VERSION);
    return 0;
}

static int cmd_app(int argc, char **argv)
{
    if (argc < 2) {
        minios_shell_write("Usage: app <list|info> ...\r\n");
        return -1;
    }
    if (strcmp(argv[1], "list") == 0) {
        return app_list(argc);
    }
    if (strcmp(argv[1], "info") == 0) {
        return app_info(argc, argv);
    }
    minios_shell_write("Unknown app operation. Use list or info.\r\n");
    return -1;
}

static int cmd_ps(int argc, char **argv)
{
    size_t count;
    size_t index;

    (void)argv;
    if (argc != 1) {
        minios_shell_write("Usage: ps\r\n");
        return -1;
    }
    count = os_process_count();
    minios_shell_write("PID    STATE      EXIT  TIME(ms)  APP\r\n");
    for (index = 0U; index < count; ++index) {
        os_process_info_t process;

        if (os_process_at(index, &process) != OS_APP_OK) {
            continue;
        }
        if (process.state == OS_PROCESS_EXITED) {
            minios_shell_printf("%-6u %-10s %-5d %-9lu %s\r\n",
                                (unsigned int)process.pid,
                                process_state_name(process.state),
                                process.exit_code,
                                (unsigned long)process.elapsed_ms,
                                process.name);
        } else {
            minios_shell_printf("%-6u %-10s %-5s %-9lu %s\r\n",
                                (unsigned int)process.pid,
                                process_state_name(process.state), "-",
                                (unsigned long)process.elapsed_ms,
                                process.name);
        }
    }
    if (count == 0U) {
        minios_shell_write("(none)\r\n");
    }
    return 0;
}

static int cmd_kill(int argc, char **argv)
{
    uint32_t parsed_pid;
    int result;

    if ((argc != 2) ||
        (minios_cmd_parse_u32(argv[1], 1U, UINT16_MAX, &parsed_pid) != 0)) {
        minios_shell_write("Usage: kill <pid>\r\n");
        return -1;
    }
    result = os_app_kill((uint16_t)parsed_pid);
    if (result == OS_APP_NOT_FOUND) {
        minios_shell_printf("kill: %lu: process not found\r\n",
                            (unsigned long)parsed_pid);
        return -1;
    }
    if (result == OS_APP_NOT_RUNNING) {
        minios_shell_printf("kill: %lu: process already exited\r\n",
                            (unsigned long)parsed_pid);
        return -1;
    }
    if (result != OS_APP_OK) {
        minios_shell_printf("kill: %lu: process error\r\n",
                            (unsigned long)parsed_pid);
        return -1;
    }
    minios_shell_printf("Stop requested for PID %lu\r\n",
                        (unsigned long)parsed_pid);
    return 0;
}

static const minios_command_t app_command = {
    .name = "app",
    .description = "List compiled applications",
    .usage = "app <list|info> ...",
    .handler = cmd_app,
};

static const minios_command_t ps_command = {
    .name = "ps",
    .description = "List application processes",
    .usage = "ps",
    .handler = cmd_ps,
};

static const minios_command_t kill_command = {
    .name = "kill",
    .description = "Request an application to stop",
    .usage = "kill <pid>",
    .handler = cmd_kill,
};

int minios_cmd_app_register(void)
{
    return minios_shell_register(&app_command);
}

int minios_cmd_ps_register(void)
{
    return minios_shell_register(&ps_command);
}

int minios_cmd_kill_register(void)
{
    return minios_shell_register(&kill_command);
}
