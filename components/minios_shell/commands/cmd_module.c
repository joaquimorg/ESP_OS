#include "shell_internal.h"

#include <string.h>

#include "minios_module.h"

static int report_module_error(const char *operation, const char *name,
                               int result)
{
    const char *reason;

    switch (result) {
    case MINIOS_MODULE_INVALID_ARGUMENT:
        reason = "invalid argument";
        break;
    case MINIOS_MODULE_NOT_FOUND:
        reason = "not found";
        break;
    case MINIOS_MODULE_ALREADY_LOADED:
        reason = "already loaded";
        break;
    case MINIOS_MODULE_NOT_LOADED:
        reason = "not loaded";
        break;
    case MINIOS_MODULE_DEPENDENCY:
        reason = "dependency unavailable";
        break;
    case MINIOS_MODULE_BUSY:
        reason = "resource busy";
        break;
    default:
        reason = "hardware or module error";
        break;
    }
    minios_shell_printf("module %s: %s: %s\r\n", operation, name, reason);
    return -1;
}

static int module_list(int argc)
{
    size_t index;

    if (argc != 2) {
        minios_shell_write("Usage: module list\r\n");
        return -1;
    }
    minios_shell_write("NAME             STATE      DEVICE\r\n");
    for (index = 0U; index < minios_module_count(); ++index) {
        const minios_module_info_t *module = minios_module_at(index);

        minios_shell_printf("%-16s %-10s %s\r\n", module->name,
                            module->loaded ? "loaded" : "available",
                            (module->device == NULL) ? "-" : module->device);
    }
    return 0;
}

static int module_info(int argc, char **argv)
{
    const minios_module_info_t *module;

    if (argc != 3) {
        minios_shell_write("Usage: module info <name>\r\n");
        return -1;
    }
    module = minios_module_find(argv[2]);
    if (module == NULL) {
        return report_module_error("info", argv[2], MINIOS_MODULE_NOT_FOUND);
    }
    minios_shell_printf("Name:         %s\r\n", module->name);
    minios_shell_printf("State:        %s\r\n",
                        module->loaded ? "loaded" : "available");
    minios_shell_printf("Device:       %s\r\n",
                        (module->device == NULL) ? "-" : module->device);
    minios_shell_printf("Description:  %s\r\n", module->description);
    return 0;
}

static int module_load(int argc, char **argv)
{
    int result;

    if ((argc < 3) || (argc > 4)) {
        minios_shell_write("Usage: module load <name> [argument]\r\n");
        return -1;
    }
    result = minios_module_load(argv[2], argc - 3,
                                (argc > 3) ? &argv[3] : NULL);
    if (result != MINIOS_MODULE_OK) {
        return report_module_error("load", argv[2], result);
    }
    minios_shell_printf("Loaded %s\r\n", argv[2]);
    return 0;
}

static int module_unload(int argc, char **argv)
{
    int result;

    if (argc != 3) {
        minios_shell_write("Usage: module unload <name>\r\n");
        return -1;
    }
    result = minios_module_unload(argv[2]);
    if (result != MINIOS_MODULE_OK) {
        return report_module_error("unload", argv[2], result);
    }
    minios_shell_printf("Unloaded %s\r\n", argv[2]);
    return 0;
}

static int cmd_module(int argc, char **argv)
{
    if (argc < 2) {
        minios_shell_write("Usage: module <list|info|load|unload> ...\r\n");
        return -1;
    }
    if (strcmp(argv[1], "list") == 0) {
        return module_list(argc);
    }
    if (strcmp(argv[1], "info") == 0) {
        return module_info(argc, argv);
    }
    if (strcmp(argv[1], "load") == 0) {
        return module_load(argc, argv);
    }
    if (strcmp(argv[1], "unload") == 0) {
        return module_unload(argc, argv);
    }
    minios_shell_write("Unknown module operation. Use list, info, load, or unload.\r\n");
    return -1;
}

static const minios_command_t module_command = {
    .name = "module",
    .description = "Manage compiled modules",
    .usage = "module <list|info|load|unload> ...",
    .handler = cmd_module,
};

int minios_cmd_module_register(void)
{
    return minios_shell_register(&module_command);
}
