#include "app_internal.h"

#include <string.h>

static int display_text(const char *text)
{
    return os_device_write("display0", text, strlen(text));
}

static int welcome_main(int argc, char **argv)
{
    os_net_info_t network;
    char title[96] = "Bem-vindo ao MiniOS";
    size_t used = 0U;
    int index;

    if (argc > 0) {
        title[0] = '\0';
        for (index = 0; index < argc; ++index) {
            size_t length = strlen(argv[index]);

            if ((used + ((used == 0U) ? 0U : 1U) + length) >=
                sizeof(title)) {
                os_print("welcome: message is too long\r\n");
                return 2;
            }
            if (used != 0U) {
                title[used++] = ' ';
            }
            memcpy(title + used, argv[index], length);
            used += length;
            title[used] = '\0';
        }
    }
    if (os_device_find("display0") == NULL) {
        os_print("welcome: /dev/display0 is not available\r\n");
        return 3;
    }
    if ((os_device_control("display0", "clear", NULL) != OS_DEVICE_OK) ||
        (display_text(title) != OS_DEVICE_OK) ||
        (os_device_control("display0", "newline", NULL) != OS_DEVICE_OK)) {
        return 4;
    }

    os_net_get_info(&network);
    if (network.state == OS_NET_STATE_CONNECTED) {
        if ((display_text("IP: ") != OS_DEVICE_OK) ||
            (display_text(network.ip) != OS_DEVICE_OK)) {
            return 4;
        }
    } else if (display_text("Rede desligada") != OS_DEVICE_OK) {
        return 4;
    }
    return 0;
}

const os_app_descriptor_t *minios_app_welcome_descriptor(void)
{
    static const os_app_descriptor_t descriptor = {
        .name = "welcome",
        .description = "Show a greeting and network IP on display0",
        .main = welcome_main,
    };

    return &descriptor;
}
