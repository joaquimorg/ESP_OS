#include "shell_internal.h"

#include <stddef.h>
#include <string.h>

#include "cmd_hw_common.h"
#include "minios_net.h"

#define WIFI_CONNECT_TIMEOUT_MS 15000U

static const char *wifi_state_name(os_net_state_t state)
{
    switch (state) {
    case OS_NET_STATE_DISCONNECTED:
        return "disconnected";
    case OS_NET_STATE_CONNECTING:
        return "connecting";
    case OS_NET_STATE_CONNECTED:
        return "connected";
    default:
        return "disabled";
    }
}

static int report_wifi_error(const char *operation, int result)
{
    const char *reason;

    switch (result) {
    case OS_NET_INVALID_ARGUMENT:
        reason = "invalid SSID, password, or argument";
        break;
    case OS_NET_NOT_INITIALIZED:
        reason = "network unavailable";
        break;
    case OS_NET_NOT_CONFIGURED:
        reason = "wifi.ssid is not configured";
        break;
    case OS_NET_TIMEOUT:
        reason = "connection timeout";
        break;
    case OS_NET_BUSY:
        reason = "network busy";
        break;
    case OS_NET_AUTH_FAILED:
        reason = "authentication failed; check password and AP security";
        break;
    case OS_NET_AP_NOT_FOUND:
        reason = "access point not found or incompatible security";
        break;
    default:
        reason = "network error or connection rejected";
        break;
    }
    minios_shell_printf("%s: %s\r\n", operation, reason);
    return -1;
}

static int print_access_point(const os_net_access_point_t *access_point,
                              void *context)
{
    (void)context;
    minios_shell_printf("%-32s %4d dBm  ch=%2u  %s\r\n",
                        access_point->ssid[0] != '\0' ? access_point->ssid
                                                      : "<hidden>",
                        access_point->rssi, access_point->channel,
                        access_point->secure ? "secured" : "open");
    return 0;
}

static int wifi_scan_command(int argc)
{
    size_t found;
    int result;

    if (argc != 2) {
        minios_shell_write("Usage: wifi scan\r\n");
        return -1;
    }
    minios_shell_write("Scanning Wi-Fi...\r\n");
    result = os_net_scan(print_access_point, NULL, &found);
    if (result != OS_NET_OK) {
        return report_wifi_error("wifi scan", result);
    }
    minios_shell_printf("%u network(s) found (maximum 20 shown)\r\n",
                        (unsigned int)found);
    return 0;
}

static int wifi_connect_command(int argc, char **argv)
{
    int result;

    if (argc == 2) {
        result = os_net_connect_saved(WIFI_CONNECT_TIMEOUT_MS);
    } else if ((argc == 3) || (argc == 4)) {
        result = os_net_connect(argv[2], argc == 4 ? argv[3] : "",
                                WIFI_CONNECT_TIMEOUT_MS);
    } else {
        minios_shell_write("Usage: wifi connect [ssid] [password]\r\n");
        return -1;
    }
    if (result != OS_NET_OK) {
        return report_wifi_error("wifi connect", result);
    }
    minios_shell_write("Connected\r\n");
    return 0;
}

static int wifi_disconnect_command(int argc)
{
    int result;

    if (argc != 2) {
        minios_shell_write("Usage: wifi disconnect\r\n");
        return -1;
    }
    result = os_net_disconnect();
    if (result != OS_NET_OK) {
        return report_wifi_error("wifi disconnect", result);
    }
    minios_shell_write("Disconnected\r\n");
    return 0;
}

static int wifi_status_command(int argc)
{
    os_net_info_t info;

    if (argc != 2) {
        minios_shell_write("Usage: wifi status\r\n");
        return -1;
    }
    os_net_get_info(&info);
    minios_shell_printf("State: %s\r\n", wifi_state_name(info.state));
    if (info.ssid[0] != '\0') {
        minios_shell_printf("SSID:  %s\r\n", info.ssid);
    }
    if (info.state == OS_NET_STATE_CONNECTED) {
        minios_shell_printf("RSSI:  %d dBm\r\nIP:    %s\r\n",
                            info.rssi, info.ip);
    }
    return 0;
}

static int cmd_wifi(int argc, char **argv)
{
    if (argc < 2) {
        minios_shell_write(
            "Usage: wifi <scan|connect|disconnect|status> ...\r\n");
        return -1;
    }
    if (strcmp(argv[1], "scan") == 0) {
        return wifi_scan_command(argc);
    }
    if (strcmp(argv[1], "connect") == 0) {
        return wifi_connect_command(argc, argv);
    }
    if (strcmp(argv[1], "disconnect") == 0) {
        return wifi_disconnect_command(argc);
    }
    if (strcmp(argv[1], "status") == 0) {
        return wifi_status_command(argc);
    }
    minios_shell_write(
        "Unknown wifi operation. Use scan, connect, disconnect, or status.\r\n");
    return -1;
}

static const minios_command_t wifi_command = {
    .name = "wifi",
    .description = "Scan and manage Wi-Fi",
    .usage = "wifi <scan|connect|disconnect|status> ...",
    .handler = cmd_wifi,
};

int minios_cmd_wifi_register(void)
{
    return minios_shell_register(&wifi_command);
}
