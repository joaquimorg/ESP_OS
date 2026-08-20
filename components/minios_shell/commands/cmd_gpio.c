#include "shell_internal.h"

#include <string.h>

#include "cmd_hw_common.h"
#include "minios_hal.h"

static int gpio_mode_command(int argc, char **argv)
{
    minios_hal_gpio_mode_t mode;
    int pin;
    int result;

    if ((argc != 4) ||
        (minios_cmd_parse_int(argv[2], 0, 255, &pin) != 0)) {
        minios_shell_write(
            "Usage: gpio mode <pin> <in|out|pullup|pulldown>\r\n");
        return -1;
    }
    if (strcmp(argv[3], "in") == 0) {
        mode = MINIOS_HAL_GPIO_INPUT;
    } else if (strcmp(argv[3], "out") == 0) {
        mode = MINIOS_HAL_GPIO_OUTPUT;
    } else if (strcmp(argv[3], "pullup") == 0) {
        mode = MINIOS_HAL_GPIO_INPUT_PULLUP;
    } else if (strcmp(argv[3], "pulldown") == 0) {
        mode = MINIOS_HAL_GPIO_INPUT_PULLDOWN;
    } else {
        minios_shell_write(
            "Usage: gpio mode <pin> <in|out|pullup|pulldown>\r\n");
        return -1;
    }
    result = minios_hal_gpio_mode(pin, mode);
    if (result != MINIOS_HAL_OK) {
        return minios_cmd_hal_report_error("gpio mode", result);
    }
    minios_shell_write("OK\r\n");
    return 0;
}

static int gpio_write_command(int argc, char **argv)
{
    int pin;
    int value;
    int result;

    if ((argc != 4) ||
        (minios_cmd_parse_int(argv[2], 0, 255, &pin) != 0) ||
        (minios_cmd_parse_int(argv[3], 0, 1, &value) != 0)) {
        minios_shell_write("Usage: gpio write <pin> <0|1>\r\n");
        return -1;
    }
    result = minios_hal_gpio_write(pin, value);
    if (result != MINIOS_HAL_OK) {
        return minios_cmd_hal_report_error("gpio write", result);
    }
    minios_shell_write("OK\r\n");
    return 0;
}

static int gpio_read_command(int argc, char **argv)
{
    int pin;
    int value;
    int result;

    if ((argc != 3) ||
        (minios_cmd_parse_int(argv[2], 0, 255, &pin) != 0)) {
        minios_shell_write("Usage: gpio read <pin>\r\n");
        return -1;
    }
    result = minios_hal_gpio_read(pin, &value);
    if (result != MINIOS_HAL_OK) {
        return minios_cmd_hal_report_error("gpio read", result);
    }
    minios_shell_printf("%d\r\n", value);
    return 0;
}

static int cmd_gpio(int argc, char **argv)
{
    if (argc < 2) {
        minios_shell_write("Usage: gpio <mode|read|write> ...\r\n");
        return -1;
    }
    if (strcmp(argv[1], "mode") == 0) {
        return gpio_mode_command(argc, argv);
    }
    if (strcmp(argv[1], "read") == 0) {
        return gpio_read_command(argc, argv);
    }
    if (strcmp(argv[1], "write") == 0) {
        return gpio_write_command(argc, argv);
    }
    minios_shell_write("Unknown gpio operation. Use mode, read, or write.\r\n");
    return -1;
}

static const minios_command_t gpio_command = {
    .name = "gpio",
    .description = "Control GPIO pins",
    .usage = "gpio <mode|read|write> ...",
    .handler = cmd_gpio,
};

int minios_cmd_gpio_register(void)
{
    return minios_shell_register(&gpio_command);
}
