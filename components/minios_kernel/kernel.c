#include "minios_kernel.h"

#include "minios.h"
#include "minios_config.h"
#include "minios_console.h"
#include "minios_fs.h"
#include "minios_shell.h"
#include "minios_version.h"

void minios_kernel_start(void)
{
    minios_console_t console;

    if (minios_console_uart_init(&console) != 0) {
        return;
    }

    minios_console_write_text(&console,
                              "\r\n" MINIOS_NAME " " MINIOS_VERSION "\r\n"
                              MINIOS_COPYRIGHT "\r\n");
    minios_console_write_text(&console, "[ OK ] Kernel\r\n");
    minios_console_write_text(&console, "[ OK ] Console\r\n");

    if (os_config_init() != OS_CONFIG_OK) {
        minios_console_write_text(&console, "[FAIL] Config\r\n");
        return;
    }
    minios_console_write_text(&console, "[ OK ] Config\r\n");

    if (os_fs_init() != OS_FS_OK) {
        minios_console_write_text(&console, "[FAIL] Filesystem\r\n");
        return;
    }
    minios_console_write_text(&console, "[ OK ] Filesystem\r\n");

    if (minios_shell_init(&console) != 0) {
        minios_console_write_text(&console, "[FAIL] Shell\r\n");
        return;
    }

    minios_console_write_text(&console, "[ OK ] Shell\r\n\r\n");
    minios_shell_run();
}

uint32_t minios_uptime_ms(void)
{
    return os_uptime_ms();
}

void minios_reboot(void)
{
    os_reboot();
}
