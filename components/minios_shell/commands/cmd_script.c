#include "shell_internal.h"

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
    return run_script_command(argc, argv, 0);
}

static int cmd_source(int argc, char **argv)
{
    return run_script_command(argc, argv, 1);
}

static const minios_command_t run_command = {
    .name = "run",
    .description = "Run a shell script",
    .usage = "run <file>",
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
