#pragma once

/*
 * Applications use the stable MiniOS API and never receive ESP-IDF or
 * FreeRTOS types.
 */
#include "minios.h"

typedef void (*minios_app_cleanup_t)(void *context);

int minios_app_run_external(const char *name, os_app_main_t main,
                            int argc, char **argv, uint16_t *pid,
                            minios_app_cleanup_t cleanup, void *context);
