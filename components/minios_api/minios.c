#include "minios.h"

#include <stdio.h>

#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

void os_print(const char *text)
{
    if (text != NULL) {
        fputs(text, stdout);
    }
}

uint32_t os_uptime_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

void minios_sleep(uint32_t milliseconds)
{
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

size_t os_free_memory(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

void os_get_memory_info(os_memory_info_t *info)
{
    if (info == NULL) {
        return;
    }

    info->total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    info->free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    info->minimum_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
}

void os_get_system_info(os_system_info_t *info)
{
    esp_chip_info_t chip_info;

    if (info == NULL) {
        return;
    }

    esp_chip_info(&chip_info);
    info->target = CONFIG_IDF_TARGET;
    info->cpu_cores = chip_info.cores;
}

void os_reboot(void)
{
    esp_restart();
}
