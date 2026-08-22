#pragma once

typedef enum {
    MINIOS_KEY_NONE = 0,
    MINIOS_KEY_CHARACTER,
    MINIOS_KEY_ESCAPE,
    MINIOS_KEY_LEFT,
    MINIOS_KEY_RIGHT,
    MINIOS_KEY_UP,
    MINIOS_KEY_DOWN,
    MINIOS_KEY_HOME,
    MINIOS_KEY_END,
    MINIOS_KEY_DELETE,
} minios_key_type_t;

typedef struct {
    minios_key_type_t type;
    unsigned char character;
} minios_key_t;

typedef struct {
    unsigned char state;
    unsigned int parameter;
} minios_terminal_decoder_t;

void minios_terminal_decoder_reset(minios_terminal_decoder_t *decoder);
minios_key_t minios_terminal_decode(minios_terminal_decoder_t *decoder,
                                    unsigned char byte);
