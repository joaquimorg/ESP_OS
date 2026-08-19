#include "minios_console.h"

#include <string.h>

int minios_console_read(minios_console_t *console, char *buffer, size_t length)
{
    if ((console == NULL) || (console->read == NULL) || (buffer == NULL) || (length == 0U)) {
        return -1;
    }
    return console->read(buffer, length);
}

int minios_console_write(minios_console_t *console, const char *buffer, size_t length)
{
    if ((console == NULL) || (console->write == NULL) || (buffer == NULL)) {
        return -1;
    }
    return console->write(buffer, length);
}

int minios_console_write_text(minios_console_t *console, const char *text)
{
    if (text == NULL) {
        return -1;
    }
    return minios_console_write(console, text, strlen(text));
}

void minios_console_close(minios_console_t *console)
{
    if ((console != NULL) && (console->close != NULL)) {
        console->close();
    }
}
