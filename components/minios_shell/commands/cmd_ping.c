#include "shell_internal.h"

#include <stdint.h>

#include "cmd_hw_common.h"
#include "minios_net.h"

typedef struct {
    const char *host;
} ping_print_context_t;

static void print_ping_reply(const os_net_ping_reply_t *reply, void *context)
{
    const ping_print_context_t *print_context =
        (const ping_print_context_t *)context;

    if (reply->timeout) {
        minios_shell_printf("Timeout from %s: seq=%u\r\n",
                            print_context->host,
                            (unsigned int)reply->sequence);
    } else {
        minios_shell_printf("Reply from %s: seq=%u ttl=%u time=%u ms\r\n",
                            print_context->host,
                            (unsigned int)reply->sequence,
                            (unsigned int)reply->ttl,
                            (unsigned int)reply->time_ms);
    }
}

static int cmd_ping(int argc, char **argv)
{
    ping_print_context_t context;
    os_net_ping_summary_t summary;
    uint32_t count = 4U;
    int result;

    if (((argc != 2) && (argc != 3)) ||
        ((argc == 3) &&
         (minios_cmd_parse_u32(argv[2], 1U, 10U, &count) != 0))) {
        minios_shell_write("Usage: ping <host> [count:1-10]\r\n");
        return -1;
    }
    context.host = argv[1];
    minios_shell_printf("PING %s: %u request(s)\r\n", argv[1],
                        (unsigned int)count);
    result = os_net_ping(argv[1], count, print_ping_reply, &context, &summary);
    if (result != OS_NET_OK) {
        const char *reason = (result == OS_NET_NOT_CONNECTED)
                                 ? "network not connected"
                             : (result == OS_NET_TIMEOUT)
                                 ? "operation timeout"
                                 : "unknown host or network error";
        minios_shell_printf("ping: %s\r\n", reason);
        return -1;
    }
    minios_shell_printf("Sent=%u Received=%u Lost=%u Time=%u ms\r\n",
                        (unsigned int)summary.transmitted,
                        (unsigned int)summary.received,
                        (unsigned int)(summary.transmitted - summary.received),
                        (unsigned int)summary.duration_ms);
    return (summary.received > 0U) ? 0 : -1;
}

static const minios_command_t ping_command = {
    .name = "ping",
    .description = "Send ICMP echo requests",
    .usage = "ping <host> [count]",
    .handler = cmd_ping,
};

int minios_cmd_ping_register(void)
{
    return minios_shell_register(&ping_command);
}
