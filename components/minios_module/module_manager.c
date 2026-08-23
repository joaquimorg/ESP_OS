#include "minios_module.h"

#include <string.h>

#include "module_internal.h"
#include "sdkconfig.h"

#define MINIOS_MODULE_MAX 8U

static minios_module_descriptor_t module_registry[MINIOS_MODULE_MAX];
static size_t module_count;
static int module_initialized;

int minios_module_register(const minios_module_descriptor_t *descriptor)
{
    size_t index;

    if (!module_initialized || (descriptor == NULL) ||
        (descriptor->info.name == NULL) ||
        (descriptor->info.description == NULL) || (descriptor->load == NULL) ||
        (descriptor->unload == NULL) || (module_count >= MINIOS_MODULE_MAX)) {
        return MINIOS_MODULE_ERROR;
    }
    for (index = 0U; index < module_count; ++index) {
        if (strcmp(module_registry[index].info.name,
                   descriptor->info.name) == 0) {
            return MINIOS_MODULE_ERROR;
        }
    }
    module_registry[module_count] = *descriptor;
    module_registry[module_count].info.loaded = 0;
    ++module_count;
    return MINIOS_MODULE_OK;
}

int minios_module_init(void)
{
    module_count = 0U;
    module_initialized = 1;
#if CONFIG_MINIOS_ENABLE_SSD1315_MODULE
    if (minios_module_register(minios_module_ssd1315_descriptor()) !=
        MINIOS_MODULE_OK) {
        module_count = 0U;
        module_initialized = 0;
        return MINIOS_MODULE_ERROR;
    }
#endif
    return MINIOS_MODULE_OK;
}

size_t minios_module_count(void)
{
    return module_initialized ? module_count : 0U;
}

const minios_module_info_t *minios_module_at(size_t index)
{
    if (!module_initialized || (index >= module_count)) {
        return NULL;
    }
    return &module_registry[index].info;
}

static minios_module_descriptor_t *find_descriptor(const char *name)
{
    size_t index;

    if (!module_initialized || (name == NULL)) {
        return NULL;
    }
    for (index = 0U; index < module_count; ++index) {
        if (strcmp(module_registry[index].info.name, name) == 0) {
            return &module_registry[index];
        }
    }
    return NULL;
}

const minios_module_info_t *minios_module_find(const char *name)
{
    minios_module_descriptor_t *descriptor = find_descriptor(name);

    return (descriptor == NULL) ? NULL : &descriptor->info;
}

int minios_module_load(const char *name, int argc, char **argv)
{
    minios_module_descriptor_t *descriptor = find_descriptor(name);
    int result;

    if ((argc < 0) || ((argc > 0) && (argv == NULL))) {
        return MINIOS_MODULE_INVALID_ARGUMENT;
    }
    if (descriptor == NULL) {
        return MINIOS_MODULE_NOT_FOUND;
    }
    if (descriptor->info.loaded) {
        return MINIOS_MODULE_ALREADY_LOADED;
    }
    result = descriptor->load(argc, argv);
    if (result == MINIOS_MODULE_OK) {
        descriptor->info.loaded = 1;
    }
    return result;
}

int minios_module_unload(const char *name)
{
    minios_module_descriptor_t *descriptor = find_descriptor(name);
    int result;

    if (descriptor == NULL) {
        return MINIOS_MODULE_NOT_FOUND;
    }
    if (!descriptor->info.loaded) {
        return MINIOS_MODULE_NOT_LOADED;
    }
    result = descriptor->unload();
    if (result == MINIOS_MODULE_OK) {
        descriptor->info.loaded = 0;
    }
    return result;
}
