#pragma once

#include <stddef.h>
#include <stdint.h>

#define MINIOS_API_VERSION 1
#define MINIOS_NAME "MiniOS"
#define MINIOS_VERSION "0.01"

typedef struct {
    size_t total;
    size_t free;
    size_t minimum_free;
} os_memory_info_t;

typedef struct {
    const char *target;
    uint32_t cpu_cores;
} os_system_info_t;

void os_print(const char *text);
uint32_t os_uptime_ms(void);
void os_sleep(uint32_t milliseconds);
size_t os_free_memory(void);
void os_get_memory_info(os_memory_info_t *info);
void os_get_system_info(os_system_info_t *info);
void os_reboot(void);
