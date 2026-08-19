#include "minios_config.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

#define CONFIG_NAMESPACE "minios"
#define CONFIG_RECORD_MAGIC 0x4d434647UL
#define CONFIG_RECORD_VERSION 1U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    char key[OS_CONFIG_KEY_MAX_LENGTH + 1U];
    char value[OS_CONFIG_VALUE_MAX_LENGTH + 1U];
} config_record_t;

static bool config_initialized;

static bool config_key_is_valid(const char *key)
{
    size_t length;
    size_t index;

    if (key == NULL) {
        return false;
    }

    length = strnlen(key, OS_CONFIG_KEY_MAX_LENGTH + 1U);
    if ((length == 0U) || (length > OS_CONFIG_KEY_MAX_LENGTH)) {
        return false;
    }

    for (index = 0U; index < length; ++index) {
        char character = key[index];
        bool valid = ((character >= 'a') && (character <= 'z')) ||
                     ((character >= 'A') && (character <= 'Z')) ||
                     ((character >= '0') && (character <= '9')) ||
                     (character == '.') || (character == '_') ||
                     (character == '-');
        if (!valid) {
            return false;
        }
    }
    return true;
}

static uint64_t config_key_hash(const char *key)
{
    const unsigned char *cursor = (const unsigned char *)key;
    uint64_t hash = UINT64_C(14695981039346656037);

    while (*cursor != '\0') {
        hash ^= (uint64_t)*cursor;
        hash *= UINT64_C(1099511628211);
        ++cursor;
    }
    return hash;
}

static void config_storage_key(const char *key, char storage_key[NVS_KEY_NAME_MAX_SIZE])
{
    static const char alphabet[] = "0123456789abcdefghijklmnopqrstuv";
    uint64_t hash = config_key_hash(key);
    int index;

    storage_key[0] = 'c';
    for (index = 13; index >= 1; --index) {
        storage_key[index] = alphabet[hash & UINT64_C(31)];
        hash >>= 5U;
    }
    storage_key[14] = '\0';
}

static bool config_record_is_valid(const config_record_t *record)
{
    return (record->magic == CONFIG_RECORD_MAGIC) &&
           (record->version == CONFIG_RECORD_VERSION) &&
           (memchr(record->key, '\0', sizeof(record->key)) != NULL) &&
           (memchr(record->value, '\0', sizeof(record->value)) != NULL);
}

static int config_read_record(nvs_handle_t handle,
                              const char *storage_key,
                              config_record_t *record)
{
    size_t size = sizeof(*record);
    esp_err_t error = nvs_get_blob(handle, storage_key, record, &size);

    if (error == ESP_ERR_NVS_NOT_FOUND) {
        return OS_CONFIG_NOT_FOUND;
    }
    if ((error != ESP_OK) || (size != sizeof(*record)) ||
        !config_record_is_valid(record)) {
        return OS_CONFIG_ERROR;
    }
    return OS_CONFIG_OK;
}

int os_config_init(void)
{
    esp_err_t error = nvs_flash_init();

    if ((error == ESP_ERR_NVS_NO_FREE_PAGES) ||
        (error == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        error = nvs_flash_erase();
        if (error == ESP_OK) {
            error = nvs_flash_init();
        }
    }

    config_initialized = (error == ESP_OK);
    return config_initialized ? OS_CONFIG_OK : OS_CONFIG_ERROR;
}

int os_config_get(const char *key, char *value, size_t length)
{
    char storage_key[NVS_KEY_NAME_MAX_SIZE];
    config_record_t record;
    nvs_handle_t handle;
    size_t value_length;
    int result;

    if (!config_initialized || !config_key_is_valid(key) ||
        (value == NULL) || (length == 0U)) {
        return OS_CONFIG_INVALID_ARGUMENT;
    }

    config_storage_key(key, storage_key);
    esp_err_t error = nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        return OS_CONFIG_NOT_FOUND;
    }
    if (error != ESP_OK) {
        return OS_CONFIG_ERROR;
    }

    result = config_read_record(handle, storage_key, &record);
    nvs_close(handle);
    if (result != OS_CONFIG_OK) {
        return result;
    }
    if (strcmp(record.key, key) != 0) {
        return OS_CONFIG_KEY_COLLISION;
    }

    value_length = strlen(record.value) + 1U;
    if (value_length > length) {
        return OS_CONFIG_BUFFER_TOO_SMALL;
    }
    memcpy(value, record.value, value_length);
    return OS_CONFIG_OK;
}

int os_config_set(const char *key, const char *value)
{
    char storage_key[NVS_KEY_NAME_MAX_SIZE];
    config_record_t existing;
    config_record_t record = {0};
    nvs_handle_t handle;
    size_t value_length;
    int existing_result;
    esp_err_t error;

    if (!config_initialized || !config_key_is_valid(key) || (value == NULL)) {
        return OS_CONFIG_INVALID_ARGUMENT;
    }
    value_length = strnlen(value, OS_CONFIG_VALUE_MAX_LENGTH + 1U);
    if (value_length > OS_CONFIG_VALUE_MAX_LENGTH) {
        return OS_CONFIG_INVALID_ARGUMENT;
    }

    config_storage_key(key, storage_key);
    if (nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return OS_CONFIG_ERROR;
    }

    existing_result = config_read_record(handle, storage_key, &existing);
    if ((existing_result == OS_CONFIG_OK) && (strcmp(existing.key, key) != 0)) {
        nvs_close(handle);
        return OS_CONFIG_KEY_COLLISION;
    }
    if ((existing_result != OS_CONFIG_OK) &&
        (existing_result != OS_CONFIG_NOT_FOUND)) {
        nvs_close(handle);
        return existing_result;
    }

    record.magic = CONFIG_RECORD_MAGIC;
    record.version = CONFIG_RECORD_VERSION;
    memcpy(record.key, key, strlen(key) + 1U);
    memcpy(record.value, value, value_length + 1U);

    error = nvs_set_blob(handle, storage_key, &record, sizeof(record));
    if (error == ESP_OK) {
        error = nvs_commit(handle);
    }
    nvs_close(handle);
    return (error == ESP_OK) ? OS_CONFIG_OK : OS_CONFIG_ERROR;
}

int os_config_delete(const char *key)
{
    char storage_key[NVS_KEY_NAME_MAX_SIZE];
    config_record_t record;
    nvs_handle_t handle;
    int result;
    esp_err_t error;

    if (!config_initialized || !config_key_is_valid(key)) {
        return OS_CONFIG_INVALID_ARGUMENT;
    }

    config_storage_key(key, storage_key);
    error = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        return OS_CONFIG_NOT_FOUND;
    }
    if (error != ESP_OK) {
        return OS_CONFIG_ERROR;
    }

    result = config_read_record(handle, storage_key, &record);
    if (result != OS_CONFIG_OK) {
        nvs_close(handle);
        return result;
    }
    if (strcmp(record.key, key) != 0) {
        nvs_close(handle);
        return OS_CONFIG_KEY_COLLISION;
    }

    error = nvs_erase_key(handle, storage_key);
    if (error == ESP_OK) {
        error = nvs_commit(handle);
    }
    nvs_close(handle);
    return (error == ESP_OK) ? OS_CONFIG_OK : OS_CONFIG_ERROR;
}

int os_config_list(os_config_list_callback_t callback, void *context)
{
    nvs_iterator_t iterator = NULL;
    nvs_handle_t handle;
    esp_err_t error;

    if (!config_initialized || (callback == NULL)) {
        return OS_CONFIG_INVALID_ARGUMENT;
    }

    error = nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        return OS_CONFIG_OK;
    }
    if (error != ESP_OK) {
        return OS_CONFIG_ERROR;
    }

    error = nvs_entry_find_in_handle(handle, NVS_TYPE_BLOB, &iterator);
    while (error == ESP_OK) {
        nvs_entry_info_t info;
        config_record_t record;

        error = nvs_entry_info(iterator, &info);
        if (error != ESP_OK) {
            break;
        }
        if (config_read_record(handle, info.key, &record) != OS_CONFIG_OK) {
            error = ESP_FAIL;
            break;
        }
        if (callback(record.key, record.value, context) != 0) {
            error = ESP_FAIL;
            break;
        }
        error = nvs_entry_next(&iterator);
    }

    nvs_release_iterator(iterator);
    nvs_close(handle);
    return ((error == ESP_OK) || (error == ESP_ERR_NVS_NOT_FOUND))
               ? OS_CONFIG_OK
               : OS_CONFIG_ERROR;
}
