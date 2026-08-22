idf_component_register(
    SRCS "module_manager.c" "modules/module_ssd1315.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES minios_api minios_device minios_hal
)
