#include "shell_internal.h"

#include "minios_net.h"

static int cmd_ifconfig(int argc, char **argv)
{
    os_net_info_t info;

    (void)argv;
    if (argc != 1) {
        minios_shell_write("Usage: ifconfig\r\n");
        return -1;
    }
    os_net_get_info(&info);
    minios_shell_write("wifi0\r\n");
    if (info.state != OS_NET_STATE_CONNECTED) {
        minios_shell_write("  state disconnected\r\n");
        return 0;
    }
    minios_shell_printf("  ssid    %s\r\n", info.ssid);
    minios_shell_printf("  inet    %s\r\n", info.ip);
    minios_shell_printf("  netmask %s\r\n", info.netmask);
    minios_shell_printf("  gateway %s\r\n", info.gateway);
    minios_shell_printf("  dns     %s\r\n", info.dns);
    return 0;
}

static const minios_command_t ifconfig_command = {
    .name = "ifconfig",
    .description = "Show network configuration",
    .usage = "ifconfig",
    .handler = cmd_ifconfig,
};

int minios_cmd_ifconfig_register(void)
{
    return minios_shell_register(&ifconfig_command);
}
