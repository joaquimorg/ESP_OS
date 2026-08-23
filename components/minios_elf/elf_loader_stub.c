#include "minios_elf.h"

int minios_elf_inspect(const char *path, minios_elf_info_t *info)
{
    (void)path;
    (void)info;
    return MINIOS_ELF_UNSUPPORTED;
}

int minios_elf_run(const char *path, int argc, char **argv, uint16_t *pid)
{
    (void)path;
    (void)argc;
    (void)argv;
    (void)pid;
    return MINIOS_ELF_UNSUPPORTED;
}

const char *minios_elf_error_string(int result)
{
    (void)result;
    return "ELF loader disabled";
}
