#include "minios_console.h"

#include <stdio.h>

static int uart_read(char *buffer, size_t length)
{
    return (int)fread(buffer, 1U, length, stdin);
}

static int uart_write(const char *buffer, size_t length)
{
    size_t written;

    written = fwrite(buffer, 1U, length, stdout);
    fflush(stdout);
    return (int)written;
}

int minios_console_uart_init(minios_console_t *console)
{
    if (console == NULL) {
        return -1;
    }

    console->read = uart_read;
    console->write = uart_write;
    console->close = NULL;
    console->context = NULL;
    return 0;
}
