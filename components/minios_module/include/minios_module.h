#pragma once

#include <stddef.h>

#define MINIOS_MODULE_OK 0
#define MINIOS_MODULE_ERROR -1
#define MINIOS_MODULE_INVALID_ARGUMENT -2
#define MINIOS_MODULE_NOT_FOUND -3
#define MINIOS_MODULE_ALREADY_LOADED -4
#define MINIOS_MODULE_NOT_LOADED -5
#define MINIOS_MODULE_DEPENDENCY -6
#define MINIOS_MODULE_BUSY -7

typedef struct {
    const char *name;
    const char *description;
    const char *device;
    int loaded;
} minios_module_info_t;

typedef int (*minios_module_load_handler_t)(int argc, char **argv);
typedef int (*minios_module_unload_handler_t)(void);

typedef struct {
    minios_module_info_t info;
    minios_module_load_handler_t load;
    minios_module_unload_handler_t unload;
} minios_module_descriptor_t;

int minios_module_init(void);
int minios_module_register(const minios_module_descriptor_t *descriptor);
size_t minios_module_count(void);
const minios_module_info_t *minios_module_at(size_t index);
const minios_module_info_t *minios_module_find(const char *name);
int minios_module_load(const char *name, int argc, char **argv);
int minios_module_unload(const char *name);
