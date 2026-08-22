#include "shell_internal.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minios_fs.h"

#define SCRIPT_MAX_SIZE 4096U
#define SCRIPT_MAX_LINES 96U
#define SCRIPT_MAX_DEPTH 3U
#define SCRIPT_MAX_VARIABLES 8U
#define SCRIPT_NAME_SIZE 17U
#define SCRIPT_VALUE_SIZE 64U
#define SCRIPT_MAX_REPEAT 100U
#define SCRIPT_MAX_STEPS 1000U
#define SCRIPT_MAX_SLEEP_MS 60000U
#define SCRIPT_MAX_BLOCK_DEPTH 8U

typedef struct {
    char name[SCRIPT_NAME_SIZE];
    char value[SCRIPT_VALUE_SIZE];
    int used;
} script_variable_t;

typedef struct {
    script_variable_t variables[SCRIPT_MAX_VARIABLES];
    unsigned int steps;
    int status;
    int exiting;
} script_context_t;

typedef struct {
    char data[SCRIPT_MAX_SIZE];
    uint16_t line_offsets[SCRIPT_MAX_LINES];
    size_t length;
    size_t line_count;
    int overflow;
    const char *path;
} script_frame_t;

static script_frame_t script_frames[SCRIPT_MAX_DEPTH];
static unsigned int script_depth;
static script_context_t *active_context;

static int load_script_data(const char *data, size_t length, void *context)
{
    script_frame_t *frame = (script_frame_t *)context;

    if (length > ((SCRIPT_MAX_SIZE - 1U) - frame->length)) {
        frame->overflow = 1;
        return -1;
    }
    memcpy(frame->data + frame->length, data, length);
    frame->length += length;
    return 0;
}

static int split_lines(script_frame_t *frame)
{
    size_t index = 0U;

    frame->data[frame->length] = '\0';
    frame->line_count = 0U;
    if (frame->length == 0U) {
        return 0;
    }
    frame->line_offsets[frame->line_count++] = 0U;
    while (index < frame->length) {
        if ((frame->data[index] == '\r') || (frame->data[index] == '\n')) {
            char separator = frame->data[index];

            frame->data[index++] = '\0';
            if ((separator == '\r') && (index < frame->length) &&
                (frame->data[index] == '\n')) {
                frame->data[index++] = '\0';
            }
            if (index < frame->length) {
                if (frame->line_count >= SCRIPT_MAX_LINES) {
                    return -1;
                }
                frame->line_offsets[frame->line_count++] = (uint16_t)index;
            }
        } else {
            ++index;
        }
    }
    return 0;
}

static char *trim_line(char *line)
{
    char *end;
    char *cursor;

    while (isspace((unsigned char)*line) != 0) {
        ++line;
    }
    end = line + strlen(line);
    while ((end > line) && (isspace((unsigned char)end[-1]) != 0)) {
        *--end = '\0';
    }
    for (cursor = line; *cursor != '\0'; ++cursor) {
        if ((*cursor == '#') &&
            ((cursor == line) || (isspace((unsigned char)cursor[-1]) != 0))) {
            *cursor = '\0';
            end = cursor;
            while ((end > line) && (isspace((unsigned char)end[-1]) != 0)) {
                *--end = '\0';
            }
            break;
        }
    }
    return line;
}

static int has_keyword(const char *line, const char *keyword)
{
    size_t length = strlen(keyword);

    return (strncmp(line, keyword, length) == 0) &&
           ((line[length] == '\0') ||
            (isspace((unsigned char)line[length]) != 0));
}

static script_variable_t *find_variable(script_context_t *context,
                                        const char *name)
{
    size_t index;

    for (index = 0U; index < SCRIPT_MAX_VARIABLES; ++index) {
        if (context->variables[index].used &&
            (strcmp(context->variables[index].name, name) == 0)) {
            return &context->variables[index];
        }
    }
    return NULL;
}

static int valid_variable_name(const char *name)
{
    const char *cursor = name;

    if ((isalpha((unsigned char)*cursor) == 0) && (*cursor != '_')) {
        return 0;
    }
    for (++cursor; *cursor != '\0'; ++cursor) {
        if ((isalnum((unsigned char)*cursor) == 0) && (*cursor != '_')) {
            return 0;
        }
    }
    return 1;
}

static int set_variable(script_context_t *context, const char *name,
                        const char *value)
{
    script_variable_t *variable = find_variable(context, name);
    size_t index;

    if (!valid_variable_name(name) || (strlen(name) >= SCRIPT_NAME_SIZE) ||
        (strlen(value) >= SCRIPT_VALUE_SIZE)) {
        return -1;
    }
    if (variable == NULL) {
        for (index = 0U; index < SCRIPT_MAX_VARIABLES; ++index) {
            if (!context->variables[index].used) {
                variable = &context->variables[index];
                variable->used = 1;
                strcpy(variable->name, name);
                break;
            }
        }
    }
    if (variable == NULL) {
        return -1;
    }
    strcpy(variable->value, value);
    return 0;
}

static const char *variable_value(script_context_t *context, const char *name)
{
    script_variable_t *variable = find_variable(context, name);

    return (variable == NULL) ? "" : variable->value;
}

static int append_text(char *output, size_t output_size, size_t *used,
                       const char *text)
{
    size_t length = strlen(text);

    if (length > ((output_size - 1U) - *used)) {
        return -1;
    }
    memcpy(output + *used, text, length);
    *used += length;
    output[*used] = '\0';
    return 0;
}

static int expand_line(script_context_t *context, const char *input,
                       char output[MINIOS_SHELL_MAX_LINE])
{
    size_t used = 0U;

    output[0] = '\0';
    while (*input != '\0') {
        if (*input != '$') {
            char character[2] = {*input++, '\0'};
            if (append_text(output, MINIOS_SHELL_MAX_LINE, &used,
                            character) != 0) {
                return -1;
            }
            continue;
        }
        ++input;
        if (*input == '$') {
            if (append_text(output, MINIOS_SHELL_MAX_LINE, &used, "$" ) != 0) {
                return -1;
            }
            ++input;
        } else if (*input == '?') {
            char status[16];
            int length;

            length = snprintf(status, sizeof(status), "%d", context->status);
            if ((length < 0) || (append_text(output, MINIOS_SHELL_MAX_LINE,
                                             &used, status) != 0)) {
                return -1;
            }
            ++input;
        } else {
            char name[SCRIPT_NAME_SIZE];
            size_t name_length = 0U;
            int braced = (*input == '{');

            if (braced) {
                ++input;
            }
            while (((isalnum((unsigned char)*input) != 0) ||
                    (*input == '_')) && (name_length < sizeof(name) - 1U)) {
                name[name_length++] = *input++;
            }
            if ((name_length == 0U) ||
                (isalnum((unsigned char)*input) != 0) || (*input == '_') ||
                (braced && (*input != '}'))) {
                return -1;
            }
            if (braced) {
                ++input;
            }
            name[name_length] = '\0';
            if (append_text(output, MINIOS_SHELL_MAX_LINE, &used,
                            variable_value(context, name)) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static int parse_unsigned(const char *text, unsigned long maximum,
                          unsigned long *value)
{
    char *end;
    unsigned long parsed;

    if ((text == NULL) || (*text == '\0') || (*text == '-')) {
        return -1;
    }
    parsed = strtoul(text, &end, 10);
    if ((*end != '\0') || (parsed > maximum)) {
        return -1;
    }
    *value = parsed;
    return 0;
}

static void report_line_error(const script_frame_t *frame, size_t line,
                              const char *message)
{
    minios_shell_printf("%s:%u: %s\r\n", frame->path,
                        (unsigned int)(line + 1U), message);
}

static int find_if_end(script_frame_t *frame, size_t start, size_t end,
                       size_t *else_line, size_t *end_line)
{
    unsigned int nesting = 0U;
    size_t index;

    *else_line = end;
    for (index = start + 1U; index < end; ++index) {
        char *line = trim_line(frame->data + frame->line_offsets[index]);

        if (has_keyword(line, "if")) {
            ++nesting;
        } else if (has_keyword(line, "endif")) {
            if (nesting == 0U) {
                *end_line = index;
                return 0;
            }
            --nesting;
        } else if (has_keyword(line, "else") && (nesting == 0U)) {
            if (*else_line != end) {
                return -1;
            }
            *else_line = index;
        }
    }
    return -1;
}

static int find_repeat_end(script_frame_t *frame, size_t start, size_t end,
                           size_t *end_line)
{
    unsigned int nesting = 0U;
    size_t index;

    for (index = start + 1U; index < end; ++index) {
        char *line = trim_line(frame->data + frame->line_offsets[index]);

        if (has_keyword(line, "repeat")) {
            ++nesting;
        } else if (has_keyword(line, "endrepeat")) {
            if (nesting == 0U) {
                *end_line = index;
                return 0;
            }
            --nesting;
        }
    }
    return -1;
}

static int validate_blocks(script_frame_t *frame)
{
    char blocks[SCRIPT_MAX_BLOCK_DEPTH];
    size_t depth = 0U;
    size_t index;

    for (index = 0U; index < frame->line_count; ++index) {
        char *line = trim_line(frame->data + frame->line_offsets[index]);

        if (has_keyword(line, "if")) {
            if (depth >= SCRIPT_MAX_BLOCK_DEPTH) {
                report_line_error(frame, index, "block nesting limit exceeded");
                return -1;
            }
            blocks[depth++] = 'i';
        } else if (has_keyword(line, "repeat")) {
            if (depth >= SCRIPT_MAX_BLOCK_DEPTH) {
                report_line_error(frame, index, "block nesting limit exceeded");
                return -1;
            }
            blocks[depth++] = 'r';
        } else if (has_keyword(line, "else")) {
            if ((depth == 0U) || (blocks[depth - 1U] != 'i')) {
                report_line_error(frame, index, "unexpected else");
                return -1;
            }
            blocks[depth - 1U] = 'e';
        } else if (has_keyword(line, "endif")) {
            if ((depth == 0U) || ((blocks[depth - 1U] != 'i') &&
                                  (blocks[depth - 1U] != 'e'))) {
                report_line_error(frame, index, "unexpected endif");
                return -1;
            }
            --depth;
        } else if (has_keyword(line, "endrepeat")) {
            if ((depth == 0U) || (blocks[depth - 1U] != 'r')) {
                report_line_error(frame, index, "unexpected endrepeat");
                return -1;
            }
            --depth;
        }
    }
    if (depth != 0U) {
        minios_shell_printf("script: %s: unterminated block\r\n", frame->path);
        return -1;
    }
    return 0;
}

static int execute_range(script_frame_t *frame, script_context_t *context,
                         size_t begin, size_t end)
{
    size_t line_index;

    for (line_index = begin; line_index < end; ++line_index) {
        char expanded[MINIOS_SHELL_MAX_LINE];
        char command_line[MINIOS_SHELL_MAX_LINE];
        char *argv[MINIOS_SHELL_MAX_ARGS];
        char *line = trim_line(frame->data + frame->line_offsets[line_index]);
        int argc;

        if (context->exiting) {
            return 0;
        }
        if ((*line == '\0') || (*line == '#')) {
            continue;
        }
        if (++context->steps > SCRIPT_MAX_STEPS) {
            report_line_error(frame, line_index, "instruction limit exceeded");
            return -1;
        }
        if (expand_line(context, line, expanded) != 0) {
            report_line_error(frame, line_index, "invalid or expanded line too long");
            return -1;
        }
        memcpy(command_line, expanded, sizeof(command_line));
        argc = minios_shell_parse(expanded, argv, MINIOS_SHELL_MAX_ARGS);
        if (argc <= 0) {
            continue;
        }

        if (strcmp(argv[0], "set") == 0) {
            if ((argc != 3) || (set_variable(context, argv[1], argv[2]) != 0)) {
                report_line_error(frame, line_index,
                                  "usage: set <name> <value> (or variable limit reached)");
                return -1;
            }
        } else if (strcmp(argv[0], "sleep") == 0) {
            unsigned long milliseconds;

            if ((argc != 2) ||
                (parse_unsigned(argv[1], SCRIPT_MAX_SLEEP_MS,
                                &milliseconds) != 0)) {
                report_line_error(frame, line_index,
                                  "sleep must be between 0 and 60000 ms");
                return -1;
            }
            os_sleep((uint32_t)milliseconds);
        } else if (strcmp(argv[0], "if") == 0) {
            size_t else_line;
            size_t endif_line;
            int condition;

            if (find_if_end(frame, line_index, end, &else_line,
                            &endif_line) != 0) {
                report_line_error(frame, line_index, "if without matching endif");
                return -1;
            }
            if (argc == 2) {
                condition = (argv[1][0] != '\0') &&
                            (strcmp(argv[1], "0") != 0) &&
                            (strcmp(argv[1], "false") != 0);
            } else if ((argc == 4) &&
                       ((strcmp(argv[2], "==") == 0) ||
                        (strcmp(argv[2], "!=") == 0))) {
                condition = (strcmp(argv[1], argv[3]) == 0);
                if (strcmp(argv[2], "!=") == 0) {
                    condition = !condition;
                }
            } else {
                report_line_error(frame, line_index,
                                  "usage: if <value> or if <left> ==|!= <right>");
                return -1;
            }
            if (condition) {
                if (execute_range(frame, context, line_index + 1U,
                                  (else_line < endif_line) ? else_line
                                                          : endif_line) != 0) {
                    return -1;
                }
            } else if (else_line < endif_line) {
                if (execute_range(frame, context, else_line + 1U,
                                  endif_line) != 0) {
                    return -1;
                }
            }
            line_index = endif_line;
        } else if (strcmp(argv[0], "repeat") == 0) {
            unsigned long count;
            unsigned long iteration;
            size_t endrepeat_line;

            if ((argc != 2) ||
                (parse_unsigned(argv[1], SCRIPT_MAX_REPEAT, &count) != 0)) {
                report_line_error(frame, line_index,
                                  "repeat count must be between 0 and 100");
                return -1;
            }
            if (find_repeat_end(frame, line_index, end,
                                &endrepeat_line) != 0) {
                report_line_error(frame, line_index,
                                  "repeat without matching endrepeat");
                return -1;
            }
            for (iteration = 0U; iteration < count; ++iteration) {
                if (execute_range(frame, context, line_index + 1U,
                                  endrepeat_line) != 0) {
                    return -1;
                }
                if (context->exiting) {
                    break;
                }
            }
            line_index = endrepeat_line;
        } else if (strcmp(argv[0], "exit") == 0) {
            unsigned long status = 0U;

            if ((argc > 2) || ((argc == 2) &&
                (parse_unsigned(argv[1], 255U, &status) != 0))) {
                report_line_error(frame, line_index,
                                  "exit status must be between 0 and 255");
                return -1;
            }
            context->status = (int)status;
            context->exiting = 1;
        } else if ((strcmp(argv[0], "else") == 0) ||
                   (strcmp(argv[0], "endif") == 0) ||
                   (strcmp(argv[0], "endrepeat") == 0)) {
            report_line_error(frame, line_index, "unexpected block terminator");
            return -1;
        } else {
            context->status = minios_shell_execute_line_locked(command_line);
        }
    }
    return 0;
}

int minios_script_execute(const char *path, int source)
{
    script_context_t local_context = {0};
    script_context_t *previous_context = active_context;
    script_context_t *context;
    script_frame_t *frame;
    int read_result;
    int execute_result;

    if ((path == NULL) || (script_depth >= SCRIPT_MAX_DEPTH)) {
        minios_shell_write("script: nesting limit exceeded\r\n");
        return MINIOS_SCRIPT_ERROR;
    }
    context = ((source > 0) && (active_context != NULL)) ? active_context
                                                        : &local_context;
    frame = &script_frames[script_depth++];
    memset(frame, 0, sizeof(*frame));
    frame->path = path;
    read_result = os_fs_read(path, load_script_data, frame);
    if (read_result != OS_FS_OK) {
        --script_depth;
        if (read_result == OS_FS_NOT_FOUND) {
            if (source >= 0) {
                minios_shell_printf("script: %s: not found\r\n", path);
            }
            return MINIOS_SCRIPT_NOT_FOUND;
        }
        minios_shell_printf("script: %s: %s\r\n", path,
                            frame->overflow ? "file exceeds 4095 bytes"
                                            : "cannot read file");
        return MINIOS_SCRIPT_ERROR;
    }
    if (split_lines(frame) != 0) {
        --script_depth;
        minios_shell_printf("script: %s: exceeds 96 lines\r\n", path);
        return MINIOS_SCRIPT_ERROR;
    }
    if (validate_blocks(frame) != 0) {
        --script_depth;
        return MINIOS_SCRIPT_ERROR;
    }

    active_context = context;
    execute_result = execute_range(frame, context, 0U, frame->line_count);
    active_context = previous_context;
    --script_depth;
    if ((execute_result != 0) || (context->status != 0)) {
        return MINIOS_SCRIPT_ERROR;
    }
    return MINIOS_SCRIPT_OK;
}
