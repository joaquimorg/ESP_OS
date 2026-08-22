#include "minios_app.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "app_internal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define APP_TASK_STACK_SIZE 3072U
#define APP_TASK_PRIORITY (tskIDLE_PRIORITY + 1U)

typedef struct {
    StaticTask_t task_storage;
    StackType_t stack[APP_TASK_STACK_SIZE];
    TaskHandle_t task;
    const os_app_descriptor_t *application;
    uint16_t pid;
    volatile int stop_requested;
    os_process_state_t state;
    int exit_code;
    uint32_t started_ms;
    uint32_t finished_ms;
    int argc;
    char arguments[OS_APP_MAX_ARGS][OS_APP_ARG_MAX + 1U];
    char *argv[OS_APP_MAX_ARGS];
} app_process_slot_t;

static os_app_descriptor_t app_registry[OS_APP_MAX];
static size_t app_count;
static app_process_slot_t process_slots[OS_APP_MAX_PROCESSES];
static StaticSemaphore_t app_mutex_storage;
static SemaphoreHandle_t app_mutex;
static uint16_t next_pid = 1U;
static int app_initialized;

static uint16_t allocate_pid(const app_process_slot_t *reused_slot)
{
    uint32_t attempts;

    for (attempts = 0U; attempts < UINT16_MAX; ++attempts) {
        uint16_t candidate = next_pid++;
        size_t index;
        int used = 0;

        if (next_pid == 0U) {
            next_pid = 1U;
        }
        for (index = 0U; index < OS_APP_MAX_PROCESSES; ++index) {
            if ((&process_slots[index] != reused_slot) &&
                (process_slots[index].pid == candidate)) {
                used = 1;
                break;
            }
        }
        if (!used) {
            return candidate;
        }
    }
    return 0U;
}

static int app_name_is_valid(const char *name)
{
    size_t length;
    size_t index;

    if (name == NULL) {
        return 0;
    }
    length = strnlen(name, OS_APP_NAME_MAX + 1U);
    if ((length == 0U) || (length > OS_APP_NAME_MAX)) {
        return 0;
    }
    for (index = 0U; index < length; ++index) {
        unsigned char character = (unsigned char)name[index];

        if ((isalnum(character) == 0) && (character != '_') &&
            (character != '-')) {
            return 0;
        }
    }
    return 1;
}

static void app_worker(void *context)
{
    app_process_slot_t *slot = (app_process_slot_t *)context;

    for (;;) {
        const os_app_descriptor_t *application;
        int argc;
        char **argv;
        int result;

        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (xSemaphoreTake(app_mutex, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        application = slot->application;
        argc = slot->argc;
        argv = slot->argv;
        slot->state = slot->stop_requested ? OS_PROCESS_STOPPING
                                           : OS_PROCESS_RUNNING;
        xSemaphoreGive(app_mutex);

        result = application->main(argc, argv);

        if (xSemaphoreTake(app_mutex, portMAX_DELAY) == pdTRUE) {
            slot->exit_code = result;
            slot->finished_ms = os_uptime_ms();
            slot->state = OS_PROCESS_EXITED;
            xSemaphoreGive(app_mutex);
        }
    }
}

int os_app_register(const os_app_descriptor_t *descriptor)
{
    size_t index;

    if (!app_initialized || (descriptor == NULL) ||
        !app_name_is_valid(descriptor->name) ||
        (descriptor->description == NULL) || (descriptor->main == NULL)) {
        return OS_APP_INVALID_ARGUMENT;
    }
    if (app_count >= OS_APP_MAX) {
        return OS_APP_REGISTRY_FULL;
    }
    for (index = 0U; index < app_count; ++index) {
        if (strcmp(app_registry[index].name, descriptor->name) == 0) {
            return OS_APP_ALREADY_EXISTS;
        }
    }
    app_registry[app_count++] = *descriptor;
    return OS_APP_OK;
}

int os_app_init(void)
{
    size_t index;

    if (app_initialized) {
        return OS_APP_OK;
    }
    memset(app_registry, 0, sizeof(app_registry));
    memset(process_slots, 0, sizeof(process_slots));
    app_count = 0U;
    next_pid = 1U;
    app_mutex = xSemaphoreCreateMutexStatic(&app_mutex_storage);
    if (app_mutex == NULL) {
        return OS_APP_ERROR;
    }
    app_initialized = 1;

    for (index = 0U; index < OS_APP_MAX_PROCESSES; ++index) {
        char task_name[configMAX_TASK_NAME_LEN];

        (void)snprintf(task_name, sizeof(task_name), "minios_app%u",
                       (unsigned int)index);
        process_slots[index].task = xTaskCreateStatic(
            app_worker, task_name, APP_TASK_STACK_SIZE, &process_slots[index],
            APP_TASK_PRIORITY, process_slots[index].stack,
            &process_slots[index].task_storage);
        if (process_slots[index].task == NULL) {
            app_initialized = 0;
            return OS_APP_ERROR;
        }
    }

    if ((os_app_register(minios_app_hello_descriptor()) != OS_APP_OK) ||
        (os_app_register(minios_app_counter_descriptor()) != OS_APP_OK) ||
        (os_app_register(minios_app_welcome_descriptor()) != OS_APP_OK)) {
        return OS_APP_ERROR;
    }
    return OS_APP_OK;
}

size_t os_app_count(void)
{
    return app_initialized ? app_count : 0U;
}

const os_app_descriptor_t *os_app_at(size_t index)
{
    if (!app_initialized || (index >= app_count)) {
        return NULL;
    }
    return &app_registry[index];
}

const os_app_descriptor_t *os_app_find(const char *name)
{
    size_t index;

    if (!app_initialized || (name == NULL)) {
        return NULL;
    }
    for (index = 0U; index < app_count; ++index) {
        if (strcmp(app_registry[index].name, name) == 0) {
            return &app_registry[index];
        }
    }
    return NULL;
}

int os_app_run(const char *name, int argc, char **argv, uint16_t *pid)
{
    const os_app_descriptor_t *application = os_app_find(name);
    app_process_slot_t *slot = NULL;
    size_t index;

    if ((application == NULL) || (pid == NULL)) {
        return (application == NULL) ? OS_APP_NOT_FOUND
                                     : OS_APP_INVALID_ARGUMENT;
    }
    if ((argc < 0) || (argc > OS_APP_MAX_ARGS) ||
        ((argc > 0) && (argv == NULL))) {
        return OS_APP_INVALID_ARGUMENT;
    }
    for (index = 0U; index < (size_t)argc; ++index) {
        if ((argv[index] == NULL) ||
            (strnlen(argv[index], OS_APP_ARG_MAX + 1U) > OS_APP_ARG_MAX)) {
            return OS_APP_INVALID_ARGUMENT;
        }
    }
    if ((app_mutex == NULL) ||
        (xSemaphoreTake(app_mutex, portMAX_DELAY) != pdTRUE)) {
        return OS_APP_ERROR;
    }
    for (index = 0U; index < OS_APP_MAX_PROCESSES; ++index) {
        if ((process_slots[index].pid == 0U) ||
            (process_slots[index].state == OS_PROCESS_EXITED)) {
            slot = &process_slots[index];
            break;
        }
    }
    if (slot == NULL) {
        xSemaphoreGive(app_mutex);
        return OS_APP_PROCESS_LIMIT;
    }

    slot->application = application;
    slot->argc = argc;
    for (index = 0U; index < (size_t)argc; ++index) {
        size_t length = strlen(argv[index]);

        memcpy(slot->arguments[index], argv[index], length + 1U);
        slot->argv[index] = slot->arguments[index];
    }
    slot->stop_requested = 0;
    slot->exit_code = 0;
    slot->started_ms = os_uptime_ms();
    slot->finished_ms = 0U;
    slot->state = OS_PROCESS_STARTING;
    slot->pid = allocate_pid(slot);
    if (slot->pid == 0U) {
        xSemaphoreGive(app_mutex);
        return OS_APP_PROCESS_LIMIT;
    }
    *pid = slot->pid;
    xSemaphoreGive(app_mutex);
    xTaskNotifyGive(slot->task);
    return OS_APP_OK;
}

int os_app_kill(uint16_t pid)
{
    size_t index;

    if (!app_initialized || (pid == 0U) || (app_mutex == NULL)) {
        return OS_APP_INVALID_ARGUMENT;
    }
    if (xSemaphoreTake(app_mutex, portMAX_DELAY) != pdTRUE) {
        return OS_APP_ERROR;
    }
    for (index = 0U; index < OS_APP_MAX_PROCESSES; ++index) {
        app_process_slot_t *slot = &process_slots[index];

        if (slot->pid != pid) {
            continue;
        }
        if (slot->state == OS_PROCESS_EXITED) {
            xSemaphoreGive(app_mutex);
            return OS_APP_NOT_RUNNING;
        }
        slot->stop_requested = 1;
        slot->state = OS_PROCESS_STOPPING;
        xSemaphoreGive(app_mutex);
        return OS_APP_OK;
    }
    xSemaphoreGive(app_mutex);
    return OS_APP_NOT_FOUND;
}

int os_app_should_stop(void)
{
    TaskHandle_t current = xTaskGetCurrentTaskHandle();
    size_t index;

    for (index = 0U; index < OS_APP_MAX_PROCESSES; ++index) {
        if (process_slots[index].task == current) {
            return process_slots[index].stop_requested != 0;
        }
    }
    return 0;
}

size_t os_process_count(void)
{
    size_t count = 0U;
    size_t index;

    if (!app_initialized || (app_mutex == NULL) ||
        (xSemaphoreTake(app_mutex, portMAX_DELAY) != pdTRUE)) {
        return 0U;
    }
    for (index = 0U; index < OS_APP_MAX_PROCESSES; ++index) {
        if (process_slots[index].pid != 0U) {
            ++count;
        }
    }
    xSemaphoreGive(app_mutex);
    return count;
}

int os_process_at(size_t requested, os_process_info_t *info)
{
    size_t current = 0U;
    size_t index;

    if (!app_initialized || (info == NULL) || (app_mutex == NULL)) {
        return OS_APP_INVALID_ARGUMENT;
    }
    if (xSemaphoreTake(app_mutex, portMAX_DELAY) != pdTRUE) {
        return OS_APP_ERROR;
    }
    for (index = 0U; index < OS_APP_MAX_PROCESSES; ++index) {
        const app_process_slot_t *slot = &process_slots[index];

        if (slot->pid == 0U) {
            continue;
        }
        if (current++ != requested) {
            continue;
        }
        info->pid = slot->pid;
        (void)snprintf(info->name, sizeof(info->name), "%s",
                       slot->application->name);
        info->state = slot->state;
        info->exit_code = slot->exit_code;
        info->elapsed_ms = ((slot->state == OS_PROCESS_EXITED)
                                ? slot->finished_ms : os_uptime_ms()) -
                           slot->started_ms;
        xSemaphoreGive(app_mutex);
        return OS_APP_OK;
    }
    xSemaphoreGive(app_mutex);
    return OS_APP_NOT_FOUND;
}
