set(MINIOS_APP_SOURCES "app_manager.c")

if(CONFIG_MINIOS_ENABLE_APP_HELLO)
    list(APPEND MINIOS_APP_SOURCES "apps/app_hello.c")
endif()

if(CONFIG_MINIOS_ENABLE_APP_COUNTER)
    list(APPEND MINIOS_APP_SOURCES "apps/app_counter.c")
endif()

if(CONFIG_MINIOS_ENABLE_APP_WELCOME)
    list(APPEND MINIOS_APP_SOURCES "apps/app_welcome.c")
endif()

idf_component_register(
    SRCS ${MINIOS_APP_SOURCES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES minios_api freertos
)
