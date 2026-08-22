#include "shell_internal.h"

#include "minios_app.h"

static int run_script_command(int argc, char **argv, int source)
{
    int result;

    if (argc != 2) {
        minios_shell_printf("Usage: %s <file>\r\n", source ? "source" : "run");
        return -1;
    }
    result = minios_script_execute(argv[1], source);
    return (result == MINIOS_SCRIPT_OK) ? 0 : -1;
}

static int cmd_run(int argc, char **argv)
{
    const os_app_descriptor_t *application;
    uint16_t pid;
    int result;

    if (argc < 2) {
        minios_shell_write("Usage: run <app> [arguments ...] | run <file>\r\n");
        return -1;
    }
    application = os_app_find(argv[1]);
    if (application != NULL) {
        result = os_app_run(application->name, argc - 2,
                            (argc > 2) ? &argv[2] : NULL, &pid);
        if (result == OS_APP_PROCESS_LIMIT) {
            minios_shell_write("run: process limit reached\r\n");
            return -1;
        }
        if (result == OS_APP_INVALID_ARGUMENT) {
            minios_shell_write("run: invalid or oversized arguments\r\n");
            return -1;
        }
        if (result != OS_APP_OK) {
            minios_shell_write("run: unable to start application\r\n");
            return -1;
        }
        minios_shell_printf("Started %s as PID %u\r\n", application->name,
                            (unsigned int)pid);
        return 0;
    }
    return run_script_command(argc, argv, 0);
}

static int cmd_source(int argc, char **argv)
{
    return run_script_command(argc, argv, 1);
}

static const minios_command_t run_command = {
    .name = "run",
    .description = "Run an application or shell script",
    .usage = "run <app> [arguments ...] | run <file>",
    .handler = cmd_run,
};

static const minios_command_t source_command = {
    .name = "source",
    .description = "Run a script in the current context",
    .usage = "source <file>",
    .handler = cmd_source,
};

int minios_cmd_run_register(void)
{
    return minios_shell_register(&run_command);
}

int minios_cmd_source_register(void)
{
    return minios_shell_register(&source_command);
}
