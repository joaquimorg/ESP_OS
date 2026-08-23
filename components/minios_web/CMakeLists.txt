if(CONFIG_MINIOS_ENABLE_WEB_SERVICE)
    set(MINIOS_WEB_SOURCE "web_service.c")
else()
    set(MINIOS_WEB_SOURCE "web_service_stub.c")
endif()

idf_component_register(
    SRCS ${MINIOS_WEB_SOURCE}
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server minios_api minios_console minios_device minios_fs minios_module minios_shell
)
