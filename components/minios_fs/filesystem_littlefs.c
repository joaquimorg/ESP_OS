#include "minios_fs.h"

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_littlefs.h"

#define FS_BASE_PATH "/fs"
#define FS_PARTITION_LABEL "storage"
#define FS_READ_BUFFER_SIZE 128

static bool fs_initialized;
static char fs_cwd[OS_FS_PATH_MAX] = "/";

static int result_from_errno(int error)
{
    switch (error) {
    case ENOENT:
        return OS_FS_NOT_FOUND;
    case ENOTDIR:
        return OS_FS_NOT_DIRECTORY;
    case EISDIR:
        return OS_FS_IS_DIRECTORY;
    case EEXIST:
        return OS_FS_ALREADY_EXISTS;
    case ENOTEMPTY:
        return OS_FS_NOT_EMPTY;
    case ENAMETOOLONG:
        return OS_FS_PATH_TOO_LONG;
    default:
        return OS_FS_ERROR;
    }
}

static int push_segment(char *path, size_t *length,
                        const char *segment, size_t segment_length)
{
    size_t start;

    if ((segment_length == 0U) ||
        ((segment_length == 1U) && (segment[0] == '.'))) {
        return OS_FS_OK;
    }
    if ((segment_length == 2U) && (segment[0] == '.') &&
        (segment[1] == '.')) {
        if (*length > 1U) {
            start = *length;
            while ((start > 1U) && (path[start - 1U] != '/')) {
                --start;
            }
            *length = (start > 1U) ? start - 1U : 1U;
            path[*length] = '\0';
        }
        return OS_FS_OK;
    }
    if (segment_length > OS_FS_NAME_MAX) {
        return OS_FS_PATH_TOO_LONG;
    }
    if ((*length + ((*length > 1U) ? 1U : 0U) + segment_length) >=
        OS_FS_PATH_MAX) {
        return OS_FS_PATH_TOO_LONG;
    }
    if (*length > 1U) {
        path[(*length)++] = '/';
    }
    memcpy(path + *length, segment, segment_length);
    *length += segment_length;
    path[*length] = '\0';
    return OS_FS_OK;
}

static int normalize_path(const char *input, char output[OS_FS_PATH_MAX])
{
    const char *cursor;
    size_t length;

    if ((input == NULL) || (input[0] == '\0')) {
        return OS_FS_INVALID_ARGUMENT;
    }
    if (input[0] == '/') {
        output[0] = '/';
        output[1] = '\0';
        length = 1U;
    } else {
        length = strnlen(fs_cwd, OS_FS_PATH_MAX);
        if (length >= OS_FS_PATH_MAX) {
            return OS_FS_ERROR;
        }
        memcpy(output, fs_cwd, length + 1U);
    }

    cursor = input;
    while (*cursor != '\0') {
        const char *segment;
        size_t segment_length;
        int result;

        while (*cursor == '/') {
            ++cursor;
        }
        segment = cursor;
        while ((*cursor != '\0') && (*cursor != '/')) {
            ++cursor;
        }
        segment_length = (size_t)(cursor - segment);
        result = push_segment(output, &length, segment, segment_length);
        if (result != OS_FS_OK) {
            return result;
        }
    }
    return OS_FS_OK;
}

static int physical_path(const char *logical, char *output, size_t length,
                         char normalized[OS_FS_PATH_MAX])
{
    int result = normalize_path(logical, normalized);
    int written;

    if (result != OS_FS_OK) {
        return result;
    }
    written = snprintf(output, length, "%s%s", FS_BASE_PATH, normalized);
    if ((written < 0) || ((size_t)written >= length)) {
        return OS_FS_PATH_TOO_LONG;
    }
    return OS_FS_OK;
}

static int ensure_directory(const char *path)
{
    if (mkdir(path, 0755) == 0) {
        return OS_FS_OK;
    }
    return (errno == EEXIST) ? OS_FS_OK : result_from_errno(errno);
}

int os_fs_init(void)
{
    static const char *const initial_directories[] = {
        "bin", "boot", "dev", "etc", "home", "modules", "tmp", "var"
    };
    const esp_vfs_littlefs_conf_t configuration = {
        .base_path = FS_BASE_PATH,
        .partition_label = FS_PARTITION_LABEL,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    size_t index;
    esp_err_t error;

    if (fs_initialized) {
        return OS_FS_OK;
    }
    error = esp_vfs_littlefs_register(&configuration);
    if (error != ESP_OK) {
        return OS_FS_ERROR;
    }
    fs_initialized = true;
    memcpy(fs_cwd, "/", 2U);

    for (index = 0U;
         index < (sizeof(initial_directories) / sizeof(initial_directories[0]));
         ++index) {
        char path[sizeof(FS_BASE_PATH) + OS_FS_NAME_MAX + 1U];
        int written = snprintf(path, sizeof(path), "%s/%s", FS_BASE_PATH,
                               initial_directories[index]);
        if ((written < 0) || ((size_t)written >= sizeof(path)) ||
            (ensure_directory(path) != OS_FS_OK)) {
            esp_vfs_littlefs_unregister(FS_PARTITION_LABEL);
            fs_initialized = false;
            return OS_FS_ERROR;
        }
    }
    return OS_FS_OK;
}

int os_fs_getcwd(char *path, size_t length)
{
    size_t required;

    if (!fs_initialized || (path == NULL)) {
        return OS_FS_INVALID_ARGUMENT;
    }
    required = strlen(fs_cwd) + 1U;
    if (required > length) {
        return OS_FS_PATH_TOO_LONG;
    }
    memcpy(path, fs_cwd, required);
    return OS_FS_OK;
}

int os_fs_resolve_path(const char *path, char *resolved, size_t length)
{
    char normalized[OS_FS_PATH_MAX];
    size_t required;
    int result;

    if (!fs_initialized || (resolved == NULL) || (length == 0U)) {
        return OS_FS_INVALID_ARGUMENT;
    }
    result = normalize_path(path, normalized);
    if (result != OS_FS_OK) {
        return result;
    }
    required = strlen(normalized) + 1U;
    if (required > length) {
        return OS_FS_PATH_TOO_LONG;
    }
    memcpy(resolved, normalized, required);
    return OS_FS_OK;
}

int os_fs_chdir(const char *path)
{
    char logical[OS_FS_PATH_MAX];
    char physical[sizeof(FS_BASE_PATH) + OS_FS_PATH_MAX];
    struct stat status;
    int result;

    if (!fs_initialized) {
        return OS_FS_ERROR;
    }
    result = physical_path(path, physical, sizeof(physical), logical);
    if (result != OS_FS_OK) {
        return result;
    }
    if (stat(physical, &status) != 0) {
        return result_from_errno(errno);
    }
    if (!S_ISDIR(status.st_mode)) {
        return OS_FS_NOT_DIRECTORY;
    }
    memcpy(fs_cwd, logical, strlen(logical) + 1U);
    return OS_FS_OK;
}

int os_fs_list(const char *path, os_fs_list_callback_t callback, void *context)
{
    char logical[OS_FS_PATH_MAX];
    char physical[sizeof(FS_BASE_PATH) + OS_FS_PATH_MAX];
    DIR *directory;
    struct dirent *entry;
    int result;

    if (!fs_initialized || (callback == NULL)) {
        return OS_FS_INVALID_ARGUMENT;
    }
    result = physical_path((path == NULL) ? "." : path, physical,
                           sizeof(physical), logical);
    if (result != OS_FS_OK) {
        return result;
    }
    directory = opendir(physical);
    if (directory == NULL) {
        return result_from_errno(errno);
    }
    result = OS_FS_OK;
    while ((entry = readdir(directory)) != NULL) {
        char child[sizeof(physical) + OS_FS_NAME_MAX + 1U];
        struct stat status;
        int written;

        if ((strcmp(entry->d_name, ".") == 0) ||
            (strcmp(entry->d_name, "..") == 0)) {
            continue;
        }
        written = snprintf(child, sizeof(child), "%s/%s", physical,
                           entry->d_name);
        if ((written < 0) || ((size_t)written >= sizeof(child)) ||
            (stat(child, &status) != 0)) {
            result = OS_FS_ERROR;
            break;
        }
        if (callback(entry->d_name, S_ISDIR(status.st_mode),
                     (size_t)status.st_size, context) != 0) {
            result = OS_FS_ERROR;
            break;
        }
    }
    closedir(directory);
    return result;
}

int os_fs_read(const char *path, os_fs_read_callback_t callback, void *context)
{
    char logical[OS_FS_PATH_MAX];
    char physical[sizeof(FS_BASE_PATH) + OS_FS_PATH_MAX];
    char buffer[FS_READ_BUFFER_SIZE];
    FILE *file;
    size_t count;
    int result;

    if (!fs_initialized || (callback == NULL)) {
        return OS_FS_INVALID_ARGUMENT;
    }
    result = physical_path(path, physical, sizeof(physical), logical);
    if (result != OS_FS_OK) {
        return result;
    }
    file = fopen(physical, "rb");
    if (file == NULL) {
        return result_from_errno(errno);
    }
    result = OS_FS_OK;
    while ((count = fread(buffer, 1U, sizeof(buffer), file)) > 0U) {
        if (callback(buffer, count, context) != 0) {
            result = OS_FS_ERROR;
            break;
        }
    }
    if (ferror(file) != 0) {
        result = OS_FS_ERROR;
    }
    fclose(file);
    return result;
}

int os_fs_write(const char *path, const char *data, int append)
{
    char logical[OS_FS_PATH_MAX];
    char physical[sizeof(FS_BASE_PATH) + OS_FS_PATH_MAX];
    FILE *file;
    size_t length;
    int result;

    if (!fs_initialized || (data == NULL)) {
        return OS_FS_INVALID_ARGUMENT;
    }
    result = physical_path(path, physical, sizeof(physical), logical);
    if (result != OS_FS_OK) {
        return result;
    }
    file = fopen(physical, append ? "ab" : "wb");
    if (file == NULL) {
        return result_from_errno(errno);
    }
    length = strlen(data);
    if ((fwrite(data, 1U, length, file) != length) ||
        (fwrite("\n", 1U, 1U, file) != 1U)) {
        result = OS_FS_ERROR;
    } else {
        result = OS_FS_OK;
    }
    fclose(file);
    return result;
}

int os_fs_mkdir(const char *path)
{
    char logical[OS_FS_PATH_MAX];
    char physical[sizeof(FS_BASE_PATH) + OS_FS_PATH_MAX];
    int result;

    if (!fs_initialized) {
        return OS_FS_ERROR;
    }
    result = physical_path(path, physical, sizeof(physical), logical);
    if (result != OS_FS_OK) {
        return result;
    }
    if (strcmp(logical, "/") == 0) {
        return OS_FS_ALREADY_EXISTS;
    }
    return (mkdir(physical, 0755) == 0) ? OS_FS_OK : result_from_errno(errno);
}

int os_fs_remove(const char *path)
{
    char logical[OS_FS_PATH_MAX];
    char physical[sizeof(FS_BASE_PATH) + OS_FS_PATH_MAX];
    struct stat status;
    int result;

    if (!fs_initialized) {
        return OS_FS_ERROR;
    }
    result = physical_path(path, physical, sizeof(physical), logical);
    if (result != OS_FS_OK) {
        return result;
    }
    if (strcmp(logical, "/") == 0) {
        return OS_FS_INVALID_ARGUMENT;
    }
    if (stat(physical, &status) != 0) {
        return result_from_errno(errno);
    }
    if (S_ISDIR(status.st_mode)) {
        return (rmdir(physical) == 0) ? OS_FS_OK : result_from_errno(errno);
    }
    return (unlink(physical) == 0) ? OS_FS_OK : result_from_errno(errno);
}
