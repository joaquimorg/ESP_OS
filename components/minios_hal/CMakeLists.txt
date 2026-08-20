idf_component_register(
    SRCS "hal.c" "hal_gpio.c" "hal_i2c.c" "hal_spi.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_spi
)
