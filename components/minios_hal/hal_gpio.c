#include "minios_hal.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "soc/soc_caps.h"

static bool gpio_configured[SOC_GPIO_PIN_COUNT];
static minios_hal_gpio_mode_t gpio_modes[SOC_GPIO_PIN_COUNT];

int minios_hal_gpio_is_usable(int pin, int require_output)
{
    /* ESP32-C3 GPIO12..17 are used by flash and GPIO18/19 by the configured
     * USB Serial/JTAG console. Reconfiguring them can stop code execution or
     * make the shell unreachable. */
    if (!GPIO_IS_VALID_GPIO(pin) || ((pin >= 12) && (pin <= 19))) {
        return 0;
    }
    return !require_output || GPIO_IS_VALID_OUTPUT_GPIO(pin);
}

int minios_hal_gpio_mode(int pin, minios_hal_gpio_mode_t mode)
{
    gpio_config_t configuration = {0};

    if (!minios_hal_gpio_is_usable(pin, mode == MINIOS_HAL_GPIO_OUTPUT) ||
        (mode < MINIOS_HAL_GPIO_INPUT) ||
        (mode > MINIOS_HAL_GPIO_INPUT_PULLDOWN) ||
        ((mode == MINIOS_HAL_GPIO_OUTPUT) &&
         !minios_hal_gpio_is_usable(pin, 1))) {
        return MINIOS_HAL_INVALID_ARGUMENT;
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
        return MINIOS_HAL_ERROR;
    }
    gpio_configured[pin] = true;
    gpio_modes[pin] = mode;
    return MINIOS_HAL_OK;
}

int minios_hal_gpio_write(int pin, int value)
{
    if (!minios_hal_gpio_is_usable(pin, 1) ||
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
    if (!minios_hal_gpio_is_usable(pin, 0) || (value == NULL)) {
        return MINIOS_HAL_INVALID_ARGUMENT;
    }
    if (!gpio_configured[pin]) {
        return MINIOS_HAL_NOT_INITIALIZED;
    }
    *value = gpio_get_level((gpio_num_t)pin);
    return MINIOS_HAL_OK;
}
