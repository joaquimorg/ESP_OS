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

static bool remote_started;

static int tcp_console_read(void *context, char *buffer, size_t length)
{
    int socket = *(const int *)context;
    int received;

    if (length > (size_t)INT_MAX) {
        return -1;
    }
    do {
        received = recv(socket, buffer, length, 0);
    } while ((received < 0) && (errno == EINTR));

    /* A zero-length receive means that the peer closed the connection. */
    return (received == 0) ? -1 : received;
}

static int tcp_console_write(void *context, const char *buffer, size_t length)
{
    int socket = *(const int *)context;
    size_t total = 0U;

    while (total < length) {
        int written = send(socket, buffer + total, length - total, 0);

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
    int *socket = (int *)context;

    if (*socket >= 0) {
        (void)shutdown(*socket, SHUT_RDWR);
        (void)close(*socket);
        *socket = -1;
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
                minios_console_t console = {
                    .read = tcp_console_read,
                    .write = tcp_console_write,
                    .close = tcp_console_close,
                    .context = &client,
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
