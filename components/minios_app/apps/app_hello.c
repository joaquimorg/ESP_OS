#include "app_internal.h"

#include <stdio.h>
#include <string.h>

static int hello_main(int argc, char **argv)
{
    char line[96];
    int index;

    os_print("Hello from a MiniOS application.\r\n");
    for (index = 0; index < argc; ++index) {
        int written = snprintf(line, sizeof(line), "argv[%d] = %s\r\n", index,
                               argv[index]);

        if (written > 0) {
            os_print(line);
        }
    }
    return 0;
}

const os_app_descriptor_t *minios_app_hello_descriptor(void)
{
    static const os_app_descriptor_t descriptor = {
        .name = "hello",
        .description = "Print a greeting and its arguments",
        .main = hello_main,
    };

    return &descriptor;
}
