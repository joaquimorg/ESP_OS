#include "minios_kernel.h"

#include <string.h>

#include "minios.h"
#include "minios_config.h"
#include "minios_console.h"
#include "minios_device.h"
#include "minios_fs.h"
#include "minios_hal.h"
#include "minios_module.h"
#include "minios_net.h"
#include "minios_remote.h"
#include "minios_shell.h"
#include "minios_version.h"
#include "sdkconfig.h"

#define MINIOS_STRINGIFY_VALUE(value) #value
#define MINIOS_STRINGIFY(value) MINIOS_STRINGIFY_VALUE(value)

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

    if (minios_hal_init() != MINIOS_HAL_OK) {
        minios_console_write_text(&console, "[FAIL] HAL\r\n");
        return;
    }
    minios_console_write_text(&console, "[ OK ] HAL\r\n");

    if (os_config_init() != OS_CONFIG_OK) {
        minios_console_write_text(&console, "[FAIL] Config\r\n");
        return;
    }
    minios_console_write_text(&console, "[ OK ] Config\r\n");

#if CONFIG_MINIOS_ENABLE_NETWORK
    if (os_net_init() != OS_NET_OK) {
        minios_console_write_text(&console, "[WARN] Network unavailable\r\n");
    } else {
        char autoconnect[OS_CONFIG_VALUE_MAX_LENGTH + 1U];

        minios_console_write_text(&console, "[ OK ] Network\r\n");
        if ((os_config_get("wifi.autoconnect", autoconnect,
                           sizeof(autoconnect)) == OS_CONFIG_OK) &&
            ((strcmp(autoconnect, "1") == 0) ||
             (strcmp(autoconnect, "true") == 0) ||
             (strcmp(autoconnect, "yes") == 0))) {
            minios_console_write_text(&console, "[....] Wi-Fi autoconnect\r\n");
            if (os_net_connect_saved(15000U) == OS_NET_OK) {
                minios_console_write_text(&console, "[ OK ] Wi-Fi connected\r\n");
            } else {
                minios_console_write_text(&console,
                                          "[WARN] Wi-Fi autoconnect failed\r\n");
            }
        }
    }
#else
    minios_console_write_text(&console, "[----] Network disabled\r\n");
#endif

    if (os_fs_init() != OS_FS_OK) {
        minios_console_write_text(&console, "[FAIL] Filesystem\r\n");
        return;
    }
    minios_console_write_text(&console, "[ OK ] Filesystem\r\n");

    if (os_device_init() != OS_DEVICE_OK) {
        minios_console_write_text(&console, "[FAIL] Device Manager\r\n");
        return;
    }
    minios_console_write_text(&console, "[ OK ] Device Manager\r\n");

    if (minios_module_init() != MINIOS_MODULE_OK) {
        minios_console_write_text(&console, "[FAIL] Modules\r\n");
        return;
    }
    minios_console_write_text(&console, "[ OK ] Modules\r\n");

    if (minios_shell_init(&console) != 0) {
        minios_console_write_text(&console, "[FAIL] Shell\r\n");
        return;
    }

    minios_console_write_text(&console, "[ OK ] Shell\r\n\r\n");
    {
        int startup_result = minios_shell_run_startup();

        if (startup_result == MINIOS_SCRIPT_OK) {
            minios_console_write_text(&console, "[ OK ] /boot/startup.rc\r\n\r\n");
        } else if (startup_result == MINIOS_SCRIPT_NOT_FOUND) {
            minios_console_write_text(&console, "[----] /boot/startup.rc not found\r\n\r\n");
        } else {
            minios_console_write_text(&console, "[WARN] /boot/startup.rc failed\r\n\r\n");
        }
    }
#if CONFIG_MINIOS_ENABLE_REMOTE_CONSOLE
    if (minios_remote_console_start() == MINIOS_REMOTE_OK) {
        minios_console_write_text(&console, "[ OK ] Remote console TCP port "
                                  MINIOS_STRINGIFY(
                                      CONFIG_MINIOS_REMOTE_CONSOLE_PORT)
                                  "\r\n\r\n");
    } else {
        minios_console_write_text(&console,
                                  "[WARN] Remote console unavailable\r\n\r\n");
    }
#endif
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
