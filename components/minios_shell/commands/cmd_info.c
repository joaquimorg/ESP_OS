#include "shell_internal.h"

#include "minios.h"

static int cmd_info(int argc, char **argv)
{
    os_system_info_t system;
    os_fs_space_info_t storage;

    (void)argv;
    if (argc != 1) {
        minios_shell_write("Usage: info\r\n");
        return -1;
    }

    os_get_system_info(&system);
    minios_shell_printf("MiniOS       : %s\r\n", MINIOS_VERSION);
    minios_shell_printf("API          : %d\r\n", MINIOS_API_VERSION);
    minios_shell_printf("Target       : %s\r\n", system.target);
    minios_shell_printf("CPU cores    : %lu\r\n", (unsigned long)system.cpu_cores);
    minios_shell_printf("Free memory  : %lu KB\r\n",
                        (unsigned long)(os_free_memory() / 1024U));
    if (system.flash_total > 0U) {
        minios_shell_printf("Flash total  : %lu KB\r\n",
                            (unsigned long)(system.flash_total / 1024U));
    } else {
        minios_shell_printf("Flash total  : unavailable\r\n");
    }
    if ((system.app_partition_total > 0U) &&
        (system.app_partition_used <= system.app_partition_total)) {
        minios_shell_printf(
            "Firmware     : %lu KB used, %lu KB free (%lu KB partition)\r\n",
            (unsigned long)(system.app_partition_used / 1024U),
            (unsigned long)((system.app_partition_total -
                             system.app_partition_used) / 1024U),
            (unsigned long)(system.app_partition_total / 1024U));
    } else {
        minios_shell_printf("Firmware     : unavailable\r\n");
    }
    if (os_fs_get_space_info(&storage) == OS_FS_OK) {
        minios_shell_printf(
            "Storage      : %lu KB used, %lu KB free (%lu KB partition)\r\n",
            (unsigned long)(storage.used / 1024U),
            (unsigned long)(storage.free / 1024U),
            (unsigned long)(storage.total / 1024U));
    } else {
        minios_shell_printf("Storage      : unavailable\r\n");
    }
    return 0;
}

static const minios_command_t info_command = {
    .name = "info",
    .description = "Show system information",
    .usage = "info",
    .handler = cmd_info,
};

int minios_cmd_info_register(void)
{
    return minios_shell_register(&info_command);
}
