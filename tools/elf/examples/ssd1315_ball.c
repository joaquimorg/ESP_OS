#include "minios.h"

#define DISPLAY_NAME "display0"
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define BALL_RADIUS 3
#define DEFAULT_DELAY_MS 40U

static int parse_positive(const char *text, uint32_t *value)
{
    uint32_t parsed = 0U;

    if ((text == NULL) || (*text == '\0') || (value == NULL)) {
        return -1;
    }
    while (*text != '\0') {
        unsigned int digit;

        if ((*text < '0') || (*text > '9')) {
            return -1;
        }
        digit = (unsigned int)(*text - '0');
        if (parsed > ((UINT32_MAX - digit) / 10U)) {
            return -1;
        }
        parsed = (parsed * 10U) + digit;
        ++text;
    }
    if (parsed == 0U) {
        return -1;
    }
    *value = parsed;
    return 0;
}

static char *append_unsigned(char *output, unsigned int value)
{
    char reversed[10];
    unsigned int used = 0U;

    do {
        reversed[used++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);
    while (used != 0U) {
        *output++ = reversed[--used];
    }
    return output;
}

static int draw_pixel(int x, int y)
{
    char value[12];
    char *cursor = value;

    cursor = append_unsigned(cursor, (unsigned int)x);
    *cursor++ = ' ';
    cursor = append_unsigned(cursor, (unsigned int)y);
    *cursor = '\0';
    return os_device_control(DISPLAY_NAME, "pixel", value);
}

static int draw_ball(int center_x, int center_y)
{
    int offset_y;

    for (offset_y = -BALL_RADIUS; offset_y <= BALL_RADIUS; ++offset_y) {
        int offset_x;

        for (offset_x = -BALL_RADIUS; offset_x <= BALL_RADIUS; ++offset_x) {
            if (((offset_x * offset_x) + (offset_y * offset_y)) <=
                (BALL_RADIUS * BALL_RADIUS)) {
                if (draw_pixel(center_x + offset_x,
                               center_y + offset_y) != OS_DEVICE_OK) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

int minios_app_main(int argc, char **argv)
{
    uint32_t delay_ms = DEFAULT_DELAY_MS;
    uint32_t frame_limit = 0U;
    uint32_t frame = 0U;
    int x = 12;
    int y = 10;
    int velocity_x = 2;
    int velocity_y = 1;

    if ((argc > 0) && (parse_positive(argv[0], &delay_ms) != 0)) {
        os_print("Usage: run ssd1315_ball.elf [delay_ms] [frames]\r\n");
        return 1;
    }
    if ((argc > 1) && (parse_positive(argv[1], &frame_limit) != 0)) {
        os_print("Usage: run ssd1315_ball.elf [delay_ms] [frames]\r\n");
        return 1;
    }
    if (argc > 2) {
        os_print("Usage: run ssd1315_ball.elf [delay_ms] [frames]\r\n");
        return 1;
    }
    if (os_device_find(DISPLAY_NAME) == NULL) {
        os_print("display0 not found; load the ssd1315 module first.\r\n");
        return 1;
    }
    os_print("SSD1315 bouncing ball started; use kill <pid> to stop.\r\n");
    while (!os_app_should_stop() &&
           ((frame_limit == 0U) || (frame < frame_limit))) {
        if ((os_device_control(DISPLAY_NAME, "frame-clear", NULL) !=
             OS_DEVICE_OK) ||
            (draw_ball(x, y) != 0) ||
            (os_device_control(DISPLAY_NAME, "refresh", NULL) !=
             OS_DEVICE_OK)) {
            os_print("SSD1315 drawing failed.\r\n");
            return 1;
        }
        minios_sleep(delay_ms);
        x += velocity_x;
        y += velocity_y;
        if (x <= BALL_RADIUS) {
            x = BALL_RADIUS;
            velocity_x = -velocity_x;
        } else if (x >= (DISPLAY_WIDTH - 1 - BALL_RADIUS)) {
            x = DISPLAY_WIDTH - 1 - BALL_RADIUS;
            velocity_x = -velocity_x;
        }
        if (y <= BALL_RADIUS) {
            y = BALL_RADIUS;
            velocity_y = -velocity_y;
        } else if (y >= (DISPLAY_HEIGHT - 1 - BALL_RADIUS)) {
            y = DISPLAY_HEIGHT - 1 - BALL_RADIUS;
            velocity_y = -velocity_y;
        }
        ++frame;
    }
    os_print("SSD1315 bouncing ball stopped.\r\n");
    return 0;
}
