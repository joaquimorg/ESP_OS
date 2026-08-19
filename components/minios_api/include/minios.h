#pragma once

#include <stddef.h>
#include <stdint.h>

#define MINIOS_API_VERSION 1
#define MINIOS_NAME "MiniOS"
#define MINIOS_VERSION "0.01"
#define MINIOS_COPYRIGHT "Copyright 2026 joaquim.org"

#define OS_CONFIG_KEY_MAX_LENGTH 63
#define OS_CONFIG_VALUE_MAX_LENGTH 127

#define OS_CONFIG_OK 0
#define OS_CONFIG_ERROR -1
#define OS_CONFIG_NOT_FOUND -2
#define OS_CONFIG_INVALID_ARGUMENT -3
#define OS_CONFIG_BUFFER_TOO_SMALL -4
#define OS_CONFIG_KEY_COLLISION -5

typedef int (*os_config_list_callback_t)(const char *key,
                                         const char *value,
                                         void *context);

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

int os_config_init(void);
int os_config_get(const char *key, char *value, size_t length);
int os_config_set(const char *key, const char *value);
int os_config_delete(const char *key);
int os_config_list(os_config_list_callback_t callback, void *context);
