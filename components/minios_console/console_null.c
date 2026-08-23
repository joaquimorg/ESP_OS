#include "minios_console.h"

static int null_read(void *context, char *buffer, size_t length)
{
    (void)context;
    (void)buffer;
    (void)length;
    return 0;
}

static int null_write(void *context, const char *buffer, size_t length)
{
    (void)context;
    (void)buffer;
    return (int)length;
}

int minios_console_uart_init(minios_console_t *console)
{
    if (console == NULL) {
        return -1;
    }
    console->read = null_read;
    console->write = null_write;
    console->close = NULL;
    console->context = NULL;
    return 0;
}
