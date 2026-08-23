#include "minios.h"

int minios_app_main(int argc, char **argv)
{
    int index;

    os_print("Hello from /bin/hello_elf.elf\r\n");
    for (index = 0; index < argc; ++index) {
        os_print(argv[index]);
        os_print("\r\n");
    }
    return 0;
}
