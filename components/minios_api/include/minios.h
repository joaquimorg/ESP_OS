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

#define OS_FS_PATH_MAX 128
#define OS_FS_NAME_MAX 64

#define OS_FS_OK 0
#define OS_FS_ERROR -1
#define OS_FS_INVALID_ARGUMENT -2
#define OS_FS_NOT_FOUND -3
#define OS_FS_NOT_DIRECTORY -4
#define OS_FS_IS_DIRECTORY -5
#define OS_FS_ALREADY_EXISTS -6
#define OS_FS_NOT_EMPTY -7
#define OS_FS_PATH_TOO_LONG -8

typedef int (*os_config_list_callback_t)(const char *key,
                                         const char *value,
                                         void *context);

typedef int (*os_fs_list_callback_t)(const char *name, int is_directory,
                                     size_t size, void *context);
typedef int (*os_fs_read_callback_t)(const char *data, size_t length,
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

int os_fs_init(void);
int os_fs_getcwd(char *path, size_t length);
int os_fs_chdir(const char *path);
int os_fs_list(const char *path, os_fs_list_callback_t callback, void *context);
int os_fs_read(const char *path, os_fs_read_callback_t callback, void *context);
int os_fs_write(const char *path, const char *data, int append);
int os_fs_mkdir(const char *path);
int os_fs_remove(const char *path);
