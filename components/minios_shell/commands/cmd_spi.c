#include "shell_internal.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_hw_common.h"
#include "minios_hal.h"

static int parse_hex_byte(const char *text, uint8_t *value)
{
    char *end;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(text, &end, 16);
    if ((errno != 0) || (end == text) || (*end != '\0') || (parsed > 0xffUL)) {
        return -1;
    }
    *value = (uint8_t)parsed;
    return 0;
}

static int spi_init_command(int argc, char **argv)
{
    uint32_t frequency = MINIOS_HAL_SPI_DEFAULT_FREQUENCY;
    int mosi;
    int miso;
    int sclk;
    int cs;
    int result;

    if (((argc != 6) && (argc != 7)) ||
        (minios_cmd_parse_int(argv[2], 0, 255, &mosi) != 0) ||
        (minios_cmd_parse_int(argv[3], 0, 255, &miso) != 0) ||
        (minios_cmd_parse_int(argv[4], 0, 255, &sclk) != 0) ||
        (minios_cmd_parse_int(argv[5], 0, 255, &cs) != 0) ||
        ((argc == 7) &&
         (minios_cmd_parse_u32(argv[6], 10000U, 10000000U,
                               &frequency) != 0))) {
        minios_shell_write(
            "Usage: spi init <mosi> <miso> <sclk> <cs> [frequency]\r\n");
        return -1;
    }
    result = minios_hal_spi_configure(mosi, miso, sclk, cs, frequency);
    if (result != MINIOS_HAL_OK) {
        return minios_cmd_hal_report_error("spi init", result);
    }
    minios_shell_write("OK\r\n");
    return 0;
}

static int spi_status_command(int argc)
{
    minios_hal_spi_info_t info;

    if (argc != 2) {
        minios_shell_write("Usage: spi status\r\n");
        return -1;
    }
    minios_hal_spi_info(&info);
    if (!info.initialized) {
        minios_shell_printf(
            "Not initialized (default MOSI=%d MISO=%d SCLK=%d CS=%d %u Hz)\r\n",
            MINIOS_HAL_SPI_DEFAULT_MOSI, MINIOS_HAL_SPI_DEFAULT_MISO,
            MINIOS_HAL_SPI_DEFAULT_SCLK, MINIOS_HAL_SPI_DEFAULT_CS,
            MINIOS_HAL_SPI_DEFAULT_FREQUENCY);
    } else {
        minios_shell_printf(
            "MOSI=%d MISO=%d SCLK=%d CS=%d Frequency=%u Hz\r\n",
            info.mosi, info.miso, info.sclk, info.cs, info.frequency);
    }
    return 0;
}

static int spi_transfer_command(int argc, char **argv)
{
    uint8_t transmit[MINIOS_HAL_SPI_MAX_TRANSFER];
    uint8_t receive[MINIOS_HAL_SPI_MAX_TRANSFER];
    size_t length;
    size_t index;
    int result;

    if ((argc < 3) || ((size_t)(argc - 2) > sizeof(transmit))) {
        minios_shell_write("Usage: spi transfer <hex-byte> [hex-byte ...]\r\n");
        return -1;
    }
    length = (size_t)(argc - 2);
    for (index = 0U; index < length; ++index) {
        if (parse_hex_byte(argv[index + 2U], &transmit[index]) != 0) {
            minios_shell_printf("spi transfer: invalid byte: %s\r\n",
                                argv[index + 2U]);
            return -1;
        }
    }
    result = minios_hal_spi_transfer(transmit, receive, length);
    if (result != MINIOS_HAL_OK) {
        return minios_cmd_hal_report_error("spi transfer", result);
    }
    minios_shell_write("RX:");
    for (index = 0U; index < length; ++index) {
        minios_shell_printf(" %02x", receive[index]);
    }
    minios_shell_write("\r\n");
    return 0;
}

static int cmd_spi(int argc, char **argv)
{
    if (argc < 2) {
        minios_shell_write("Usage: spi <init|status|transfer> ...\r\n");
        return -1;
    }
    if (strcmp(argv[1], "init") == 0) {
        return spi_init_command(argc, argv);
    }
    if (strcmp(argv[1], "status") == 0) {
        return spi_status_command(argc);
    }
    if (strcmp(argv[1], "transfer") == 0) {
        return spi_transfer_command(argc, argv);
    }
    minios_shell_write(
        "Unknown spi operation. Use init, status, or transfer.\r\n");
    return -1;
}

static const minios_command_t spi_command = {
    .name = "spi",
    .description = "Configure and transfer over SPI",
    .usage = "spi <init|status|transfer> ...",
    .handler = cmd_spi,
};

int minios_cmd_spi_register(void)
{
    return minios_shell_register(&spi_command);
}
