#pragma once

#include "minios_shell.h"

int minios_shell_parse(char *line, char **argv, int max_args);

int minios_cmd_clear_register(void);
int minios_cmd_cat_register(void);
int minios_cmd_cd_register(void);
int minios_cmd_config_register(void);
int minios_cmd_echo_register(void);
int minios_cmd_help_register(void);
int minios_cmd_info_register(void);
int minios_cmd_ls_register(void);
int minios_cmd_mem_register(void);
int minios_cmd_mkdir_register(void);
int minios_cmd_pwd_register(void);
int minios_cmd_reboot_register(void);
int minios_cmd_rm_register(void);
int minios_cmd_uptime_register(void);
int minios_cmd_version_register(void);
