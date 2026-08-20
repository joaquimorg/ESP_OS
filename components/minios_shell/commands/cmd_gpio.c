#include "shell_internal.h"

#include <string.h>

#include "sdkconfig.h"
#include "cmd_hw_common.h"
#include "minios_hal.h"

static const char *gpio_yes_no(int value)
{
    return value ? "yes" : "no";
}

static const char *gpio_mode_name(minios_hal_gpio_mode_t mode)
{
    switch (mode) {
    case MINIOS_HAL_GPIO_INPUT:
        return "in";
    case MINIOS_HAL_GPIO_OUTPUT:
        return "out";
    case MINIOS_HAL_GPIO_INPUT_PULLUP:
        return "pullup";
    case MINIOS_HAL_GPIO_INPUT_PULLDOWN:
        return "pulldown";
    default:
        return "unknown";
    }
}

static const char *gpio_status(const minios_hal_gpio_info_t *info)
{
    if (info->configured) {
        return "gpio";
    }
    return info->reserved ? "reserved" : "free";
}

static int gpio_list_command(int argc)
{
    size_t pin;

    if (argc != 2) {
        minios_shell_write("Usage: gpio list\r\n");
        return -1;
    }
    minios_shell_printf("Target: %s\r\n", CONFIG_IDF_TARGET);
    minios_shell_write("PIN IN  OUT PU  PD  STATUS\r\n");
    for (pin = 0U; pin < minios_hal_gpio_count(); ++pin) {
        minios_hal_gpio_info_t info;

        if ((minios_hal_gpio_info((int)pin, &info) == MINIOS_HAL_OK) &&
            info.valid) {
            minios_shell_printf("%3u %-3s %-3s %-3s %-3s %s\r\n",
                                (unsigned int)pin, gpio_yes_no(info.input),
                                gpio_yes_no(info.output),
                                gpio_yes_no(info.pullup),
                                gpio_yes_no(info.pulldown), gpio_status(&info));
        }
    }
    return 0;
}

static int gpio_info_command(int argc, char **argv)
{
    minios_hal_gpio_info_t info;
    int pin;

    if ((argc != 3) ||
        (minios_cmd_parse_int(argv[2], 0, 255, &pin) != 0) ||
        (minios_hal_gpio_info(pin, &info) != MINIOS_HAL_OK) || !info.valid) {
        minios_shell_write("Usage: gpio info <valid-pin>\r\n");
        return -1;
    }
    minios_shell_printf("GPIO %d: input=%s output=%s pullup=%s pulldown=%s "
                        "status=%s",
                        pin, gpio_yes_no(info.input), gpio_yes_no(info.output),
                        gpio_yes_no(info.pullup), gpio_yes_no(info.pulldown),
                        gpio_status(&info));
    if (info.configured) {
        minios_shell_printf(" mode=%s", gpio_mode_name(info.mode));
    }
    minios_shell_write("\r\n");
    return 0;
}

static int gpio_reset_command(int argc, char **argv)
{
    int pin;
    int result;

    if ((argc != 3) ||
        (minios_cmd_parse_int(argv[2], 0, 255, &pin) != 0)) {
        minios_shell_write("Usage: gpio reset <pin>\r\n");
        return -1;
    }
    result = minios_hal_gpio_reset(pin);
    if (result != MINIOS_HAL_OK) {
        return minios_cmd_hal_report_error("gpio reset", result);
    }
    minios_shell_write("OK\r\n");
    return 0;
}

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
        minios_shell_write(
            "Usage: gpio <list|info|mode|read|write|reset> ...\r\n");
        return -1;
    }
    if (strcmp(argv[1], "list") == 0) {
        return gpio_list_command(argc);
    }
    if (strcmp(argv[1], "info") == 0) {
        return gpio_info_command(argc, argv);
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
    if (strcmp(argv[1], "reset") == 0) {
        return gpio_reset_command(argc, argv);
    }
    minios_shell_write(
        "Unknown gpio operation. Use list, info, mode, read, write, or reset.\r\n");
    return -1;
}

static const minios_command_t gpio_command = {
    .name = "gpio",
    .description = "Inspect and control GPIO pins",
    .usage = "gpio <list|info|mode|read|write|reset> ...",
    .handler = cmd_gpio,
};

int minios_cmd_gpio_register(void)
{
    return minios_shell_register(&gpio_command);
}
