#include "module_internal.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "minios_device.h"
#include "minios_hal.h"

#define SSD1315_WIDTH 128U
#define SSD1315_HEIGHT 64U
#define SSD1315_PAGES (SSD1315_HEIGHT / 8U)
#define SSD1315_BUFFER_SIZE (SSD1315_WIDTH * SSD1315_PAGES)
#define SSD1315_DEFAULT_ADDRESS 0x3CU
#define SSD1315_I2C_FREQUENCY 400000U
#define SSD1315_TIMEOUT_MS 100U

typedef struct {
    minios_hal_i2c_device_t *i2c_device;
    uint8_t framebuffer[SSD1315_BUFFER_SIZE];
    uint8_t cursor_x;
    uint8_t cursor_y;
} ssd1315_state_t;

static ssd1315_state_t display_state;

static void move_to_next_line(void)
{
    display_state.cursor_x = 0U;
    display_state.cursor_y =
        (display_state.cursor_y > (SSD1315_HEIGHT - 9U))
            ? 0U : (uint8_t)(display_state.cursor_y + 8U);
}

static int send_commands(const uint8_t *commands, size_t length)
{
    uint8_t transfer[32];

    if ((commands == NULL) || (length == 0U) ||
        (length > (sizeof(transfer) - 1U))) {
        return MINIOS_MODULE_INVALID_ARGUMENT;
    }
    transfer[0] = 0x00U;
    memcpy(transfer + 1U, commands, length);
    return (minios_hal_i2c_device_write(display_state.i2c_device, transfer,
                                        length + 1U, SSD1315_TIMEOUT_MS) ==
            MINIOS_HAL_OK)
               ? MINIOS_MODULE_OK
               : MINIOS_MODULE_ERROR;
}

static int refresh_display(void)
{
    static const uint8_t address_commands[] = {
        0x21U, 0x00U, 0x7fU, 0x22U, 0x00U, 0x07U,
    };
    uint8_t transfer[MINIOS_HAL_I2C_MAX_TRANSFER];
    size_t offset = 0U;

    if (send_commands(address_commands, sizeof(address_commands)) !=
        MINIOS_MODULE_OK) {
        return MINIOS_MODULE_ERROR;
    }
    transfer[0] = 0x40U;
    while (offset < sizeof(display_state.framebuffer)) {
        size_t count = sizeof(transfer) - 1U;

        if (count > (sizeof(display_state.framebuffer) - offset)) {
            count = sizeof(display_state.framebuffer) - offset;
        }
        memcpy(transfer + 1U, display_state.framebuffer + offset, count);
        if (minios_hal_i2c_device_write(display_state.i2c_device, transfer,
                                        count + 1U, SSD1315_TIMEOUT_MS) !=
            MINIOS_HAL_OK) {
            return MINIOS_MODULE_ERROR;
        }
        offset += count;
    }
    return MINIOS_MODULE_OK;
}

static void glyph_for_character(char character, uint8_t glyph[5])
{
    static const char characters[] =
        " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.-_:/?!";
    static const uint8_t glyphs[][5] = {
        {0x00,0x00,0x00,0x00,0x00}, {0x3e,0x51,0x49,0x45,0x3e},
        {0x00,0x42,0x7f,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46},
        {0x21,0x41,0x45,0x4b,0x31}, {0x18,0x14,0x12,0x7f,0x10},
        {0x27,0x45,0x45,0x45,0x39}, {0x3c,0x4a,0x49,0x49,0x30},
        {0x01,0x71,0x09,0x05,0x03}, {0x36,0x49,0x49,0x49,0x36},
        {0x06,0x49,0x49,0x29,0x1e}, {0x7e,0x11,0x11,0x11,0x7e},
        {0x7f,0x49,0x49,0x49,0x36}, {0x3e,0x41,0x41,0x41,0x22},
        {0x7f,0x41,0x41,0x22,0x1c}, {0x7f,0x49,0x49,0x49,0x41},
        {0x7f,0x09,0x09,0x09,0x01}, {0x3e,0x41,0x49,0x49,0x7a},
        {0x7f,0x08,0x08,0x08,0x7f}, {0x00,0x41,0x7f,0x41,0x00},
        {0x20,0x40,0x41,0x3f,0x01}, {0x7f,0x08,0x14,0x22,0x41},
        {0x7f,0x40,0x40,0x40,0x40}, {0x7f,0x02,0x0c,0x02,0x7f},
        {0x7f,0x04,0x08,0x10,0x7f}, {0x3e,0x41,0x41,0x41,0x3e},
        {0x7f,0x09,0x09,0x09,0x06}, {0x3e,0x41,0x51,0x21,0x5e},
        {0x7f,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
        {0x01,0x01,0x7f,0x01,0x01}, {0x3f,0x40,0x40,0x40,0x3f},
        {0x1f,0x20,0x40,0x20,0x1f}, {0x3f,0x40,0x38,0x40,0x3f},
        {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07},
        {0x61,0x51,0x49,0x45,0x43}, {0x00,0x60,0x60,0x00,0x00},
        {0x08,0x08,0x08,0x08,0x08}, {0x40,0x40,0x40,0x40,0x40},
        {0x00,0x36,0x36,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
        {0x02,0x01,0x51,0x09,0x06}, {0x00,0x00,0x5f,0x00,0x00},
    };
    const char *position;
    size_t index;

    character = (char)toupper((unsigned char)character);
    position = strchr(characters, character);
    if (position == NULL) {
        position = strchr(characters, '?');
    }
    index = (size_t)(position - characters);
    memcpy(glyph, glyphs[index], 5U);
}

static void draw_character(char character)
{
    uint8_t glyph[5];
    size_t column;
    size_t row;

    if (character == '\n') {
        move_to_next_line();
        return;
    }
    if ((display_state.cursor_x + 6U) > SSD1315_WIDTH) {
        move_to_next_line();
    }
    glyph_for_character(character, glyph);
    for (column = 0U; column < 6U; ++column) {
        uint8_t pixels = (column < 5U) ? glyph[column] : 0U;

        for (row = 0U; row < 7U; ++row) {
            size_t x = (size_t)display_state.cursor_x + column;
            size_t y = (size_t)display_state.cursor_y + row;
            size_t offset;
            uint8_t mask;

            if ((x >= SSD1315_WIDTH) || (y >= SSD1315_HEIGHT)) {
                continue;
            }
            offset = ((y / 8U) * SSD1315_WIDTH) + x;
            mask = (uint8_t)(1U << (y % 8U));
            if ((pixels & (uint8_t)(1U << row)) != 0U) {
                display_state.framebuffer[offset] |= mask;
            } else {
                display_state.framebuffer[offset] &= (uint8_t)~mask;
            }
        }
    }
    display_state.cursor_x += 6U;
}

static int display_write(const void *data, size_t length, void *context)
{
    const char *text = (const char *)data;
    size_t index;

    (void)context;
    for (index = 0U; index < length; ++index) {
        draw_character(text[index]);
    }
    return (refresh_display() == MINIOS_MODULE_OK) ? OS_DEVICE_OK
                                                    : OS_DEVICE_ERROR;
}

static int parse_byte(const char *value, uint8_t *parsed)
{
    char *end;
    unsigned long number;

    if ((value == NULL) || (value[0] == '\0')) {
        return -1;
    }
    number = strtoul(value, &end, 0);
    if ((*end != '\0') || (number > 255U)) {
        return -1;
    }
    *parsed = (uint8_t)number;
    return 0;
}

static int parse_position(const char *value, uint8_t *x, uint8_t *y)
{
    char *end;
    unsigned long parsed_x;
    unsigned long parsed_y;

    if ((value == NULL) || (x == NULL) || (y == NULL)) {
        return -1;
    }
    parsed_x = strtoul(value, &end, 0);
    if ((end == value) || ((*end != ' ') && (*end != '\t'))) {
        return -1;
    }
    while ((*end == ' ') || (*end == '\t')) {
        ++end;
    }
    value = end;
    parsed_y = strtoul(value, &end, 0);
    while ((*end == ' ') || (*end == '\t')) {
        ++end;
    }
    if ((end == value) || (*end != '\0') ||
        (parsed_x > (SSD1315_WIDTH - 6U)) ||
        (parsed_y > (SSD1315_HEIGHT - 7U))) {
        return -1;
    }
    *x = (uint8_t)parsed_x;
    *y = (uint8_t)parsed_y;
    return 0;
}

static int display_control(const char *operation, const char *value,
                           void *context)
{
    uint8_t commands[2];

    (void)context;
    if (strcmp(operation, "clear") == 0) {
        memset(display_state.framebuffer, 0, sizeof(display_state.framebuffer));
        display_state.cursor_x = 0U;
        display_state.cursor_y = 0U;
        return (refresh_display() == MINIOS_MODULE_OK) ? OS_DEVICE_OK
                                                        : OS_DEVICE_ERROR;
    }
    if (strcmp(operation, "newline") == 0) {
        draw_character('\n');
        return OS_DEVICE_OK;
    }
    if (strcmp(operation, "position") == 0) {
        uint8_t x;
        uint8_t y;

        if (parse_position(value, &x, &y) != 0) {
            return OS_DEVICE_INVALID_ARGUMENT;
        }
        display_state.cursor_x = x;
        display_state.cursor_y = y;
        return OS_DEVICE_OK;
    }
    if (strcmp(operation, "refresh") == 0) {
        return (refresh_display() == MINIOS_MODULE_OK) ? OS_DEVICE_OK
                                                        : OS_DEVICE_ERROR;
    }
    if (strcmp(operation, "contrast") == 0) {
        if ((parse_byte(value, &commands[1]) != 0) ||
            (commands[1] == 0U)) {
            return OS_DEVICE_INVALID_ARGUMENT;
        }
        commands[0] = 0x81U;
        return (send_commands(commands, 2U) == MINIOS_MODULE_OK)
                   ? OS_DEVICE_OK : OS_DEVICE_ERROR;
    }
    if ((strcmp(operation, "on") == 0) ||
        (strcmp(operation, "off") == 0) ||
        (strcmp(operation, "invert") == 0) ||
        (strcmp(operation, "normal") == 0)) {
        commands[0] = (strcmp(operation, "on") == 0) ? 0xafU :
                      (strcmp(operation, "off") == 0) ? 0xaeU :
                      (strcmp(operation, "invert") == 0) ? 0xa7U : 0xa6U;
        return (send_commands(commands, 1U) == MINIOS_MODULE_OK)
                   ? OS_DEVICE_OK : OS_DEVICE_ERROR;
    }
    return OS_DEVICE_NOT_SUPPORTED;
}

static minios_device_t display_device = {
    .name = "display0",
    .device_class = OS_DEVICE_CLASS_CHARACTER,
    .driver = "ssd1315-i2c",
    .description = "SSD1315 128x64 monochrome OLED display",
    .capabilities = OS_DEVICE_CAP_WRITE | OS_DEVICE_CAP_CONTROL,
    .write = display_write,
    .control = display_control,
    .context = &display_state,
};

static int load_ssd1315(int argc, char **argv)
{
    static const uint8_t initialization[] = {
        0xaeU, 0xd5U, 0x80U, 0xa8U, 0x3fU, 0xd3U, 0x00U, 0x40U,
        0x8dU, 0x14U, 0x20U, 0x00U, 0xa1U, 0xc8U, 0xdaU, 0x12U,
        0x81U, 0x7fU, 0xd9U, 0xf1U, 0xdbU, 0x20U, 0xa4U, 0xa6U,
        0x2eU, 0xafU,
    };
    uint8_t address = SSD1315_DEFAULT_ADDRESS;
    int result;

    if ((argc > 1) || ((argc == 1) && (parse_byte(argv[0], &address) != 0)) ||
        (address < 0x03U) || (address > 0x77U)) {
        return MINIOS_MODULE_INVALID_ARGUMENT;
    }
    memset(&display_state, 0, sizeof(display_state));
    result = minios_hal_i2c_device_open(address, SSD1315_I2C_FREQUENCY,
                                        &display_state.i2c_device);
    if (result == MINIOS_HAL_NOT_INITIALIZED) {
        return MINIOS_MODULE_DEPENDENCY;
    }
    if (result != MINIOS_HAL_OK) {
        return (result == MINIOS_HAL_BUSY) ? MINIOS_MODULE_BUSY
                                           : MINIOS_MODULE_ERROR;
    }
    if ((send_commands(initialization, sizeof(initialization)) !=
         MINIOS_MODULE_OK) || (refresh_display() != MINIOS_MODULE_OK) ||
        (os_device_register(&display_device) != OS_DEVICE_OK)) {
        (void)minios_hal_i2c_device_close(display_state.i2c_device);
        memset(&display_state, 0, sizeof(display_state));
        return MINIOS_MODULE_ERROR;
    }
    return MINIOS_MODULE_OK;
}

static int unload_ssd1315(void)
{
    const uint8_t display_off = 0xaeU;

    (void)send_commands(&display_off, 1U);
    if (minios_hal_i2c_device_close(display_state.i2c_device) !=
        MINIOS_HAL_OK) {
        return MINIOS_MODULE_BUSY;
    }
    (void)os_device_unregister(display_device.name);
    memset(&display_state, 0, sizeof(display_state));
    return MINIOS_MODULE_OK;
}

const minios_module_descriptor_t *minios_module_ssd1315_descriptor(void)
{
    static const minios_module_descriptor_t descriptor = {
        .info = {
            .name = "ssd1315",
            .description = "SSD1315 128x64 OLED display over I2C",
            .device = "/dev/display0",
            .loaded = 0,
        },
        .load = load_ssd1315,
        .unload = unload_ssd1315,
    };

    return &descriptor;
}
