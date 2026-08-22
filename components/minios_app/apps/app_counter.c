#include "app_internal.h"

#include <stdio.h>
#include <stdlib.h>

static int parse_unsigned(const char *value, unsigned long minimum,
                          unsigned long maximum, unsigned long *parsed)
{
    char *end;
    unsigned long result;

    if ((value == NULL) || (parsed == NULL) || (value[0] == '\0')) {
        return -1;
    }
    result = strtoul(value, &end, 0);
    if ((*end != '\0') || (result < minimum) || (result > maximum)) {
        return -1;
    }
    *parsed = result;
    return 0;
}

static int counter_main(int argc, char **argv)
{
    unsigned long count = 10U;
    unsigned long delay_ms = 1000U;
    unsigned long index;

    if ((argc > 2) ||
        ((argc >= 1) && (parse_unsigned(argv[0], 1U, 100U, &count) != 0)) ||
        ((argc == 2) &&
         (parse_unsigned(argv[1], 100U, 60000U, &delay_ms) != 0))) {
        os_print(
            "Usage: run counter [count:1-100] [delay_ms:100-60000]\r\n");
        return 2;
    }

    for (index = 1U; index <= count; ++index) {
        char line[48];

        if (os_app_should_stop()) {
            os_print("counter: stopped\r\n");
            return 130;
        }
        (void)snprintf(line, sizeof(line), "counter: %lu/%lu\r\n", index,
                       count);
        os_print(line);
        if (index < count) {
            unsigned long remaining = delay_ms;

            while (remaining > 0U) {
                uint32_t interval = (remaining > 100U) ? 100U
                                                       : (uint32_t)remaining;

                os_sleep(interval);
                remaining -= interval;
                if (os_app_should_stop()) {
                    os_print("counter: stopped\r\n");
                    return 130;
                }
            }
        }
    }
    return 0;
}

const os_app_descriptor_t *minios_app_counter_descriptor(void)
{
    static const os_app_descriptor_t descriptor = {
        .name = "counter",
        .description = "Cooperative background counter",
        .main = counter_main,
    };

    return &descriptor;
}
