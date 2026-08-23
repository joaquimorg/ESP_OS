idf_component_register(
    SRCS "kernel.c"
    INCLUDE_DIRS "include"
    REQUIRES minios_api minios_app minios_config minios_console minios_device minios_fs minios_hal minios_module minios_net minios_remote minios_shell minios_web
)
