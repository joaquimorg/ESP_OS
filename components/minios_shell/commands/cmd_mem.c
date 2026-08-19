#include "shell_internal.h"

#include "minios.h"

static int cmd_mem(int argc, char **argv)
{
    os_memory_info_t memory;

    (void)argv;
    if (argc != 1) {
        minios_shell_write("Usage: mem\r\n");
        return -1;
    }

    os_get_memory_info(&memory);
    minios_shell_printf("Heap total : %lu bytes\r\n", (unsigned long)memory.total);
    minios_shell_printf("Heap free  : %lu bytes\r\n", (unsigned long)memory.free);
    minios_shell_printf("Heap min   : %lu bytes\r\n", (unsigned long)memory.minimum_free);
    return 0;
}

static const minios_command_t mem_command = {
    .name = "mem",
    .description = "Show memory information",
    .usage = "mem",
    .handler = cmd_mem,
};

int minios_cmd_mem_register(void)
{
    return minios_shell_register(&mem_command);
}
