#include "cmd_hw_common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "minios_hal.h"
#include "shell_internal.h"

int minios_cmd_parse_int(const char *text, int minimum, int maximum, int *value)
{
    char *end;
    long parsed;

    if ((text == NULL) || (value == NULL)) {
        return -1;
    }
    errno = 0;
    parsed = strtol(text, &end, 10);
    if ((errno != 0) || (end == text) || (*end != '\0') ||
        (parsed < minimum) || (parsed > maximum)) {
        return -1;
    }
    *value = (int)parsed;
    return 0;
}

int minios_cmd_parse_u32(const char *text, uint32_t minimum,
                         uint32_t maximum, uint32_t *value)
{
    char *end;
    unsigned long parsed;

    if ((text == NULL) || (value == NULL)) {
        return -1;
    }
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if ((errno != 0) || (end == text) || (*end != '\0') ||
        (parsed > UINT32_MAX) || ((uint32_t)parsed < minimum) ||
        ((uint32_t)parsed > maximum)) {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

int minios_cmd_hal_report_error(const char *operation, int result)
{
    const char *reason;

    switch (result) {
    case MINIOS_HAL_INVALID_ARGUMENT:
        reason = "invalid argument or pin";
        break;
    case MINIOS_HAL_NOT_INITIALIZED:
        reason = "not configured";
        break;
    case MINIOS_HAL_BUSY:
        reason = "pin or controller busy";
        break;
    case MINIOS_HAL_TIMEOUT:
        reason = "bus timeout";
        break;
    default:
        reason = "hardware error";
        break;
    }
    minios_shell_printf("%s: %s\r\n", operation, reason);
    return -1;
}
