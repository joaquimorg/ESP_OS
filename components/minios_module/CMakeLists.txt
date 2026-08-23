set(MINIOS_MODULE_SOURCES "module_manager.c")

if(CONFIG_MINIOS_ENABLE_SSD1315_MODULE)
    list(APPEND MINIOS_MODULE_SOURCES "modules/module_ssd1315.c")
endif()

idf_component_register(
    SRCS ${MINIOS_MODULE_SOURCES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES minios_api minios_device minios_hal
)
