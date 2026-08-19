idf_component_register(
    SRCS "config_nvs.c"
    INCLUDE_DIRS "include"
    REQUIRES minios_api nvs_flash
)
