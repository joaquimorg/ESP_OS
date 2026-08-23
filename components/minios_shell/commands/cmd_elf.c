#include "shell_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minios.h"
#include "minios_elf.h"
#include "minios_fs.h"

static int discard_file_data(const char *data, size_t length, void *context)
{
    (void)data;
    (void)length;
    (void)context;
    return 0;
}

static int hex_value(unsigned char character)
{
    if ((character >= '0') && (character <= '9')) {
        return character - '0';
    }
    character = (unsigned char)tolower(character);
    return ((character >= 'a') && (character <= 'f'))
               ? character - 'a' + 10
               : -1;
}

static int elf_name_is_valid(const char *name)
{
    size_t length;
    size_t index;

    if (name == NULL) {
        return 0;
    }
    length = strnlen(name, OS_APP_NAME_MAX + 1U);
    if ((length == 0U) || (length > OS_APP_NAME_MAX)) {
        return 0;
    }
    for (index = 0U; index < length; ++index) {
        unsigned char character = (unsigned char)name[index];

        if ((isalnum(character) == 0) && (character != '_') &&
            (character != '-')) {
            return 0;
        }
    }
    return 1;
}

static int receive_elf(const char *name)
{
    unsigned char *data;
    size_t used = 0U;
    int high_nibble = -1;
    char path[OS_APP_NAME_MAX + 10U];
    minios_elf_info_t info;
    int result;

    if (!elf_name_is_valid(name)) {
        minios_shell_write(
            "elf: name must contain 1-15 letters, digits, '_' or '-'\r\n");
        return -1;
    }
    (void)snprintf(path, sizeof(path), "/bin/%s.elf", name);
    result = os_fs_read(path, discard_file_data, NULL);
    if (result == OS_FS_OK) {
        minios_shell_printf("elf: %s already exists; remove it first\r\n",
                            path);
        return -1;
    }
    if (result != OS_FS_NOT_FOUND) {
        minios_shell_printf("elf: cannot access %s\r\n", path);
        return -1;
    }
    data = malloc(MINIOS_ELF_FILE_MAX);
    if (data == NULL) {
        minios_shell_write("elf: not enough memory for receive buffer\r\n");
        return -1;
    }
    minios_shell_printf(
        "Receiving hexadecimal ELF as %s\r\n"
        "Paste hex; Ctrl-D saves, Ctrl-C cancels.\r\n",
        path);

    for (;;) {
        char byte;
        int received = minios_shell_read_bytes(&byte, 1U);
        int nibble;

        if (received < 0) {
            free(data);
            return -1;
        }
        if (received == 0) {
            os_sleep(10U);
            continue;
        }
        if ((unsigned char)byte == 0x03U) {
            free(data);
            minios_shell_write("\r\nReceive cancelled\r\n");
            return -1;
        }
        if ((unsigned char)byte == 0x04U) {
            break;
        }
        if (isspace((unsigned char)byte) != 0) {
            continue;
        }
        nibble = hex_value((unsigned char)byte);
        if (nibble < 0) {
            free(data);
            minios_shell_write("\r\nelf: invalid hexadecimal input\r\n");
            return -1;
        }
        if (high_nibble < 0) {
            high_nibble = nibble;
            continue;
        }
        if (used >= MINIOS_ELF_FILE_MAX) {
            free(data);
            minios_shell_write("\r\nelf: file exceeds 32 KiB\r\n");
            return -1;
        }
        data[used++] = (unsigned char)((high_nibble << 4) | nibble);
        high_nibble = -1;
    }
    if ((high_nibble >= 0) || (used == 0U)) {
        free(data);
        minios_shell_write("\r\nelf: incomplete hexadecimal input\r\n");
        return -1;
    }
    result = minios_fs_replace(path, (const char *)data, used);
    free(data);
    if (result != OS_FS_OK) {
        (void)os_fs_remove(path);
        minios_shell_write("\r\nelf: cannot save received file\r\n");
        return -1;
    }
    result = minios_elf_inspect(path, &info);
    if (result != MINIOS_ELF_OK) {
        (void)os_fs_remove(path);
        minios_shell_printf("\r\nelf: rejected: %s\r\n",
                            minios_elf_error_string(result));
        return -1;
    }
    minios_shell_printf("\r\nReceived %u bytes; %s is ready\r\n",
                        (unsigned int)used, path);
    return 0;
}

static int cmd_elf(int argc, char **argv)
{
    minios_elf_info_t info;
    int result;

    if ((argc == 3) && (strcmp(argv[1], "receive") == 0)) {
        return receive_elf(argv[2]);
    }
    if ((argc != 3) || (strcmp(argv[1], "info") != 0)) {
        minios_shell_write(
            "Usage: elf <info /bin/application.elf|receive name>\r\n");
        return -1;
    }
    result = minios_elf_inspect(argv[2], &info);
    if (result != MINIOS_ELF_OK) {
        minios_shell_printf("elf: %s: %s\r\n", argv[2],
                            minios_elf_error_string(result));
        return -1;
    }
    minios_shell_printf("Name:       %s\r\n", info.name);
    minios_shell_printf("File size:  %u bytes\r\n",
                        (unsigned int)info.file_size);
    minios_shell_printf("Image size: %u bytes\r\n",
                        (unsigned int)info.image_size);
    minios_shell_printf("Imports:    %u\r\n", (unsigned int)info.imports);
    minios_shell_printf("API:        %u\r\n", MINIOS_API_VERSION);
    return 0;
}

static const minios_command_t elf_command = {
    .name = "elf",
    .description = "Receive or inspect an ELF application",
    .usage = "elf <info /bin/application.elf|receive name>",
    .handler = cmd_elf,
};

int minios_cmd_elf_register(void)
{
    return minios_shell_register(&elf_command);
}
