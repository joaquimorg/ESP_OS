#include "minios_hal.h"

#include <stdbool.h>
#include <string.h>

#include "driver/i2c_master.h"

#define I2C_PROBE_TIMEOUT_MS 20
#define I2C_DEVICE_MAX 4U

struct minios_hal_i2c_device {
    i2c_master_dev_handle_t handle;
    uint8_t address;
    bool used;
};

static i2c_master_bus_handle_t i2c_bus;
static minios_hal_i2c_info_t i2c_info;
static minios_hal_i2c_device_t i2c_devices[I2C_DEVICE_MAX];

static bool i2c_has_open_devices(void)
{
    size_t index;

    for (index = 0U; index < I2C_DEVICE_MAX; ++index) {
        if (i2c_devices[index].used) {
            return true;
        }
    }
    return false;
}

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
    if ((i2c_bus != NULL) && i2c_has_open_devices()) {
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

int minios_hal_i2c_device_open(uint8_t address, uint32_t frequency,
                               minios_hal_i2c_device_t **device)
{
    i2c_device_config_t configuration = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = frequency,
    };
    size_t index;

    if ((device == NULL) || (address < 0x03U) || (address > 0x77U) ||
        (frequency == 0U)) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    if (i2c_bus == NULL) {
        return MINIOS_HAL_NOT_INITIALIZED;
    }
    for (index = 0U; index < I2C_DEVICE_MAX; ++index) {
        if (i2c_devices[index].used &&
            (i2c_devices[index].address == address)) {
            return MINIOS_HAL_BUSY;
        }
    }
    for (index = 0U; index < I2C_DEVICE_MAX; ++index) {
        if (!i2c_devices[index].used) {
            if (i2c_master_bus_add_device(i2c_bus, &configuration,
                                          &i2c_devices[index].handle) != ESP_OK) {
                return MINIOS_HAL_ERROR;
            }
            i2c_devices[index].address = address;
            i2c_devices[index].used = true;
            *device = &i2c_devices[index];
            return MINIOS_HAL_OK;
        }
    }
    return MINIOS_HAL_BUSY;
}

int minios_hal_i2c_device_write(minios_hal_i2c_device_t *device,
                                const uint8_t *data, size_t length,
                                uint32_t timeout_ms)
{
    esp_err_t error;

    if ((device == NULL) || !device->used || (data == NULL) ||
        (length == 0U) || (length > MINIOS_HAL_I2C_MAX_TRANSFER) ||
        (timeout_ms == 0U)) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    error = i2c_master_transmit(device->handle, data, length,
                                (int)timeout_ms);
    if (error == ESP_ERR_TIMEOUT) {
        return MINIOS_HAL_TIMEOUT;
    }
    return (error == ESP_OK) ? MINIOS_HAL_OK : MINIOS_HAL_ERROR;
}

int minios_hal_i2c_device_close(minios_hal_i2c_device_t *device)
{
    if ((device == NULL) || !device->used) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    if (i2c_master_bus_rm_device(device->handle) != ESP_OK) {
        return MINIOS_HAL_BUSY;
    }
    memset(device, 0, sizeof(*device));
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
