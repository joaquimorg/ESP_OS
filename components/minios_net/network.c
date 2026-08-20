#include "minios_net.h"

#include <stdbool.h>
#include <string.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "ping/ping_sock.h"

#define NET_CONNECTED_BIT BIT0
#define NET_FAILED_BIT BIT1
#define NET_DISCONNECTED_BIT BIT2
#define NET_CONNECT_RETRIES 5U
#define NET_SCAN_MAX_RESULTS 20U
#define NET_PING_TIMEOUT_MS 1000U
#define NET_PING_INTERVAL_MS 1000U

static StaticEventGroup_t net_event_storage;
static EventGroupHandle_t net_events;
static esp_netif_t *netif;
static bool net_initialized;
static volatile bool connection_pending;
static volatile unsigned int connection_retries;
static volatile os_net_state_t net_state = OS_NET_STATE_DISABLED;
static char current_ssid[OS_NET_SSID_MAX_LENGTH + 1U];

typedef struct {
    StaticSemaphore_t semaphore_storage;
    SemaphoreHandle_t complete;
    os_net_ping_callback_t callback;
    void *callback_context;
    os_net_ping_summary_t *summary;
} ping_context_t;

static void net_event_handler(void *argument, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    (void)argument;
    (void)event_data;

    if ((event_base == WIFI_EVENT) &&
        (event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        net_state = OS_NET_STATE_DISCONNECTED;
        xEventGroupClearBits(net_events, NET_CONNECTED_BIT);
        xEventGroupSetBits(net_events, NET_DISCONNECTED_BIT);
        if (connection_pending &&
            (connection_retries < NET_CONNECT_RETRIES)) {
            ++connection_retries;
            net_state = OS_NET_STATE_CONNECTING;
            (void)esp_wifi_connect();
        } else if (connection_pending) {
            connection_pending = false;
            xEventGroupSetBits(net_events, NET_FAILED_BIT);
        }
    } else if ((event_base == IP_EVENT) &&
               (event_id == IP_EVENT_STA_GOT_IP)) {
        connection_pending = false;
        net_state = OS_NET_STATE_CONNECTED;
        xEventGroupClearBits(net_events,
                             NET_FAILED_BIT | NET_DISCONNECTED_BIT);
        xEventGroupSetBits(net_events, NET_CONNECTED_BIT);
    }
}

static void ipv4_to_string(const esp_ip4_addr_t *address, char *output,
                           size_t output_length)
{
    if (esp_ip4addr_ntoa(address, output, (int)output_length) == NULL) {
        output[0] = '\0';
    }
}

int os_net_init(void)
{
    wifi_init_config_t wifi_configuration = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t error;

    if (net_initialized) {
        return OS_NET_OK;
    }
    net_events = xEventGroupCreateStatic(&net_event_storage);
    if (net_events == NULL) {
        return OS_NET_ERROR;
    }
    error = esp_netif_init();
    if ((error != ESP_OK) && (error != ESP_ERR_INVALID_STATE)) {
        return OS_NET_ERROR;
    }
    error = esp_event_loop_create_default();
    if ((error != ESP_OK) && (error != ESP_ERR_INVALID_STATE)) {
        return OS_NET_ERROR;
    }
    netif = esp_netif_create_default_wifi_sta();
    if (netif == NULL) {
        return OS_NET_ERROR;
    }
    if ((esp_wifi_init(&wifi_configuration) != ESP_OK) ||
        (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                    net_event_handler, NULL) != ESP_OK) ||
        (esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                    net_event_handler, NULL) != ESP_OK) ||
        (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) ||
        (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) ||
        (esp_wifi_start() != ESP_OK)) {
        return OS_NET_ERROR;
    }
    net_initialized = true;
    net_state = OS_NET_STATE_DISCONNECTED;
    return OS_NET_OK;
}

int os_net_scan(os_net_scan_callback_t callback, void *context, size_t *found)
{
    wifi_scan_config_t scan_configuration = {
        .show_hidden = true,
    };
    wifi_ap_record_t records[NET_SCAN_MAX_RESULTS];
    uint16_t count = NET_SCAN_MAX_RESULTS;
    size_t index;

    if (!net_initialized) {
        return OS_NET_NOT_INITIALIZED;
    }
    if ((callback == NULL) || (found == NULL)) {
        return OS_NET_INVALID_ARGUMENT;
    }
    if (net_state == OS_NET_STATE_CONNECTING) {
        return OS_NET_BUSY;
    }
    if (esp_wifi_scan_start(&scan_configuration, true) != ESP_OK) {
        return OS_NET_ERROR;
    }
    if (esp_wifi_scan_get_ap_records(&count, records) != ESP_OK) {
        return OS_NET_ERROR;
    }
    for (index = 0U; index < count; ++index) {
        os_net_access_point_t access_point = {0};
        size_t ssid_length = strnlen((const char *)records[index].ssid,
                                     OS_NET_SSID_MAX_LENGTH);

        memcpy(access_point.ssid, records[index].ssid, ssid_length);
        access_point.rssi = records[index].rssi;
        access_point.channel = records[index].primary;
        access_point.secure = records[index].authmode != WIFI_AUTH_OPEN;
        if (callback(&access_point, context) != 0) {
            return OS_NET_ERROR;
        }
    }
    *found = count;
    return OS_NET_OK;
}

int os_net_disconnect(void)
{
    if (!net_initialized) {
        return OS_NET_NOT_INITIALIZED;
    }
    connection_pending = false;
    connection_retries = 0U;
    if ((net_state != OS_NET_STATE_CONNECTED) &&
        (net_state != OS_NET_STATE_CONNECTING)) {
        return OS_NET_OK;
    }
    xEventGroupClearBits(net_events, NET_DISCONNECTED_BIT);
    if (esp_wifi_disconnect() != ESP_OK) {
        return OS_NET_ERROR;
    }
    (void)xEventGroupWaitBits(net_events, NET_DISCONNECTED_BIT, pdTRUE,
                              pdFALSE, pdMS_TO_TICKS(2000U));
    net_state = OS_NET_STATE_DISCONNECTED;
    return OS_NET_OK;
}

int os_net_connect(const char *ssid, const char *password, uint32_t timeout_ms)
{
    wifi_config_t configuration = {0};
    size_t ssid_length;
    size_t password_length;
    EventBits_t bits;

    if (!net_initialized) {
        return OS_NET_NOT_INITIALIZED;
    }
    if ((ssid == NULL) || (password == NULL) || (timeout_ms == 0U)) {
        return OS_NET_INVALID_ARGUMENT;
    }
    ssid_length = strnlen(ssid, OS_NET_SSID_MAX_LENGTH + 1U);
    password_length = strnlen(password, OS_NET_PASSWORD_MAX_LENGTH + 1U);
    if ((ssid_length == 0U) || (ssid_length > OS_NET_SSID_MAX_LENGTH) ||
        (password_length > OS_NET_PASSWORD_MAX_LENGTH)) {
        return OS_NET_INVALID_ARGUMENT;
    }
    if (os_net_disconnect() != OS_NET_OK) {
        return OS_NET_ERROR;
    }
    memcpy(configuration.sta.ssid, ssid, ssid_length);
    memcpy(configuration.sta.password, password, password_length);
    configuration.sta.threshold.authmode = WIFI_AUTH_OPEN;
    configuration.sta.pmf_cfg.capable = true;
    configuration.sta.pmf_cfg.required = false;
    if (esp_wifi_set_config(WIFI_IF_STA, &configuration) != ESP_OK) {
        return OS_NET_ERROR;
    }
    memcpy(current_ssid, ssid, ssid_length);
    current_ssid[ssid_length] = '\0';
    connection_retries = 0U;
    connection_pending = true;
    net_state = OS_NET_STATE_CONNECTING;
    xEventGroupClearBits(net_events,
                         NET_CONNECTED_BIT | NET_FAILED_BIT |
                             NET_DISCONNECTED_BIT);
    if (esp_wifi_connect() != ESP_OK) {
        connection_pending = false;
        net_state = OS_NET_STATE_DISCONNECTED;
        return OS_NET_ERROR;
    }
    bits = xEventGroupWaitBits(net_events,
                               NET_CONNECTED_BIT | NET_FAILED_BIT,
                               pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    if ((bits & NET_CONNECTED_BIT) != 0U) {
        return OS_NET_OK;
    }
    connection_pending = false;
    (void)esp_wifi_disconnect();
    net_state = OS_NET_STATE_DISCONNECTED;
    return ((bits & NET_FAILED_BIT) != 0U) ? OS_NET_ERROR : OS_NET_TIMEOUT;
}

int os_net_connect_saved(uint32_t timeout_ms)
{
    char ssid[OS_NET_SSID_MAX_LENGTH + 1U];
    char password[OS_NET_PASSWORD_MAX_LENGTH + 1U] = {0};
    int result;

    result = os_config_get("wifi.ssid", ssid, sizeof(ssid));
    if (result == OS_CONFIG_NOT_FOUND) {
        return OS_NET_NOT_CONFIGURED;
    }
    if (result != OS_CONFIG_OK) {
        return OS_NET_ERROR;
    }
    result = os_config_get("wifi.password", password, sizeof(password));
    if ((result != OS_CONFIG_OK) && (result != OS_CONFIG_NOT_FOUND)) {
        return OS_NET_ERROR;
    }
    return os_net_connect(ssid, password, timeout_ms);
}

void os_net_get_info(os_net_info_t *info)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_dns_info_t dns_info;
    wifi_ap_record_t access_point;

    if (info == NULL) {
        return;
    }
    memset(info, 0, sizeof(*info));
    info->state = net_state;
    memcpy(info->ssid, current_ssid, strlen(current_ssid) + 1U);
    if (!net_initialized || (net_state != OS_NET_STATE_CONNECTED)) {
        return;
    }
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ipv4_to_string(&ip_info.ip, info->ip, sizeof(info->ip));
        ipv4_to_string(&ip_info.netmask, info->netmask,
                       sizeof(info->netmask));
        ipv4_to_string(&ip_info.gw, info->gateway, sizeof(info->gateway));
    }
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info) ==
        ESP_OK) {
        ipv4_to_string(&dns_info.ip.u_addr.ip4, info->dns,
                       sizeof(info->dns));
    }
    if (esp_wifi_sta_get_ap_info(&access_point) == ESP_OK) {
        info->rssi = access_point.rssi;
    }
}

static void ping_reply(esp_ping_handle_t handle, void *argument, int timeout)
{
    ping_context_t *context = (ping_context_t *)argument;
    os_net_ping_reply_t reply = {.timeout = timeout};

    (void)esp_ping_get_profile(handle, ESP_PING_PROF_SEQNO,
                               &reply.sequence, sizeof(reply.sequence));
    if (!timeout) {
        (void)esp_ping_get_profile(handle, ESP_PING_PROF_TTL,
                                   &reply.ttl, sizeof(reply.ttl));
        (void)esp_ping_get_profile(handle, ESP_PING_PROF_TIMEGAP,
                                   &reply.time_ms, sizeof(reply.time_ms));
    }
    if (context->callback != NULL) {
        context->callback(&reply, context->callback_context);
    }
}

static void ping_success(esp_ping_handle_t handle, void *argument)
{
    ping_reply(handle, argument, 0);
}

static void ping_timeout(esp_ping_handle_t handle, void *argument)
{
    ping_reply(handle, argument, 1);
}

static void ping_end(esp_ping_handle_t handle, void *argument)
{
    ping_context_t *context = (ping_context_t *)argument;

    (void)esp_ping_get_profile(handle, ESP_PING_PROF_REQUEST,
                               &context->summary->transmitted,
                               sizeof(context->summary->transmitted));
    (void)esp_ping_get_profile(handle, ESP_PING_PROF_REPLY,
                               &context->summary->received,
                               sizeof(context->summary->received));
    (void)esp_ping_get_profile(handle, ESP_PING_PROF_DURATION,
                               &context->summary->duration_ms,
                               sizeof(context->summary->duration_ms));
    xSemaphoreGive(context->complete);
}

int os_net_ping(const char *host, uint32_t count,
                os_net_ping_callback_t callback, void *callback_context,
                os_net_ping_summary_t *summary)
{
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_RAW};
    struct addrinfo *address_info = NULL;
    esp_ping_config_t configuration = ESP_PING_DEFAULT_CONFIG();
    esp_ping_callbacks_t callbacks = {0};
    esp_ping_handle_t handle = NULL;
    ping_context_t context = {0};
    struct sockaddr_in *address;
    TickType_t wait_ticks;

    if (!net_initialized) {
        return OS_NET_NOT_INITIALIZED;
    }
    if (net_state != OS_NET_STATE_CONNECTED) {
        return OS_NET_NOT_CONNECTED;
    }
    if ((host == NULL) || (host[0] == '\0') || (count == 0U) ||
        (count > 10U) || (summary == NULL)) {
        return OS_NET_INVALID_ARGUMENT;
    }
    if (getaddrinfo(host, NULL, &hints, &address_info) != 0) {
        return OS_NET_ERROR;
    }
    address = (struct sockaddr_in *)address_info->ai_addr;
    ip_addr_set_ip4_u32(&configuration.target_addr, address->sin_addr.s_addr);
    freeaddrinfo(address_info);

    memset(summary, 0, sizeof(*summary));
    context.complete = xSemaphoreCreateBinaryStatic(&context.semaphore_storage);
    context.callback = callback;
    context.callback_context = callback_context;
    context.summary = summary;
    configuration.count = count;
    configuration.interval_ms = NET_PING_INTERVAL_MS;
    configuration.timeout_ms = NET_PING_TIMEOUT_MS;
    callbacks.cb_args = &context;
    callbacks.on_ping_success = ping_success;
    callbacks.on_ping_timeout = ping_timeout;
    callbacks.on_ping_end = ping_end;
    if ((context.complete == NULL) ||
        (esp_ping_new_session(&configuration, &callbacks, &handle) != ESP_OK) ||
        (esp_ping_start(handle) != ESP_OK)) {
        if (handle != NULL) {
            (void)esp_ping_delete_session(handle);
        }
        return OS_NET_ERROR;
    }
    wait_ticks = pdMS_TO_TICKS(count *
                               (NET_PING_INTERVAL_MS + NET_PING_TIMEOUT_MS) +
                               2000U);
    if (xSemaphoreTake(context.complete, wait_ticks) != pdTRUE) {
        (void)esp_ping_stop(handle);
        (void)esp_ping_delete_session(handle);
        return OS_NET_TIMEOUT;
    }
    (void)esp_ping_delete_session(handle);
    return OS_NET_OK;
}
