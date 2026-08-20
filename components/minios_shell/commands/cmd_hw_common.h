#pragma once

#include <stdint.h>

int minios_cmd_parse_int(const char *text, int minimum, int maximum,
                         int *value);
int minios_cmd_parse_u32(const char *text, uint32_t minimum,
                         uint32_t maximum, uint32_t *value);
int minios_cmd_hal_report_error(const char *operation, int result);
