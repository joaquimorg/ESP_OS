#include "minios_web.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "minios.h"
#include "minios_console.h"
#include "minios_fs.h"
#include "minios_module.h"
#include "minios_shell.h"
#include "sdkconfig.h"

#define WEB_INPUT_BUFFER_SIZE 512U
#define WEB_INPUT_TRIGGER_LEVEL 1U
#define WEB_FRAME_MAX 512U
#define WEB_QUERY_MAX (OS_FS_PATH_MAX * 3U + 32U)
#define WEB_TASK_PRIORITY 5U

typedef struct {
    httpd_handle_t server;
    int socket;
    volatile bool active;
    StaticStreamBuffer_t input_storage;
    unsigned char input_data[WEB_INPUT_BUFFER_SIZE];
    StreamBufferHandle_t input;
} web_shell_t;

typedef struct {
    httpd_req_t *request;
    bool first;
    bool device_directory;
    bool module_directory;
} list_context_t;

static httpd_handle_t web_server;
static web_shell_t web_shell = {.socket = -1};

static void set_cors(httpd_req_t *request)
{
    httpd_resp_set_hdr(request, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(request, "Access-Control-Allow-Methods",
                       "GET, PUT, POST, DELETE, OPTIONS");
    httpd_resp_set_hdr(request, "Access-Control-Allow-Headers",
                       "Content-Type, X-MiniOS-Token");
    httpd_resp_set_hdr(request, "Access-Control-Allow-Private-Network",
                       "true");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
}

static esp_err_t send_json(httpd_req_t *request, const char *status,
                           const char *body)
{
    set_cors(request);
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, body);
}

static int hex_digit(char character)
{
    if ((character >= '0') && (character <= '9')) {
        return character - '0';
    }
    if ((character >= 'a') && (character <= 'f')) {
        return character - 'a' + 10;
    }
    if ((character >= 'A') && (character <= 'F')) {
        return character - 'A' + 10;
    }
    return -1;
}

static int url_decode(const char *source, char *destination, size_t size)
{
    size_t used = 0U;

    while (*source != '\0') {
        unsigned char value;

        if (*source == '%') {
            int high;
            int low;

            if ((source[1] == '\0') || (source[2] == '\0') ||
                ((high = hex_digit(source[1])) < 0) ||
                ((low = hex_digit(source[2])) < 0)) {
                return -1;
            }
            value = (unsigned char)((high << 4) | low);
            source += 3;
        } else {
            value = (unsigned char)((*source == '+') ? ' ' : *source);
            ++source;
        }
        if ((value == 0U) || ((used + 1U) >= size)) {
            return -1;
        }
        destination[used++] = (char)value;
    }
    destination[used] = '\0';
    return 0;
}

static int query_value(httpd_req_t *request, const char *key, char *value,
                       size_t size)
{
    char query[WEB_QUERY_MAX];
    char encoded[WEB_QUERY_MAX];

    if ((httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK) ||
        (httpd_query_key_value(query, key, encoded, sizeof(encoded)) != ESP_OK)) {
        return -1;
    }
    return url_decode(encoded, value, size);
}

static bool request_authorized(httpd_req_t *request, bool websocket)
{
    const char configured[] = CONFIG_MINIOS_WEB_ACCESS_TOKEN;
    char supplied[OS_CONFIG_VALUE_MAX_LENGTH + 1U];
    size_t length;

    if (configured[0] == '\0') {
        return true;
    }
    if (websocket) {
        return (query_value(request, "token", supplied, sizeof(supplied)) == 0) &&
               (strcmp(configured, supplied) == 0);
    }
    length = httpd_req_get_hdr_value_len(request, "X-MiniOS-Token");
    return (length > 0U) && (length < sizeof(supplied)) &&
           (httpd_req_get_hdr_value_str(request, "X-MiniOS-Token", supplied,
                                        sizeof(supplied)) == ESP_OK) &&
           (strcmp(configured, supplied) == 0);
}

static bool require_http_auth(httpd_req_t *request)
{
    if (request_authorized(request, false)) {
        return true;
    }
    (void)send_json(request, "401 Unauthorized",
                    "{\"error\":\"unauthorized\"}");
    return false;
}

static const char *fs_error_string(int result)
{
    switch (result) {
    case OS_FS_NOT_FOUND:
        return "not found";
    case OS_FS_NOT_DIRECTORY:
        return "not a directory";
    case OS_FS_IS_DIRECTORY:
        return "is a directory";
    case OS_FS_ALREADY_EXISTS:
        return "already exists";
    case OS_FS_NOT_EMPTY:
        return "directory is not empty";
    case OS_FS_PATH_TOO_LONG:
        return "path too long";
    case OS_FS_IS_DEVICE:
        return "device namespace is protected";
    case OS_FS_READ_ONLY:
        return "namespace is read-only";
    default:
        return "filesystem error";
    }
}

static esp_err_t send_fs_error(httpd_req_t *request, int result)
{
    char body[96];
    const char *status = (result == OS_FS_NOT_FOUND) ? "404 Not Found"
                                                     : "400 Bad Request";

    (void)snprintf(body, sizeof(body), "{\"error\":\"%s\"}",
                   fs_error_string(result));
    return send_json(request, status, body);
}

static size_t json_escape(const char *source, char *destination, size_t size)
{
    size_t used = 0U;

    while ((*source != '\0') && ((used + 1U) < size)) {
        unsigned char character = (unsigned char)*source++;

        if ((character == '"') || (character == '\\')) {
            if ((used + 2U) >= size) {
                break;
            }
            destination[used++] = '\\';
            destination[used++] = (char)character;
        } else if (character >= 0x20U) {
            destination[used++] = (char)character;
        }
    }
    destination[used] = '\0';
    return used;
}

static int list_entry(const char *name, int is_directory, size_t size,
                      void *context)
{
    list_context_t *list = (list_context_t *)context;
    char escaped[OS_FS_NAME_MAX * 2U + 1U];
    char item[OS_FS_NAME_MAX * 2U + 96U];
    int length;

    if ((list->device_directory && (os_device_find(name) != NULL)) ||
        list->module_directory) {
        return 0;
    }

    (void)json_escape(name, escaped, sizeof(escaped));
    length = snprintf(item, sizeof(item),
                      "%s{\"name\":\"%s\",\"directory\":%s,\"size\":%u}",
                      list->first ? "" : ",", escaped,
                      is_directory ? "true" : "false", (unsigned int)size);
    if ((length < 0) || ((size_t)length >= sizeof(item)) ||
        (httpd_resp_send_chunk(list->request, item, (ssize_t)length) != ESP_OK)) {
        return -1;
    }
    list->first = false;
    return 0;
}

static int list_virtual_entry(list_context_t *list, const char *name,
                              const char *kind, const char *detail)
{
    char escaped_name[OS_FS_NAME_MAX * 2U + 1U];
    char escaped_detail[193];
    char item[512];
    int length;

    (void)json_escape(name, escaped_name, sizeof(escaped_name));
    (void)json_escape(detail, escaped_detail, sizeof(escaped_detail));
    length = snprintf(item, sizeof(item),
                      "%s{\"name\":\"%s\",\"directory\":false,\"size\":0,"
                      "\"virtual\":true,\"kind\":\"%s\",\"detail\":\"%s\"}",
                      list->first ? "" : ",", escaped_name, kind,
                      escaped_detail);
    if ((length < 0) || ((size_t)length >= sizeof(item)) ||
        (httpd_resp_send_chunk(list->request, item, (ssize_t)length) != ESP_OK)) {
        return -1;
    }
    list->first = false;
    return 0;
}

static int list_devices(list_context_t *context)
{
    size_t index;

    for (index = 0U; index < os_device_count(); ++index) {
        const minios_device_t *device = os_device_at(index);
        char detail[192];

        if (device == NULL) {
            continue;
        }
        (void)snprintf(detail, sizeof(detail), "%s - %s",
                       device->driver, device->description);
        if (list_virtual_entry(context, device->name, "device", detail) != 0) {
            return -1;
        }
    }
    return 0;
}

static int list_modules(list_context_t *context)
{
    size_t index;

    for (index = 0U; index < minios_module_count(); ++index) {
        const minios_module_info_t *module = minios_module_at(index);
        char detail[192];

        if (module == NULL) {
            continue;
        }
        (void)snprintf(detail, sizeof(detail), "%s - %s",
                       module->loaded ? "loaded" : "available",
                       module->description);
        if (list_virtual_entry(context, module->name, "module", detail) != 0) {
            return -1;
        }
    }
    return 0;
}

static esp_err_t options_handler(httpd_req_t *request)
{
    set_cors(request);
    return httpd_resp_send(request, NULL, 0);
}

static esp_err_t info_handler(httpd_req_t *request)
{
    os_fs_space_info_t space = {0};
    char body[192];

    if (!require_http_auth(request)) {
        return ESP_OK;
    }
    (void)os_fs_get_space_info(&space);
    (void)snprintf(body, sizeof(body),
                   "{\"name\":\"%s\",\"version\":\"%s\",\"api\":%u,"
                   "\"filesystem\":{\"total\":%u,\"used\":%u,\"free\":%u}}",
                   MINIOS_NAME, MINIOS_VERSION, MINIOS_API_VERSION,
                   (unsigned int)space.total, (unsigned int)space.used,
                   (unsigned int)space.free);
    return send_json(request, "200 OK", body);
}

static esp_err_t list_handler(httpd_req_t *request)
{
    char path[OS_FS_PATH_MAX];
    char resolved[OS_FS_PATH_MAX];
    list_context_t context = {.request = request, .first = true};
    int result;

    if (!require_http_auth(request)) {
        return ESP_OK;
    }
    if (query_value(request, "path", path, sizeof(path)) != 0) {
        return send_json(request, "400 Bad Request", "{\"error\":\"missing path\"}");
    }
    result = os_fs_resolve_path(path, resolved, sizeof(resolved));
    if (result != OS_FS_OK) {
        return send_fs_error(request, result);
    }
    context.device_directory = strcmp(resolved, "/dev") == 0;
    context.module_directory = strcmp(resolved, "/modules") == 0;
    set_cors(request);
    httpd_resp_set_type(request, "application/json");
    if (httpd_resp_send_chunk(request, "[", 1) != ESP_OK) {
        return ESP_FAIL;
    }
    result = os_fs_list(path, list_entry, &context);
    if (result != OS_FS_OK) {
        (void)httpd_resp_send_chunk(request, "]", 1);
        (void)httpd_resp_send_chunk(request, NULL, 0);
        return ESP_FAIL;
    }
    if ((context.device_directory && (list_devices(&context) != 0)) ||
        (context.module_directory && (list_modules(&context) != 0))) {
        (void)httpd_resp_send_chunk(request, "]", 1);
        (void)httpd_resp_send_chunk(request, NULL, 0);
        return ESP_FAIL;
    }
    if (httpd_resp_send_chunk(request, "]", 1) != ESP_OK) {
        return ESP_FAIL;
    }
    return httpd_resp_send_chunk(request, NULL, 0);
}

static int download_chunk(const char *data, size_t length, void *context)
{
    return (httpd_resp_send_chunk((httpd_req_t *)context, data,
                                  (ssize_t)length) == ESP_OK)
               ? 0
               : -1;
}

static esp_err_t download_handler(httpd_req_t *request)
{
    char path[OS_FS_PATH_MAX];
    int result;

    if (!require_http_auth(request)) {
        return ESP_OK;
    }
    if (query_value(request, "path", path, sizeof(path)) != 0) {
        return send_json(request, "400 Bad Request", "{\"error\":\"missing path\"}");
    }
    set_cors(request);
    httpd_resp_set_type(request, "application/octet-stream");
    httpd_resp_set_hdr(request, "Content-Disposition",
                       "attachment; filename=\"minios-download.bin\"");
    result = os_fs_read(path, download_chunk, request);
    if (result != OS_FS_OK) {
        return ESP_FAIL;
    }
    return httpd_resp_send_chunk(request, NULL, 0);
}

static esp_err_t upload_handler(httpd_req_t *request)
{
    char path[OS_FS_PATH_MAX];
    unsigned char *data;
    size_t used = 0U;
    int result;

    if (!require_http_auth(request)) {
        return ESP_OK;
    }
    if ((query_value(request, "path", path, sizeof(path)) != 0) ||
        ((size_t)request->content_len > CONFIG_MINIOS_WEB_MAX_UPLOAD_SIZE)) {
        return send_json(request, "400 Bad Request",
                         "{\"error\":\"missing path or upload too large\"}");
    }
    data = malloc((request->content_len == 0) ? 1U : (size_t)request->content_len);
    if (data == NULL) {
        return send_json(request, "503 Service Unavailable",
                         "{\"error\":\"not enough memory\"}");
    }
    while (used < (size_t)request->content_len) {
        int received = httpd_req_recv(request, (char *)data + used,
                                      (size_t)request->content_len - used);

        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (received <= 0) {
            free(data);
            return ESP_FAIL;
        }
        used += (size_t)received;
    }
    result = minios_fs_replace(path, (const char *)data, used);
    free(data);
    return (result == OS_FS_OK)
               ? send_json(request, "200 OK", "{\"ok\":true}")
               : send_fs_error(request, result);
}

static esp_err_t mkdir_handler(httpd_req_t *request)
{
    char path[OS_FS_PATH_MAX];
    int result;

    if (!require_http_auth(request)) {
        return ESP_OK;
    }
    if (query_value(request, "path", path, sizeof(path)) != 0) {
        return send_json(request, "400 Bad Request", "{\"error\":\"missing path\"}");
    }
    result = os_fs_mkdir(path);
    return (result == OS_FS_OK)
               ? send_json(request, "200 OK", "{\"ok\":true}")
               : send_fs_error(request, result);
}

static esp_err_t remove_handler(httpd_req_t *request)
{
    char path[OS_FS_PATH_MAX];
    int result;

    if (!require_http_auth(request)) {
        return ESP_OK;
    }
    if (query_value(request, "path", path, sizeof(path)) != 0) {
        return send_json(request, "400 Bad Request", "{\"error\":\"missing path\"}");
    }
    result = os_fs_remove(path);
    return (result == OS_FS_OK)
               ? send_json(request, "200 OK", "{\"ok\":true}")
               : send_fs_error(request, result);
}

static int websocket_read(void *context, char *buffer, size_t length)
{
    web_shell_t *shell = (web_shell_t *)context;
    size_t received;

    if (!shell->active) {
        return -1;
    }
    received = xStreamBufferReceive(shell->input, buffer, length,
                                    pdMS_TO_TICKS(100U));
    return (received == 0U) ? (shell->active ? 0 : -1) : (int)received;
}

static int websocket_write(void *context, const char *buffer, size_t length)
{
    web_shell_t *shell = (web_shell_t *)context;
    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)buffer,
        .len = length,
    };

    if (!shell->active ||
        (httpd_ws_get_fd_info(shell->server, shell->socket) !=
         HTTPD_WS_CLIENT_WEBSOCKET) ||
        (httpd_ws_send_data(shell->server, shell->socket, &frame) != ESP_OK)) {
        shell->active = false;
        return -1;
    }
    return (int)length;
}

static void websocket_shell_task(void *argument)
{
    web_shell_t *shell = (web_shell_t *)argument;
    minios_console_t console = {
        .read = websocket_read,
        .write = websocket_write,
        .close = NULL,
        .context = shell,
    };

    vTaskDelay(pdMS_TO_TICKS(50U));
    minios_console_write_text(&console, "\r\nMiniOS WebShell\r\n");
    minios_shell_run_console(&console);
    shell->active = false;
    shell->socket = -1;
    vTaskDelete(NULL);
}

static esp_err_t websocket_authorize(httpd_req_t *request)
{
    return request_authorized(request, true) ? ESP_OK : ESP_FAIL;
}

static esp_err_t websocket_handler(httpd_req_t *request)
{
    if (request->method == HTTP_GET) {
        BaseType_t created;

        if (web_shell.active) {
            return ESP_FAIL;
        }
        web_shell.server = request->handle;
        web_shell.socket = httpd_req_to_sockfd(request);
        web_shell.active = true;
        xStreamBufferReset(web_shell.input);
        created = xTaskCreate(websocket_shell_task, "web_shell",
                              CONFIG_MINIOS_WEB_STACK_SIZE, &web_shell,
                              WEB_TASK_PRIORITY, NULL);
        if (created != pdPASS) {
            web_shell.active = false;
            web_shell.socket = -1;
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }
    {
        httpd_ws_frame_t frame = {0};
        unsigned char payload[WEB_FRAME_MAX];
        esp_err_t result = httpd_ws_recv_frame(request, &frame, 0);

        if (result != ESP_OK) {
            return result;
        }
        if (frame.len > sizeof(payload)) {
            web_shell.active = false;
            return ESP_ERR_INVALID_SIZE;
        }
        frame.payload = payload;
        result = httpd_ws_recv_frame(request, &frame, sizeof(payload));
        if (result != ESP_OK) {
            return result;
        }
        if ((frame.type == HTTPD_WS_TYPE_TEXT) ||
            (frame.type == HTTPD_WS_TYPE_BINARY)) {
            size_t sent = xStreamBufferSend(web_shell.input, payload, frame.len,
                                            pdMS_TO_TICKS(100U));

            return (sent == frame.len) ? ESP_OK : ESP_FAIL;
        }
        return ESP_OK;
    }
}

static void session_closed(httpd_handle_t server, int socket)
{
    (void)server;
    if (web_shell.socket == socket) {
        web_shell.active = false;
    }
}

static const httpd_uri_t routes[] = {
    {.uri = "/api/info", .method = HTTP_GET, .handler = info_handler},
    {.uri = "/api/fs/list", .method = HTTP_GET, .handler = list_handler},
    {.uri = "/api/fs/download", .method = HTTP_GET, .handler = download_handler},
    {.uri = "/api/fs/upload", .method = HTTP_PUT, .handler = upload_handler},
    {.uri = "/api/fs/mkdir", .method = HTTP_POST, .handler = mkdir_handler},
    {.uri = "/api/fs", .method = HTTP_DELETE, .handler = remove_handler},
    {.uri = "/api/shell", .method = HTTP_GET, .handler = websocket_handler,
     .is_websocket = true, .ws_pre_handshake_cb = websocket_authorize},
    {.uri = "/*", .method = HTTP_OPTIONS, .handler = options_handler},
};

int minios_web_start(void)
{
    httpd_config_t configuration = HTTPD_DEFAULT_CONFIG();
    size_t index;

    if (web_server != NULL) {
        return MINIOS_WEB_OK;
    }
    web_shell.input = xStreamBufferCreateStatic(
        sizeof(web_shell.input_data), WEB_INPUT_TRIGGER_LEVEL,
        web_shell.input_data, &web_shell.input_storage);
    if (web_shell.input == NULL) {
        return MINIOS_WEB_ERROR;
    }
    configuration.server_port = CONFIG_MINIOS_WEB_PORT;
    configuration.stack_size = CONFIG_MINIOS_WEB_STACK_SIZE;
    configuration.max_uri_handlers = sizeof(routes) / sizeof(routes[0]);
    configuration.lru_purge_enable = true;
    configuration.close_fn = session_closed;
    configuration.uri_match_fn = httpd_uri_match_wildcard;
    if (httpd_start(&web_server, &configuration) != ESP_OK) {
        web_server = NULL;
        return MINIOS_WEB_ERROR;
    }
    for (index = 0U; index < (sizeof(routes) / sizeof(routes[0])); ++index) {
        if (httpd_register_uri_handler(web_server, &routes[index]) != ESP_OK) {
            (void)httpd_stop(web_server);
            web_server = NULL;
            return MINIOS_WEB_ERROR;
        }
    }
    return MINIOS_WEB_OK;
}
