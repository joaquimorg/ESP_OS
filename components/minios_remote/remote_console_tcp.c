#include "minios_remote.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "minios_console.h"
#include "minios_shell.h"
#include "sdkconfig.h"

#define REMOTE_TASK_PRIORITY 5U
#define REMOTE_LISTEN_BACKLOG 1
#define REMOTE_RETRY_DELAY_MS 1000U

#define TELNET_IAC 255U
#define TELNET_DONT 254U
#define TELNET_DO 253U
#define TELNET_WONT 252U
#define TELNET_WILL 251U
#define TELNET_SB 250U
#define TELNET_SE 240U
#define TELNET_OPTION_ECHO 1U
#define TELNET_OPTION_SUPPRESS_GO_AHEAD 3U

typedef enum {
    TELNET_STATE_DATA = 0,
    TELNET_STATE_COMMAND,
    TELNET_STATE_OPTION,
    TELNET_STATE_SUBNEGOTIATION,
    TELNET_STATE_SUBNEGOTIATION_IAC,
} telnet_state_t;

typedef struct {
    int socket;
    telnet_state_t telnet_state;
    uint8_t telnet_command;
    bool telnet_announced;
} tcp_console_context_t;

static bool remote_started;

static void telnet_reply(tcp_console_context_t *context, uint8_t command,
                         uint8_t option)
{
    uint8_t reply[3] = {TELNET_IAC, command, option};
    size_t sent = 0U;

    while (sent < sizeof(reply)) {
        int written = send(context->socket, reply + sent,
                           sizeof(reply) - sent, 0);

        if ((written < 0) && (errno == EINTR)) {
            continue;
        }
        if (written <= 0) {
            return;
        }
        sent += (size_t)written;
    }
}

static void telnet_negotiate_option(tcp_console_context_t *context,
                                    uint8_t option)
{
    switch (context->telnet_command) {
    case TELNET_DO:
        telnet_reply(context,
                     ((option == TELNET_OPTION_ECHO) ||
                      (option == TELNET_OPTION_SUPPRESS_GO_AHEAD))
                         ? TELNET_WILL
                         : TELNET_WONT,
                     option);
        break;
    case TELNET_WILL:
        telnet_reply(context,
                     (option == TELNET_OPTION_SUPPRESS_GO_AHEAD)
                         ? TELNET_DO
                         : TELNET_DONT,
                     option);
        break;
    default:
        break;
    }
}

static void telnet_announce_options(tcp_console_context_t *context)
{
    telnet_reply(context, TELNET_WILL, TELNET_OPTION_ECHO);
    telnet_reply(context, TELNET_WILL, TELNET_OPTION_SUPPRESS_GO_AHEAD);
    telnet_reply(context, TELNET_DO, TELNET_OPTION_SUPPRESS_GO_AHEAD);
    context->telnet_announced = true;
}

static int tcp_console_read(void *context, char *buffer, size_t length)
{
    tcp_console_context_t *tcp = (tcp_console_context_t *)context;

    if ((buffer == NULL) || (length == 0U) ||
        (length > (size_t)INT_MAX)) {
        return -1;
    }
    for (;;) {
        uint8_t byte;
        int received;

        do {
            received = recv(tcp->socket, &byte, 1U, 0);
        } while ((received < 0) && (errno == EINTR));

        if (received <= 0) {
            /* A zero-length receive means that the peer disconnected. */
            return -1;
        }

        switch (tcp->telnet_state) {
        case TELNET_STATE_DATA:
            if (byte == TELNET_IAC) {
                if (!tcp->telnet_announced) {
                    telnet_announce_options(tcp);
                }
                tcp->telnet_state = TELNET_STATE_COMMAND;
                continue;
            }
            buffer[0] = (char)byte;
            return 1;
        case TELNET_STATE_COMMAND:
            if (byte == TELNET_IAC) {
                tcp->telnet_state = TELNET_STATE_DATA;
                buffer[0] = (char)byte;
                return 1;
            }
            if (byte == TELNET_SB) {
                tcp->telnet_state = TELNET_STATE_SUBNEGOTIATION;
            } else if ((byte == TELNET_DO) || (byte == TELNET_DONT) ||
                       (byte == TELNET_WILL) || (byte == TELNET_WONT)) {
                tcp->telnet_command = byte;
                tcp->telnet_state = TELNET_STATE_OPTION;
            } else {
                tcp->telnet_state = TELNET_STATE_DATA;
            }
            continue;
        case TELNET_STATE_OPTION:
            telnet_negotiate_option(tcp, byte);
            tcp->telnet_state = TELNET_STATE_DATA;
            continue;
        case TELNET_STATE_SUBNEGOTIATION:
            if (byte == TELNET_IAC) {
                tcp->telnet_state = TELNET_STATE_SUBNEGOTIATION_IAC;
            }
            continue;
        case TELNET_STATE_SUBNEGOTIATION_IAC:
            tcp->telnet_state = (byte == TELNET_SE)
                                    ? TELNET_STATE_DATA
                                    : TELNET_STATE_SUBNEGOTIATION;
            continue;
        default:
            tcp->telnet_state = TELNET_STATE_DATA;
            continue;
        }
    }
}

static int tcp_console_write(void *context, const char *buffer, size_t length)
{
    const tcp_console_context_t *tcp =
        (const tcp_console_context_t *)context;
    size_t total = 0U;

    while (total < length) {
        int written = send(tcp->socket, buffer + total, length - total, 0);

        if ((written < 0) && (errno == EINTR)) {
            continue;
        }
        if (written <= 0) {
            return (total == 0U) ? -1 : (int)total;
        }
        total += (size_t)written;
    }
    return (int)total;
}

static void tcp_console_close(void *context)
{
    tcp_console_context_t *tcp = (tcp_console_context_t *)context;

    if (tcp->socket >= 0) {
        (void)shutdown(tcp->socket, SHUT_RDWR);
        (void)close(tcp->socket);
        tcp->socket = -1;
    }
}

static int create_server_socket(void)
{
    struct sockaddr_in address = {0};
    int reuse = 1;
    int server = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    if (server < 0) {
        return -1;
    }
    (void)setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    address.sin_family = AF_INET;
    address.sin_port = htons(CONFIG_MINIOS_REMOTE_CONSOLE_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if ((bind(server, (struct sockaddr *)&address, sizeof(address)) != 0) ||
        (listen(server, REMOTE_LISTEN_BACKLOG) != 0)) {
        (void)close(server);
        return -1;
    }
    return server;
}

static void remote_console_task(void *argument)
{
    (void)argument;

    for (;;) {
        int server = create_server_socket();

        if (server < 0) {
            vTaskDelay(pdMS_TO_TICKS(REMOTE_RETRY_DELAY_MS));
            continue;
        }
        for (;;) {
            int client = accept(server, NULL, NULL);

            if (client < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            {
                tcp_console_context_t tcp = {
                    .socket = client,
                    .telnet_state = TELNET_STATE_DATA,
                };
                minios_console_t console = {
                    .read = tcp_console_read,
                    .write = tcp_console_write,
                    .close = tcp_console_close,
                    .context = &tcp,
                };

                minios_console_write_text(
                    &console, "\r\nMiniOS remote console\r\n");
                minios_shell_run_console(&console);
                minios_console_close(&console);
            }
        }
        (void)close(server);
        vTaskDelay(pdMS_TO_TICKS(REMOTE_RETRY_DELAY_MS));
    }
}

int minios_remote_console_start(void)
{
    BaseType_t created;

    if (remote_started) {
        return MINIOS_REMOTE_OK;
    }
    created = xTaskCreate(remote_console_task, "remote_console",
                          CONFIG_MINIOS_REMOTE_CONSOLE_STACK_SIZE, NULL,
                          REMOTE_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        return MINIOS_REMOTE_ERROR;
    }
    remote_started = true;
    return MINIOS_REMOTE_OK;
}
