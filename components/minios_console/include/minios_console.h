#pragma once

#include <stddef.h>

typedef struct minios_console {
    int (*read)(char *buffer, size_t length);
    int (*write)(const char *buffer, size_t length);
    void (*close)(void);
    void *context;
} minios_console_t;

int minios_console_uart_init(minios_console_t *console);
int minios_console_read(minios_console_t *console, char *buffer, size_t length);
int minios_console_write(minios_console_t *console, const char *buffer, size_t length);
int minios_console_write_text(minios_console_t *console, const char *text);
void minios_console_close(minios_console_t *console);
