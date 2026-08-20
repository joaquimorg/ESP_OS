#include "minios_hal.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_private/esp_gpio_reserve.h"
#include "soc/soc_caps.h"

static bool gpio_configured[SOC_GPIO_PIN_COUNT];
static minios_hal_gpio_mode_t gpio_modes[SOC_GPIO_PIN_COUNT];

size_t minios_hal_gpio_count(void)
{
    return SOC_GPIO_PIN_COUNT;
}

int minios_hal_gpio_info(int pin, minios_hal_gpio_info_t *info)
{
    if ((info == NULL) || (pin < 0) || (pin >= SOC_GPIO_PIN_COUNT)) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    info->valid = GPIO_IS_VALID_GPIO(pin);
    info->input = info->valid;
    info->output = GPIO_IS_VALID_OUTPUT_GPIO(pin);
    /* On ESP targets, input-only pads do not have software-controlled
     * internal pull resistors. */
    info->pullup = info->output;
    info->pulldown = info->output;
    info->reserved = info->valid &&
                     esp_gpio_is_reserved(UINT64_C(1) << (unsigned int)pin);
    info->configured = info->valid && gpio_configured[pin];
    info->mode = info->configured ? gpio_modes[pin] : MINIOS_HAL_GPIO_INPUT;
    return MINIOS_HAL_OK;
}

int minios_hal_gpio_is_usable(int pin, int require_output)
{
    minios_hal_gpio_info_t info;

    if ((minios_hal_gpio_info(pin, &info) != MINIOS_HAL_OK) || !info.valid ||
        info.reserved) {
        return 0;
    }
    return !require_output || info.output;
}

int minios_hal_gpio_mode(int pin, minios_hal_gpio_mode_t mode)
{
    gpio_config_t configuration = {0};

    minios_hal_gpio_info_t info;
    bool newly_reserved = false;

    if ((mode < MINIOS_HAL_GPIO_INPUT) ||
        (mode > MINIOS_HAL_GPIO_INPUT_PULLDOWN) ||
        (minios_hal_gpio_info(pin, &info) != MINIOS_HAL_OK) || !info.valid ||
        ((mode == MINIOS_HAL_GPIO_OUTPUT) && !info.output) ||
        ((mode == MINIOS_HAL_GPIO_INPUT_PULLUP) && !info.pullup) ||
        ((mode == MINIOS_HAL_GPIO_INPUT_PULLDOWN) && !info.pulldown)) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    if (info.reserved && !info.configured) {
        return MINIOS_HAL_BUSY;
    }
    if (!info.configured) {
        uint64_t mask = UINT64_C(1) << (unsigned int)pin;
        if ((esp_gpio_reserve(mask) & mask) != 0U) {
            return MINIOS_HAL_BUSY;
        }
        newly_reserved = true;
    }
    configuration.pin_bit_mask = UINT64_C(1) << (unsigned int)pin;
    configuration.mode = (mode == MINIOS_HAL_GPIO_OUTPUT)
                             ? GPIO_MODE_OUTPUT
                             : GPIO_MODE_INPUT;
    configuration.pull_up_en = (mode == MINIOS_HAL_GPIO_INPUT_PULLUP)
                                   ? GPIO_PULLUP_ENABLE
                                   : GPIO_PULLUP_DISABLE;
    configuration.pull_down_en = (mode == MINIOS_HAL_GPIO_INPUT_PULLDOWN)
                                     ? GPIO_PULLDOWN_ENABLE
                                     : GPIO_PULLDOWN_DISABLE;
    configuration.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&configuration) != ESP_OK) {
        if (newly_reserved) {
            esp_gpio_revoke(UINT64_C(1) << (unsigned int)pin);
        }
        return MINIOS_HAL_ERROR;
    }
    gpio_configured[pin] = true;
    gpio_modes[pin] = mode;
    return MINIOS_HAL_OK;
}

int minios_hal_gpio_reset(int pin)
{
    if ((pin < 0) || (pin >= SOC_GPIO_PIN_COUNT) || !gpio_configured[pin]) {
        return MINIOS_HAL_NOT_INITIALIZED;
    }
    if (gpio_reset_pin((gpio_num_t)pin) != ESP_OK) {
        return MINIOS_HAL_ERROR;
    }
    gpio_configured[pin] = false;
    return MINIOS_HAL_OK;
}

int minios_hal_gpio_write(int pin, int value)
{
    if ((pin < 0) || (pin >= SOC_GPIO_PIN_COUNT) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(pin) ||
        ((value != 0) && (value != 1))) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    if (!gpio_configured[pin] || (gpio_modes[pin] != MINIOS_HAL_GPIO_OUTPUT)) {
        return MINIOS_HAL_NOT_INITIALIZED;
    }
    return (gpio_set_level((gpio_num_t)pin, (uint32_t)value) == ESP_OK)
               ? MINIOS_HAL_OK
               : MINIOS_HAL_ERROR;
}

int minios_hal_gpio_read(int pin, int *value)
{
    if ((pin < 0) || (pin >= SOC_GPIO_PIN_COUNT) ||
        !GPIO_IS_VALID_GPIO(pin) || (value == NULL)) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    if (!gpio_configured[pin]) {
        return MINIOS_HAL_NOT_INITIALIZED;
    }
    *value = gpio_get_level((gpio_num_t)pin);
    return MINIOS_HAL_OK;
}
