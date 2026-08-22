#include "shell_internal.h"

#include <stdio.h>
#include <string.h>

#include "cmd_fs_common.h"
#include "minios_fs.h"
#include "shell_terminal.h"

#define EDITOR_MAX_LINES 64U
#define EDITOR_MAX_COLUMNS 72U
#define EDITOR_VISIBLE_LINES 20U
#define EDITOR_SAVE_SIZE \
    (EDITOR_MAX_LINES * (EDITOR_MAX_COLUMNS + 1U))

typedef struct {
    char lines[EDITOR_MAX_LINES][EDITOR_MAX_COLUMNS + 1U];
    size_t lengths[EDITOR_MAX_LINES];
    size_t line_count;
    size_t row;
    size_t column;
    size_t top;
    int dirty;
    int load_failed;
    int quit_armed;
    char status[96];
} editor_t;

static editor_t editor;
static char save_buffer[EDITOR_SAVE_SIZE];

static int load_text(const char *data, size_t length, void *context)
{
    editor_t *state = (editor_t *)context;
    size_t index;

    for (index = 0U; index < length; ++index) {
        unsigned char character = (unsigned char)data[index];

        if (character == '\r') {
            continue;
        }
        if (character == '\n') {
            if (state->line_count >= EDITOR_MAX_LINES) {
                state->load_failed = 1;
                return -1;
            }
            ++state->line_count;
            continue;
        }
        if ((character < 0x20U) || (character > 0x7eU) ||
            (state->lengths[state->line_count - 1U] >=
             EDITOR_MAX_COLUMNS)) {
            state->load_failed = 1;
            return -1;
        }
        state->lines[state->line_count - 1U]
                    [state->lengths[state->line_count - 1U]++] =
            (char)character;
        state->lines[state->line_count - 1U]
                    [state->lengths[state->line_count - 1U]] = '\0';
    }
    return 0;
}

static void set_status(editor_t *state, const char *text)
{
    (void)snprintf(state->status, sizeof(state->status), "%s", text);
}

static void update_viewport(editor_t *state)
{
    if (state->row < state->top) {
        state->top = state->row;
    } else if (state->row >= (state->top + EDITOR_VISIBLE_LINES)) {
        state->top = state->row - EDITOR_VISIBLE_LINES + 1U;
    }
}

static void render_editor(const editor_t *state, const char *path)
{
    size_t screen_line;

    minios_shell_write("\x1b[?25l\x1b[H\x1b[2J");
    minios_shell_printf("MiniOS editor: %s%s\r\n", path,
                        state->dirty ? "  [modified]" : "");
    minios_shell_write(
        "Ctrl-S save | Ctrl-Q quit | arrows/Home/End/Delete\r\n");
    for (screen_line = 0U; screen_line < EDITOR_VISIBLE_LINES;
         ++screen_line) {
        size_t document_line = state->top + screen_line;

        minios_shell_write("\x1b[2K");
        if (document_line < state->line_count) {
            minios_shell_write_bytes(state->lines[document_line],
                                     state->lengths[document_line]);
        } else {
            minios_shell_write("~");
        }
        minios_shell_write("\r\n");
    }
    minios_shell_printf("\x1b[2K%s\x1b[%u;%uH\x1b[?25h", state->status,
                        (unsigned int)(3U + state->row - state->top),
                        (unsigned int)(1U + state->column));
}

static int save_editor(editor_t *state, const char *path)
{
    size_t used = 0U;
    size_t row;
    int result;

    for (row = 0U; row < state->line_count; ++row) {
        size_t length = state->lengths[row];

        if ((used + length + ((row + 1U < state->line_count) ? 1U : 0U)) >
            sizeof(save_buffer)) {
            set_status(state, "File is too large to save");
            return -1;
        }
        memcpy(save_buffer + used, state->lines[row], length);
        used += length;
        if (row + 1U < state->line_count) {
            save_buffer[used++] = '\n';
        }
    }
    result = minios_fs_replace(path, save_buffer, used);
    if (result != OS_FS_OK) {
        minios_cmd_fs_report_error("edit", path, result);
        set_status(state, "Save failed; press Ctrl-Q twice to discard");
        return -1;
    }
    state->dirty = 0;
    state->quit_armed = 0;
    set_status(state, "Saved");
    return 0;
}

static void mark_changed(editor_t *state)
{
    state->dirty = 1;
    state->quit_armed = 0;
    state->status[0] = '\0';
}

static void insert_character(editor_t *state, char character)
{
    char *line = state->lines[state->row];
    size_t *length = &state->lengths[state->row];

    if (*length >= EDITOR_MAX_COLUMNS) {
        set_status(state, "Line limit reached (72 characters)");
        return;
    }
    memmove(line + state->column + 1U, line + state->column,
            *length - state->column + 1U);
    line[state->column++] = character;
    ++*length;
    mark_changed(state);
}

static void insert_newline(editor_t *state)
{
    size_t tail;

    if (state->line_count >= EDITOR_MAX_LINES) {
        set_status(state, "File limit reached (64 lines)");
        return;
    }
    memmove(&state->lines[state->row + 2U],
            &state->lines[state->row + 1U],
            (state->line_count - state->row - 1U) *
                sizeof(state->lines[0]));
    memmove(&state->lengths[state->row + 2U],
            &state->lengths[state->row + 1U],
            (state->line_count - state->row - 1U) *
                sizeof(state->lengths[0]));
    tail = state->lengths[state->row] - state->column;
    memcpy(state->lines[state->row + 1U],
           state->lines[state->row] + state->column, tail);
    state->lines[state->row + 1U][tail] = '\0';
    state->lengths[state->row + 1U] = tail;
    state->lengths[state->row] = state->column;
    state->lines[state->row][state->column] = '\0';
    ++state->line_count;
    ++state->row;
    state->column = 0U;
    mark_changed(state);
}

static void remove_line(editor_t *state, size_t row)
{
    memmove(&state->lines[row], &state->lines[row + 1U],
            (state->line_count - row - 1U) * sizeof(state->lines[0]));
    memmove(&state->lengths[row], &state->lengths[row + 1U],
            (state->line_count - row - 1U) * sizeof(state->lengths[0]));
    --state->line_count;
    memset(state->lines[state->line_count], 0,
           sizeof(state->lines[state->line_count]));
    state->lengths[state->line_count] = 0U;
}

static void backspace(editor_t *state)
{
    if (state->column > 0U) {
        char *line = state->lines[state->row];
        size_t *length = &state->lengths[state->row];

        --state->column;
        memmove(line + state->column, line + state->column + 1U,
                *length - state->column);
        --*length;
        mark_changed(state);
    } else if (state->row > 0U) {
        size_t previous = state->row - 1U;
        size_t previous_length = state->lengths[previous];

        if ((previous_length + state->lengths[state->row]) >
            EDITOR_MAX_COLUMNS) {
            set_status(state, "Lines are too long to join");
            return;
        }
        memcpy(state->lines[previous] + previous_length,
               state->lines[state->row], state->lengths[state->row] + 1U);
        state->lengths[previous] += state->lengths[state->row];
        remove_line(state, state->row);
        state->row = previous;
        state->column = previous_length;
        mark_changed(state);
    }
}

static void delete_character(editor_t *state)
{
    char *line = state->lines[state->row];
    size_t *length = &state->lengths[state->row];

    if (state->column < *length) {
        memmove(line + state->column, line + state->column + 1U,
                *length - state->column);
        --*length;
        mark_changed(state);
    } else if (state->row + 1U < state->line_count) {
        size_t next_length = state->lengths[state->row + 1U];

        if ((*length + next_length) > EDITOR_MAX_COLUMNS) {
            set_status(state, "Lines are too long to join");
            return;
        }
        memcpy(line + *length, state->lines[state->row + 1U],
               next_length + 1U);
        *length += next_length;
        remove_line(state, state->row + 1U);
        mark_changed(state);
    }
}

static int handle_character(editor_t *state, unsigned char character,
                            const char *path)
{
    size_t spaces;

    switch (character) {
    case 0x11U:
        if (!state->dirty || state->quit_armed) {
            return 1;
        }
        state->quit_armed = 1;
        set_status(state, "Unsaved changes; Ctrl-Q again to discard");
        return 0;
    case 0x13U:
        (void)save_editor(state, path);
        return 0;
    case 0x01U:
        state->column = 0U;
        return 0;
    case 0x05U:
        state->column = state->lengths[state->row];
        return 0;
    case '\r':
    case '\n':
        insert_newline(state);
        return 0;
    case '\b':
    case 0x7fU:
        backspace(state);
        return 0;
    case '\t':
        spaces = 4U - (state->column % 4U);
        while (spaces-- > 0U) {
            insert_character(state, ' ');
        }
        return 0;
    default:
        if ((character >= 0x20U) && (character <= 0x7eU)) {
            insert_character(state, (char)character);
        }
        return 0;
    }
}

static int run_editor(const char *path)
{
    minios_terminal_decoder_t decoder;
    int ignore_lf = 0;
    int read_result;

    memset(&editor, 0, sizeof(editor));
    editor.line_count = 1U;
    read_result = os_fs_read(path, load_text, &editor);
    if ((read_result != OS_FS_OK) && (read_result != OS_FS_NOT_FOUND)) {
        if (editor.load_failed) {
            minios_shell_write(
                "edit: file exceeds 64 lines, 72 columns, or contains binary data\r\n");
            return -1;
        }
        return minios_cmd_fs_report_error("edit", path, read_result);
    }
    if (read_result == OS_FS_NOT_FOUND) {
        set_status(&editor, "New file");
    }
    minios_terminal_decoder_reset(&decoder);
    update_viewport(&editor);
    render_editor(&editor, path);

    for (;;) {
        char byte;
        minios_key_t key;
        int received;

        received = minios_shell_read_bytes(&byte, 1U);
        if (received < 0) {
            minios_shell_write("\x1b[?25h\x1b[2J\x1b[H");
            return -1;
        }
        if (received == 0) {
            os_sleep(10U);
            continue;
        }
        if (ignore_lf && ((byte == '\n') || (byte == '\0'))) {
            ignore_lf = 0;
            continue;
        }
        ignore_lf = (byte == '\r');
        key = minios_terminal_decode(&decoder, (unsigned char)byte);
        switch (key.type) {
        case MINIOS_KEY_CHARACTER:
            if (handle_character(&editor, key.character, path)) {
                minios_shell_write("\x1b[?25h\x1b[2J\x1b[H");
                return 0;
            }
            break;
        case MINIOS_KEY_LEFT:
            if (editor.column > 0U) {
                --editor.column;
            }
            break;
        case MINIOS_KEY_RIGHT:
            if (editor.column < editor.lengths[editor.row]) {
                ++editor.column;
            }
            break;
        case MINIOS_KEY_UP:
            if (editor.row > 0U) {
                --editor.row;
                if (editor.column > editor.lengths[editor.row]) {
                    editor.column = editor.lengths[editor.row];
                }
            }
            break;
        case MINIOS_KEY_DOWN:
            if (editor.row + 1U < editor.line_count) {
                ++editor.row;
                if (editor.column > editor.lengths[editor.row]) {
                    editor.column = editor.lengths[editor.row];
                }
            }
            break;
        case MINIOS_KEY_HOME:
            editor.column = 0U;
            break;
        case MINIOS_KEY_END:
            editor.column = editor.lengths[editor.row];
            break;
        case MINIOS_KEY_DELETE:
            delete_character(&editor);
            break;
        default:
            break;
        }
        if (key.type != MINIOS_KEY_NONE) {
            update_viewport(&editor);
            render_editor(&editor, path);
        }
    }
}

static int cmd_edit(int argc, char **argv)
{
    if (argc != 2) {
        minios_shell_write("Usage: edit <file>\r\n");
        return -1;
    }
    return run_editor(argv[1]);
}

static const minios_command_t edit_command = {
    .name = "edit",
    .description = "Edit a text or script file",
    .usage = "edit <file>",
    .handler = cmd_edit,
};

int minios_cmd_edit_register(void)
{
    return minios_shell_register(&edit_command);
}
