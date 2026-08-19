#include "minios_console.h"

#include <limits.h>
#include <unistd.h>

static int uart_read(char *buffer, size_t length)
{
    ssize_t received;

    if (length > (size_t)INT_MAX) {
        return -1;
    }

    /*
     * Use the console VFS directly.  fread() retains EOF after an empty
     * non-blocking USB Serial/JTAG read, preventing later keystrokes from
     * reaching the shell.
     */
    received = read(STDIN_FILENO, buffer, length);
    return (received < 0) ? -1 : (int)received;
}

static int uart_write(const char *buffer, size_t length)
{
    size_t total = 0U;

    if (length > (size_t)INT_MAX) {
        return -1;
    }

    while (total < length) {
        ssize_t written = write(STDOUT_FILENO, buffer + total, length - total);

        if (written <= 0) {
            return (total == 0U) ? -1 : (int)total;
        }
        total += (size_t)written;
    }

    /* Prompts and echoed characters have no trailing newline.  USB
     * Serial/JTAG needs an explicit flush to send these short packets. */
    if (fsync(STDOUT_FILENO) != 0) {
        return -1;
    }
    return (int)total;
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
