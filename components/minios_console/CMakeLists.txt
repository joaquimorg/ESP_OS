if(CONFIG_MINIOS_ENABLE_LOCAL_CONSOLE)
    set(MINIOS_CONSOLE_SOURCE "console_uart.c")
else()
    set(MINIOS_CONSOLE_SOURCE "console_null.c")
endif()

idf_component_register(
    SRCS "console.c" ${MINIOS_CONSOLE_SOURCE}
    INCLUDE_DIRS "include"
)
