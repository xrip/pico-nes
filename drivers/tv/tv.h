#pragma once

#include "stdbool.h"

#define PIO_VIDEO pio0
#define PIO_VIDEO_ADDR pio0

#define TV_BASE_PIN (6)

// TODO: Сделать настраиваемо
static const uint8_t textmode_palette[16] = {
    200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215
};

#define TEXTMODE_COLS 53
#define TEXTMODE_ROWS 30

#define RGB888(r, g, b) ((r<<16) | (g << 8 ) | b )

typedef enum g_mode {
    g_mode_320x240x8bpp,
    g_mode_320x240x4bpp
} g_mode;

typedef enum g_out_TV {
    TV_OUT_PAL,
    TV_OUT_NTSC
} g_out_TV;


//для совместимости
typedef enum fr_rate {
    rate_60Hz = 0,
    rate_72Hz = 1,
    rate_75Hz = 2,
    rate_85Hz = 3
} fr_rate;

typedef enum g_out {
    g_out_AUTO = 0,
    g_out_VGA = 1,
    g_out_HDMI = 2
} g_out;

bool graphics_try_framerate(g_out g_out, fr_rate rate, bool apply);


static void graphics_set_flashmode(bool flash_line, bool flash_frame) {
    // dummy
}

static void graphics_set_bgcolor(uint32_t color888) {
    // dummy
}