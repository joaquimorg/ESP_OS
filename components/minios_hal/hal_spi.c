#include "minios_hal.h"

#include <string.h>

#include "driver/spi_master.h"

static spi_device_handle_t spi_device;
static minios_hal_spi_info_t spi_info;

static int spi_release(void)
{
    if (spi_device != NULL) {
        if (spi_bus_remove_device(spi_device) != ESP_OK) {
            return MINIOS_HAL_BUSY;
        }
        spi_device = NULL;
    }
    if (spi_info.initialized) {
        if (spi_bus_free(SPI2_HOST) != ESP_OK) {
            return MINIOS_HAL_BUSY;
        }
        memset(&spi_info, 0, sizeof(spi_info));
    }
    return MINIOS_HAL_OK;
}

int minios_hal_spi_configure(int mosi, int miso, int sclk, int cs,
                             uint32_t frequency)
{
    spi_bus_config_t bus_configuration = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = MINIOS_HAL_SPI_MAX_TRANSFER,
    };
    spi_device_interface_config_t device_configuration = {
        .clock_speed_hz = (int)frequency,
        .mode = 0,
        .spics_io_num = cs,
        .queue_size = 1,
    };
    int result;

    if (!minios_hal_gpio_is_usable(mosi, 1) ||
        !minios_hal_gpio_is_usable(miso, 0) ||
        !minios_hal_gpio_is_usable(sclk, 1) ||
        !minios_hal_gpio_is_usable(cs, 1) ||
        (mosi == miso) || (mosi == sclk) ||
        (mosi == cs) || (miso == sclk) || (miso == cs) || (sclk == cs) ||
        (frequency < 10000U) || (frequency > 10000000U)) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    result = spi_release();
    if (result != MINIOS_HAL_OK) {
        return result;
    }
    if (spi_bus_initialize(SPI2_HOST, &bus_configuration,
                           SPI_DMA_DISABLED) != ESP_OK) {
        return MINIOS_HAL_ERROR;
    }
    if (spi_bus_add_device(SPI2_HOST, &device_configuration,
                           &spi_device) != ESP_OK) {
        spi_bus_free(SPI2_HOST);
        spi_device = NULL;
        return MINIOS_HAL_ERROR;
    }
    spi_info.initialized = 1;
    spi_info.mosi = mosi;
    spi_info.miso = miso;
    spi_info.sclk = sclk;
    spi_info.cs = cs;
    spi_info.frequency = frequency;
    return MINIOS_HAL_OK;
}

void minios_hal_spi_info(minios_hal_spi_info_t *info)
{
    if (info != NULL) {
        *info = spi_info;
    }
}

int minios_hal_spi_transfer(const uint8_t *transmit, uint8_t *receive,
                            size_t length)
{
    spi_transaction_t transaction = {0};

    if ((transmit == NULL) || (receive == NULL) || (length == 0U) ||
        (length > MINIOS_HAL_SPI_MAX_TRANSFER)) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    if (spi_device == NULL) {
        int result = minios_hal_spi_configure(
            MINIOS_HAL_SPI_DEFAULT_MOSI, MINIOS_HAL_SPI_DEFAULT_MISO,
            MINIOS_HAL_SPI_DEFAULT_SCLK, MINIOS_HAL_SPI_DEFAULT_CS,
            MINIOS_HAL_SPI_DEFAULT_FREQUENCY);
        if (result != MINIOS_HAL_OK) {
            return result;
        }
    }
    transaction.length = length * 8U;
    transaction.tx_buffer = transmit;
    transaction.rx_buffer = receive;
    return (spi_device_transmit(spi_device, &transaction) == ESP_OK)
               ? MINIOS_HAL_OK
               : MINIOS_HAL_ERROR;
}
