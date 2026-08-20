#pragma once

#include <stddef.h>
#include <stdint.h>

#define MINIOS_API_VERSION 2
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
#define OS_FS_IS_DEVICE -9

#define OS_DEVICE_MAX 8
#define OS_DEVICE_NAME_MAX 15

#define OS_DEVICE_OK 0
#define OS_DEVICE_ERROR -1
#define OS_DEVICE_INVALID_ARGUMENT -2
#define OS_DEVICE_NOT_FOUND -3
#define OS_DEVICE_ALREADY_EXISTS -4
#define OS_DEVICE_REGISTRY_FULL -5

#define OS_DEVICE_CAP_READ (1U << 0)
#define OS_DEVICE_CAP_WRITE (1U << 1)
#define OS_DEVICE_CAP_CONTROL (1U << 2)

#define OS_NET_OK 0
#define OS_NET_ERROR -1
#define OS_NET_INVALID_ARGUMENT -2
#define OS_NET_NOT_INITIALIZED -3
#define OS_NET_NOT_CONNECTED -4
#define OS_NET_TIMEOUT -5
#define OS_NET_BUSY -6
#define OS_NET_NOT_CONFIGURED -7

#define OS_NET_SSID_MAX_LENGTH 32
#define OS_NET_PASSWORD_MAX_LENGTH 64
#define OS_NET_IPV4_STRING_LENGTH 16

typedef int (*os_config_list_callback_t)(const char *key,
                                         const char *value,
                                         void *context);

typedef int (*os_fs_list_callback_t)(const char *name, int is_directory,
                                     size_t size, void *context);
typedef int (*os_fs_read_callback_t)(const char *data, size_t length,
                                     void *context);

typedef enum {
    OS_DEVICE_CLASS_CHARACTER = 0,
    OS_DEVICE_CLASS_CONTROLLER,
} os_device_class_t;

typedef struct minios_device {
    const char *name;
    os_device_class_t device_class;
    const char *driver;
    const char *description;
    uint32_t capabilities;
} minios_device_t;

typedef struct {
    size_t total;
    size_t free;
    size_t minimum_free;
} os_memory_info_t;

typedef struct {
    const char *target;
    uint32_t cpu_cores;
} os_system_info_t;

typedef enum {
    OS_NET_STATE_DISABLED = 0,
    OS_NET_STATE_DISCONNECTED,
    OS_NET_STATE_CONNECTING,
    OS_NET_STATE_CONNECTED,
} os_net_state_t;

typedef struct {
    char ssid[OS_NET_SSID_MAX_LENGTH + 1U];
    int rssi;
    uint8_t channel;
    int secure;
} os_net_access_point_t;

typedef struct {
    os_net_state_t state;
    char ssid[OS_NET_SSID_MAX_LENGTH + 1U];
    char ip[OS_NET_IPV4_STRING_LENGTH];
    char netmask[OS_NET_IPV4_STRING_LENGTH];
    char gateway[OS_NET_IPV4_STRING_LENGTH];
    char dns[OS_NET_IPV4_STRING_LENGTH];
    int rssi;
} os_net_info_t;

typedef struct {
    uint32_t sequence;
    uint32_t time_ms;
    uint32_t ttl;
    int timeout;
} os_net_ping_reply_t;

typedef struct {
    uint32_t transmitted;
    uint32_t received;
    uint32_t duration_ms;
} os_net_ping_summary_t;

typedef int (*os_net_scan_callback_t)(const os_net_access_point_t *access_point,
                                      void *context);
typedef void (*os_net_ping_callback_t)(const os_net_ping_reply_t *reply,
                                       void *context);

void os_print(const char *text);
uint32_t os_uptime_ms(void);
void minios_sleep(uint32_t milliseconds);
static inline void os_sleep(uint32_t milliseconds)
{
    minios_sleep(milliseconds);
}
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
int os_fs_resolve_path(const char *path, char *resolved, size_t length);
int os_fs_getcwd(char *path, size_t length);
int os_fs_chdir(const char *path);
int os_fs_list(const char *path, os_fs_list_callback_t callback, void *context);
int os_fs_read(const char *path, os_fs_read_callback_t callback, void *context);
int os_fs_write(const char *path, const char *data, int append);
int os_fs_mkdir(const char *path);
int os_fs_remove(const char *path);

int os_device_init(void);
int os_device_register(const minios_device_t *device);
size_t os_device_count(void);
const minios_device_t *os_device_at(size_t index);
const minios_device_t *os_device_find(const char *name);

int os_net_init(void);
int os_net_scan(os_net_scan_callback_t callback, void *context, size_t *found);
int os_net_connect(const char *ssid, const char *password,
                   uint32_t timeout_ms);
int os_net_connect_saved(uint32_t timeout_ms);
int os_net_disconnect(void);
void os_net_get_info(os_net_info_t *info);
int os_net_ping(const char *host, uint32_t count,
                os_net_ping_callback_t callback, void *context,
                os_net_ping_summary_t *summary);
