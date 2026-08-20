#include "minios_net.h"

#include <string.h>

int os_net_init(void)
{
    return OS_NET_NOT_INITIALIZED;
}

int os_net_scan(os_net_scan_callback_t callback, void *context, size_t *found)
{
    (void)callback;
    (void)context;
    (void)found;
    return OS_NET_NOT_INITIALIZED;
}

int os_net_connect(const char *ssid, const char *password,
                   uint32_t timeout_ms)
{
    (void)ssid;
    (void)password;
    (void)timeout_ms;
    return OS_NET_NOT_INITIALIZED;
}

int os_net_connect_saved(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return OS_NET_NOT_INITIALIZED;
}

int os_net_disconnect(void)
{
    return OS_NET_NOT_INITIALIZED;
}

void os_net_get_info(os_net_info_t *info)
{
    if (info != NULL) {
        memset(info, 0, sizeof(*info));
        info->state = OS_NET_STATE_DISABLED;
    }
}

int os_net_ping(const char *host, uint32_t count,
                os_net_ping_callback_t callback, void *context,
                os_net_ping_summary_t *summary)
{
    (void)host;
    (void)count;
    (void)callback;
    (void)context;
    (void)summary;
    return OS_NET_NOT_INITIALIZED;
}
