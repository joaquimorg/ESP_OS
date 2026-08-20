#include "shell_internal.h"

#include <stddef.h>
#include <string.h>

#include "cmd_hw_common.h"
#include "minios_hal.h"

static int print_i2c_address(uint8_t address, void *context)
{
    (void)context;
    minios_shell_printf("0x%02x\r\n", address);
    return 0;
}

static int i2c_init_command(int argc, char **argv)
{
    int sda;
    int scl;
    int result;

    if ((argc != 4) ||
        (minios_cmd_parse_int(argv[2], 0, 255, &sda) != 0) ||
        (minios_cmd_parse_int(argv[3], 0, 255, &scl) != 0)) {
        minios_shell_write("Usage: i2c init <sda> <scl>\r\n");
        return -1;
    }
    result = minios_hal_i2c_configure(sda, scl);
    if (result != MINIOS_HAL_OK) {
        return minios_cmd_hal_report_error("i2c init", result);
    }
    minios_shell_write("OK\r\n");
    return 0;
}

static int i2c_status_command(int argc)
{
    minios_hal_i2c_info_t info;

    if (argc != 2) {
        minios_shell_write("Usage: i2c status\r\n");
        return -1;
    }
    minios_hal_i2c_info(&info);
    if (!info.initialized) {
        minios_shell_write("Not initialized. Use: i2c init <sda> <scl>\r\n");
    } else {
        minios_shell_printf("SDA=%d SCL=%d Scan=%u Hz\r\n", info.sda,
                            info.scl, MINIOS_HAL_I2C_DEFAULT_FREQUENCY);
    }
    return 0;
}

static int i2c_scan_command(int argc)
{
    size_t found;
    int result;

    if (argc != 2) {
        minios_shell_write("Usage: i2c scan\r\n");
        return -1;
    }
    minios_shell_write("Scanning I2C bus...\r\n");
    result = minios_hal_i2c_scan(print_i2c_address, NULL, &found);
    if (result != MINIOS_HAL_OK) {
        return minios_cmd_hal_report_error("i2c scan", result);
    }
    if (found == 0U) {
        minios_shell_write("No devices found\r\n");
    } else {
        minios_shell_printf("%u device(s) found\r\n", (unsigned int)found);
    }
    return 0;
}

static int cmd_i2c(int argc, char **argv)
{
    if (argc < 2) {
        minios_shell_write("Usage: i2c <init|status|scan> ...\r\n");
        return -1;
    }
    if (strcmp(argv[1], "init") == 0) {
        return i2c_init_command(argc, argv);
    }
    if (strcmp(argv[1], "status") == 0) {
        return i2c_status_command(argc);
    }
    if (strcmp(argv[1], "scan") == 0) {
        return i2c_scan_command(argc);
    }
    minios_shell_write("Unknown i2c operation. Use init, status, or scan.\r\n");
    return -1;
}

static const minios_command_t i2c_command = {
    .name = "i2c",
    .description = "Configure and scan I2C",
    .usage = "i2c <init|status|scan> ...",
    .handler = cmd_i2c,
};

int minios_cmd_i2c_register(void)
{
    return minios_shell_register(&i2c_command);
}
