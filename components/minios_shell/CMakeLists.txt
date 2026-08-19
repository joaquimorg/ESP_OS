idf_component_register(
    SRCS
        "shell.c"
        "shell_parser.c"
        "commands/cmd_clear.c"
        "commands/cmd_config.c"
        "commands/cmd_help.c"
        "commands/cmd_info.c"
        "commands/cmd_mem.c"
        "commands/cmd_reboot.c"
        "commands/cmd_uptime.c"
        "commands/cmd_version.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES minios_console minios_api minios_config
)
