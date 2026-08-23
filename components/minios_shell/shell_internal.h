#pragma once

#include "minios_shell.h"

int minios_shell_parse(char *line, char **argv, int max_args);
int minios_shell_execute_line_locked(char *line);
int minios_shell_read_bytes(char *data, size_t length);
int minios_script_execute(const char *path, int source);

int minios_cmd_clear_register(void);
int minios_cmd_app_register(void);
int minios_cmd_cat_register(void);
int minios_cmd_cd_register(void);
int minios_cmd_config_register(void);
int minios_cmd_device_register(void);
int minios_cmd_edit_register(void);
int minios_cmd_elf_register(void);
int minios_cmd_echo_register(void);
int minios_cmd_help_register(void);
int minios_cmd_info_register(void);
int minios_cmd_ifconfig_register(void);
int minios_cmd_gpio_register(void);
int minios_cmd_i2c_register(void);
int minios_cmd_kill_register(void);
int minios_cmd_ls_register(void);
int minios_cmd_mem_register(void);
int minios_cmd_mkdir_register(void);
int minios_cmd_module_register(void);
int minios_cmd_pwd_register(void);
int minios_cmd_ping_register(void);
int minios_cmd_ps_register(void);
int minios_cmd_reboot_register(void);
int minios_cmd_rm_register(void);
int minios_cmd_run_register(void);
int minios_cmd_source_register(void);
int minios_cmd_spi_register(void);
int minios_cmd_uptime_register(void);
int minios_cmd_version_register(void);
int minios_cmd_wifi_register(void);
