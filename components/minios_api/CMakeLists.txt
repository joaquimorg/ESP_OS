idf_component_register(
    SRCS "minios.c"
    INCLUDE_DIRS "include"
    REQUIRES app_update bootloader_support esp_system esp_timer heap freertos spi_flash
)
