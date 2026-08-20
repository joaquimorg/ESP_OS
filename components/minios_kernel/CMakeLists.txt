idf_component_register(
    SRCS "kernel.c"
    INCLUDE_DIRS "include"
    REQUIRES minios_api minios_config minios_console minios_device minios_fs minios_shell
)
