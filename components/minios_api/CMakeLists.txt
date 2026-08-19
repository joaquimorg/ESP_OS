idf_component_register(
    SRCS "minios.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_system esp_timer heap freertos
)
