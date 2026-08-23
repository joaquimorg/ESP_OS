#pragma once

#include <stddef.h>
#include <stdint.h>

#define MINIOS_ELF_OK 0
#define MINIOS_ELF_ERROR -1
#define MINIOS_ELF_INVALID_ARGUMENT -2
#define MINIOS_ELF_NOT_FOUND -3
#define MINIOS_ELF_INVALID_PATH -4
#define MINIOS_ELF_TOO_LARGE -5
#define MINIOS_ELF_INVALID_FORMAT -6
#define MINIOS_ELF_UNSUPPORTED -7
#define MINIOS_ELF_UNKNOWN_SYMBOL -8
#define MINIOS_ELF_NO_MEMORY -9
#define MINIOS_ELF_PROCESS_LIMIT -10

#define MINIOS_ELF_FILE_MAX (32U * 1024U)
#define MINIOS_ELF_IMAGE_MAX (32U * 1024U)

typedef struct {
    char name[16];
    size_t file_size;
    size_t image_size;
    size_t imports;
} minios_elf_info_t;

int minios_elf_inspect(const char *path, minios_elf_info_t *info);
int minios_elf_run(const char *path, int argc, char **argv, uint16_t *pid);
const char *minios_elf_error_string(int result);
