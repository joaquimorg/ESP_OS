idf_component_register(
    SRCS
        "app_manager.c"
        "apps/app_hello.c"
        "apps/app_counter.c"
        "apps/app_welcome.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES minios_api freertos
)
