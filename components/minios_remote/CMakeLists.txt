if(CONFIG_MINIOS_ENABLE_REMOTE_CONSOLE)
    set(MINIOS_REMOTE_SOURCE "remote_console_tcp.c")
else()
    set(MINIOS_REMOTE_SOURCE "remote_console_stub.c")
endif()

idf_component_register(
    SRCS ${MINIOS_REMOTE_SOURCE}
    INCLUDE_DIRS "include"
    REQUIRES minios_console minios_shell lwip
)
