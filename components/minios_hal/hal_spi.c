#include "minios_hal.h"

#include <string.h>

#include "driver/spi_master.h"
#include "esp_private/esp_gpio_reserve.h"

static spi_device_handle_t spi_device;
static minios_hal_spi_info_t spi_info;
static uint64_t spi_reserved_mask;

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
    if (spi_reserved_mask != 0U) {
        esp_gpio_revoke(spi_reserved_mask);
        spi_reserved_mask = 0U;
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
    minios_hal_gpio_info_t pin_info[4];
    const int pins[4] = {mosi, miso, sclk, cs};
    uint64_t requested_mask = 0U;
    uint64_t old_mask;
    size_t index;
    int result;

    if ((mosi == miso) || (mosi == sclk) ||
        (mosi == cs) || (miso == sclk) || (miso == cs) || (sclk == cs) ||
        (frequency < 10000U) || (frequency > 10000000U)) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    for (index = 0U; index < 4U; ++index) {
        int require_output = (index != 1U);
        if ((minios_hal_gpio_info(pins[index], &pin_info[index]) !=
             MINIOS_HAL_OK) || !pin_info[index].valid ||
            (require_output && !pin_info[index].output)) {
            return MINIOS_HAL_INVALID_ARGUMENT;
        }
        if (pin_info[index].reserved &&
            ((spi_reserved_mask & (UINT64_C(1) << pins[index])) == 0U)) {
            return MINIOS_HAL_BUSY;
        }
        requested_mask |= UINT64_C(1) << pins[index];
    }
    result = spi_release();
    if (result != MINIOS_HAL_OK) {
        return result;
    }
    old_mask = esp_gpio_reserve(requested_mask) & requested_mask;
    if (old_mask != 0U) {
        esp_gpio_revoke(requested_mask & ~old_mask);
        return MINIOS_HAL_BUSY;
    }
    spi_reserved_mask = requested_mask;
    if (spi_bus_initialize(SPI2_HOST, &bus_configuration,
                           SPI_DMA_DISABLED) != ESP_OK) {
        esp_gpio_revoke(spi_reserved_mask);
        spi_reserved_mask = 0U;
        return MINIOS_HAL_ERROR;
    }
    if (spi_bus_add_device(SPI2_HOST, &device_configuration,
                           &spi_device) != ESP_OK) {
        spi_bus_free(SPI2_HOST);
        esp_gpio_revoke(spi_reserved_mask);
        spi_reserved_mask = 0U;
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
        return MINIOS_HAL_NOT_INITIALIZED;
    }
    transaction.length = length * 8U;
    transaction.tx_buffer = transmit;
    transaction.rx_buffer = receive;
    return (spi_device_transmit(spi_device, &transaction) == ESP_OK)
               ? MINIOS_HAL_OK
               : MINIOS_HAL_ERROR;
}
