#include "shell_terminal.h"

#include <stddef.h>

enum {
    TERMINAL_NORMAL = 0,
    TERMINAL_ESCAPE,
    TERMINAL_CSI,
    TERMINAL_SS3,
};

static minios_key_t key(minios_key_type_t type, unsigned char character)
{
    minios_key_t result = {.type = type, .character = character};

    return result;
}

void minios_terminal_decoder_reset(minios_terminal_decoder_t *decoder)
{
    if (decoder != NULL) {
        decoder->state = TERMINAL_NORMAL;
        decoder->parameter = 0U;
    }
}

minios_key_t minios_terminal_decode(minios_terminal_decoder_t *decoder,
                                    unsigned char byte)
{
    if (decoder == NULL) {
        return key(MINIOS_KEY_NONE, 0U);
    }
    if (decoder->state == TERMINAL_NORMAL) {
        if (byte == 0x1bU) {
            decoder->state = TERMINAL_ESCAPE;
            return key(MINIOS_KEY_NONE, 0U);
        }
        return key(MINIOS_KEY_CHARACTER, byte);
    }
    if (decoder->state == TERMINAL_ESCAPE) {
        decoder->parameter = 0U;
        if (byte == '[') {
            decoder->state = TERMINAL_CSI;
            return key(MINIOS_KEY_NONE, 0U);
        }
        if (byte == 'O') {
            decoder->state = TERMINAL_SS3;
            return key(MINIOS_KEY_NONE, 0U);
        }
        decoder->state = TERMINAL_NORMAL;
        return key(MINIOS_KEY_ESCAPE, byte);
    }
    if (decoder->state == TERMINAL_CSI) {
        if ((byte >= '0') && (byte <= '9')) {
            decoder->parameter = (decoder->parameter * 10U) +
                                 (unsigned int)(byte - '0');
            return key(MINIOS_KEY_NONE, 0U);
        }
        if ((byte == ';') || (byte == '?')) {
            return key(MINIOS_KEY_NONE, 0U);
        }
        decoder->state = TERMINAL_NORMAL;
        switch (byte) {
        case 'A':
            return key(MINIOS_KEY_UP, 0U);
        case 'B':
            return key(MINIOS_KEY_DOWN, 0U);
        case 'C':
            return key(MINIOS_KEY_RIGHT, 0U);
        case 'D':
            return key(MINIOS_KEY_LEFT, 0U);
        case 'H':
            return key(MINIOS_KEY_HOME, 0U);
        case 'F':
            return key(MINIOS_KEY_END, 0U);
        case '~':
            if (decoder->parameter == 3U) {
                return key(MINIOS_KEY_DELETE, 0U);
            }
            if ((decoder->parameter == 1U) ||
                (decoder->parameter == 7U)) {
                return key(MINIOS_KEY_HOME, 0U);
            }
            if ((decoder->parameter == 4U) ||
                (decoder->parameter == 8U)) {
                return key(MINIOS_KEY_END, 0U);
            }
            break;
        default:
            break;
        }
        return key(MINIOS_KEY_NONE, 0U);
    }

    decoder->state = TERMINAL_NORMAL;
    switch (byte) {
    case 'A':
        return key(MINIOS_KEY_UP, 0U);
    case 'B':
        return key(MINIOS_KEY_DOWN, 0U);
    case 'C':
        return key(MINIOS_KEY_RIGHT, 0U);
    case 'D':
        return key(MINIOS_KEY_LEFT, 0U);
    case 'H':
        return key(MINIOS_KEY_HOME, 0U);
    case 'F':
        return key(MINIOS_KEY_END, 0U);
    default:
        return key(MINIOS_KEY_NONE, 0U);
    }
}
