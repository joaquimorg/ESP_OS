#pragma once

#include <stddef.h>
#include <stdint.h>

#define MINIOS_HAL_OK 0
#define MINIOS_HAL_ERROR -1
#define MINIOS_HAL_INVALID_ARGUMENT -2
#define MINIOS_HAL_NOT_INITIALIZED -3
#define MINIOS_HAL_BUSY -4
#define MINIOS_HAL_TIMEOUT -5

#define MINIOS_HAL_I2C_DEFAULT_FREQUENCY 100000U

#define MINIOS_HAL_SPI_DEFAULT_FREQUENCY 1000000U
#define MINIOS_HAL_SPI_MAX_TRANSFER 32U

typedef enum {
    MINIOS_HAL_GPIO_INPUT = 0,
    MINIOS_HAL_GPIO_OUTPUT,
    MINIOS_HAL_GPIO_INPUT_PULLUP,
    MINIOS_HAL_GPIO_INPUT_PULLDOWN,
} minios_hal_gpio_mode_t;

typedef struct {
    int valid;
    int input;
    int output;
    int pullup;
    int pulldown;
    int reserved;
    int configured;
    minios_hal_gpio_mode_t mode;
} minios_hal_gpio_info_t;

typedef int (*minios_hal_i2c_scan_callback_t)(uint8_t address, void *context);

typedef struct {
    int initialized;
    int sda;
    int scl;
} minios_hal_i2c_info_t;

typedef struct {
    int initialized;
    int mosi;
    int miso;
    int sclk;
    int cs;
    uint32_t frequency;
} minios_hal_spi_info_t;

int minios_hal_init(void);

size_t minios_hal_gpio_count(void);
int minios_hal_gpio_info(int pin, minios_hal_gpio_info_t *info);
int minios_hal_gpio_is_usable(int pin, int require_output);
int minios_hal_gpio_mode(int pin, minios_hal_gpio_mode_t mode);
int minios_hal_gpio_write(int pin, int value);
int minios_hal_gpio_read(int pin, int *value);
int minios_hal_gpio_reset(int pin);

int minios_hal_i2c_configure(int sda, int scl);
void minios_hal_i2c_info(minios_hal_i2c_info_t *info);
int minios_hal_i2c_scan(minios_hal_i2c_scan_callback_t callback,
                        void *context, size_t *found);

int minios_hal_spi_configure(int mosi, int miso, int sclk, int cs,
                             uint32_t frequency);
void minios_hal_spi_info(minios_hal_spi_info_t *info);
int minios_hal_spi_transfer(const uint8_t *transmit, uint8_t *receive,
                            size_t length);
