#include "minios_hal.h"

#include <string.h>

#include "driver/i2c_master.h"

#define I2C_PROBE_TIMEOUT_MS 20

static i2c_master_bus_handle_t i2c_bus;
static minios_hal_i2c_info_t i2c_info;

int minios_hal_i2c_configure(int sda, int scl)
{
    minios_hal_gpio_info_t sda_info;
    minios_hal_gpio_info_t scl_info;
    i2c_master_bus_config_t configuration = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };

    if ((sda == scl) ||
        (minios_hal_gpio_info(sda, &sda_info) != MINIOS_HAL_OK) ||
        (minios_hal_gpio_info(scl, &scl_info) != MINIOS_HAL_OK) ||
        !sda_info.valid || !sda_info.output || !scl_info.valid ||
        !scl_info.output) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    if ((sda_info.reserved &&
         (!i2c_info.initialized || ((sda != i2c_info.sda) &&
                                    (sda != i2c_info.scl)))) ||
        (scl_info.reserved &&
         (!i2c_info.initialized || ((scl != i2c_info.sda) &&
                                    (scl != i2c_info.scl))))) {
        return MINIOS_HAL_BUSY;
    }
    if (i2c_bus != NULL) {
        if (i2c_del_master_bus(i2c_bus) != ESP_OK) {
            return MINIOS_HAL_BUSY;
        }
        i2c_bus = NULL;
        memset(&i2c_info, 0, sizeof(i2c_info));
    }
    if (!minios_hal_gpio_is_usable(sda, 1) ||
        !minios_hal_gpio_is_usable(scl, 1)) {
        return MINIOS_HAL_BUSY;
    }
    if (i2c_new_master_bus(&configuration, &i2c_bus) != ESP_OK) {
        i2c_bus = NULL;
        return MINIOS_HAL_ERROR;
    }
    i2c_info.initialized = 1;
    i2c_info.sda = sda;
    i2c_info.scl = scl;
    return MINIOS_HAL_OK;
}

void minios_hal_i2c_info(minios_hal_i2c_info_t *info)
{
    if (info != NULL) {
        *info = i2c_info;
    }
}

int minios_hal_i2c_scan(minios_hal_i2c_scan_callback_t callback,
                        void *context, size_t *found)
{
    uint16_t address;
    size_t count = 0U;

    if ((callback == NULL) || (found == NULL)) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    if (i2c_bus == NULL) {
        return MINIOS_HAL_NOT_INITIALIZED;
    }
    for (address = 0x03U; address <= 0x77U; ++address) {
        esp_err_t error = i2c_master_probe(i2c_bus, address,
                                           I2C_PROBE_TIMEOUT_MS);
        if (error == ESP_OK) {
            if (callback((uint8_t)address, context) != 0) {
                return MINIOS_HAL_ERROR;
            }
            ++count;
        } else if (error == ESP_ERR_TIMEOUT) {
            return MINIOS_HAL_TIMEOUT;
        } else if (error != ESP_ERR_NOT_FOUND) {
            return MINIOS_HAL_ERROR;
        }
    }
    *found = count;
    return MINIOS_HAL_OK;
}
