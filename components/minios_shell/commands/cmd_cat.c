#include "shell_internal.h"

#include <stddef.h>

#include "cmd_fs_common.h"
#include "minios_fs.h"

typedef struct {
    int pending_cr;
    int line_open;
} cat_output_t;

static int write_file_data(const char *data, size_t length, void *context)
{
    cat_output_t *output = (cat_output_t *)context;
    char terminal_data[64];
    size_t input_index;
    size_t output_length = 0U;

    for (input_index = 0U; input_index < length; ++input_index) {
        char character = data[input_index];

        if (output_length > (sizeof(terminal_data) - 3U)) {
            if (minios_shell_write_bytes(terminal_data, output_length) !=
                (int)output_length) {
                return -1;
            }
            output_length = 0U;
        }
        if (output->pending_cr) {
            terminal_data[output_length++] = '\r';
            terminal_data[output_length++] = '\n';
            output->pending_cr = 0;
            output->line_open = 0;
            if (character == '\n') {
                continue;
            }
        }
        if (character == '\r') {
            output->pending_cr = 1;
        } else if (character == '\n') {
            terminal_data[output_length++] = '\r';
            terminal_data[output_length++] = '\n';
            output->line_open = 0;
        } else {
            terminal_data[output_length++] = character;
            output->line_open = 1;
        }
    }
    return ((output_length == 0U) ||
            (minios_shell_write_bytes(terminal_data, output_length) ==
             (int)output_length))
               ? 0
               : -1;
}

static int cmd_cat(int argc, char **argv)
{
    cat_output_t output = {0};
    int result;

    if (argc != 2) {
        minios_shell_write("Usage: cat <file>\r\n");
        return -1;
    }
    result = os_fs_read(argv[1], write_file_data, &output);
    if (result != OS_FS_OK) {
        return minios_cmd_fs_report_error("cat", argv[1], result);
    }
    if (output.pending_cr || output.line_open) {
        minios_shell_write("\r\n");
    }
    return 0;
}

static const minios_command_t cat_command = {
    .name = "cat",
    .description = "Display a file",
    .usage = "cat <file>",
    .handler = cmd_cat,
};

int minios_cmd_cat_register(void)
{
    return minios_shell_register(&cat_command);
}
