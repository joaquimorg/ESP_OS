set(MINIOS_SHELL_SOURCES
    "shell.c"
    "shell_parser.c"
    "commands/cmd_fs_common.c"
    "commands/cmd_hw_common.c"
    "commands/cmd_cat.c"
    "commands/cmd_cd.c"
    "commands/cmd_clear.c"
    "commands/cmd_config.c"
    "commands/cmd_device.c"
    "commands/cmd_echo.c"
    "commands/cmd_help.c"
    "commands/cmd_gpio.c"
    "commands/cmd_i2c.c"
    "commands/cmd_info.c"
    "commands/cmd_ls.c"
    "commands/cmd_mem.c"
    "commands/cmd_mkdir.c"
    "commands/cmd_pwd.c"
    "commands/cmd_reboot.c"
    "commands/cmd_rm.c"
    "commands/cmd_spi.c"
    "commands/cmd_uptime.c"
    "commands/cmd_version.c"
)

if(CONFIG_MINIOS_ENABLE_NETWORK)
    list(APPEND MINIOS_SHELL_SOURCES
        "commands/cmd_ifconfig.c"
        "commands/cmd_ping.c"
        "commands/cmd_wifi.c"
    )
endif()

idf_component_register(
    SRCS ${MINIOS_SHELL_SOURCES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES minios_console minios_api minios_config minios_device minios_fs minios_hal minios_net
)
