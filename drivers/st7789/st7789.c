/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"

#include "st7789.h"

#include <string.h>

#include "st7789.pio.h"
#include "fnt6x8.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define SERIAL_CLK_DIV 2.f

#define CHECK_BIT(var, pos) (((var)>>(pos)) & 1)

static uint sm = 0;
static PIO pio = pio0;
uint16_t palette[256] = { 0 };

static uint8_t* text_buffer = NULL;
static uint8_t* graphics_buffer = NULL;

static uint graphics_buffer_width = 0;
static uint graphics_buffer_height = 0;
static int graphics_buffer_shift_x = 0;
static int graphics_buffer_shift_y = 0;

enum graphics_mode_t graphics_mode;

static const uint8_t st7789_init_seq[] = {
    1, 20, 0x01, // Software reset
    1, 10, 0x11, // Exit sleep mode
    2, 2, 0x3a, 0x55, // Set colour mode to 16 bit
    2, 0, 0x36, 0b01100000, // Set MADCTL: row then column, refresh is bottom to top ????
    //2, 0, 0x36, 0x60,             // Set MADCTL: row then column, refresh is bottom to top ????
    5, 0, 0x2a, 0x00, 0x00, SCREEN_WIDTH >> 8, SCREEN_WIDTH & 0xff, // CASET: column addresses
    5, 0, 0x2b, 0x00, 0x00, SCREEN_HEIGHT >> 8, SCREEN_HEIGHT & 0xff, // RASET: row addresses
    1, 2, 0x20, // Inversion on, then 10 ms delay (supposedly a hack?)
    1, 2, 0x13, // Normal display on, then 10 ms delay
    1, 2, 0x29, // Main screen turn on, then wait 500 ms
    0 // Terminate list
};
// Format: cmd length (including cmd byte), post delay in units of 5 ms, then cmd payload
// Note the delays have been shortened a little


static inline void lcd_set_dc_cs(bool dc, bool cs) {
    sleep_us(1);
    gpio_put_masked((1u << PIN_DC) | (1u << PIN_CS), !!dc << PIN_DC | !!cs << PIN_CS);
    sleep_us(1);
}

static inline void lcd_write_cmd(PIO pio, uint sm, const uint8_t* cmd, size_t count) {
    st7789_lcd_wait_idle(pio, sm);
    lcd_set_dc_cs(0, 0);
    st7789_lcd_put(pio, sm, *cmd++);
    if (count >= 2) {
        st7789_lcd_wait_idle(pio, sm);
        lcd_set_dc_cs(1, 0);
        for (size_t i = 0; i < count - 1; ++i)
            st7789_lcd_put(pio, sm, *cmd++);
    }
    st7789_lcd_wait_idle(pio, sm);
    lcd_set_dc_cs(1, 1);
}

static inline void lcd_set_window(PIO pio, uint sm,
                                  uint16_t x,
                                  uint16_t y,
                                  uint16_t width,
                                  uint16_t height) {
    static uint8_t screen_width_cmd[] = { 0x2a, 0x00, 0x00, SCREEN_WIDTH >> 8, SCREEN_WIDTH & 0xff };
    static uint8_t screen_height_command[] = { 0x2b, 0x00, 0x00, SCREEN_HEIGHT >> 8, SCREEN_HEIGHT & 0xff };
    screen_width_cmd[2] = x;
    screen_width_cmd[4] = x + width - 1;

    screen_height_command[2] = y;
    screen_height_command[4] = y + height - 1;
    lcd_write_cmd(pio, sm, screen_width_cmd, 5);
    lcd_write_cmd(pio, sm, screen_height_command, 5);
}

static inline void lcd_init(PIO pio, uint sm, const uint8_t* init_seq) {
    const uint8_t* cmd = init_seq;
    while (*cmd) {
        lcd_write_cmd(pio, sm, cmd + 2, *cmd);
        sleep_ms(*(cmd + 1) * 5);
        cmd += *cmd + 2;
    }
}

static inline void st7789_start_pixels(PIO pio, uint sm) {
    uint8_t cmd = 0x2c; // RAMWR
    lcd_write_cmd(pio, sm, &cmd, 1);
    lcd_set_dc_cs(1, 0);
}

void graphics_init() {
    uint offset = pio_add_program(pio, &st7789_lcd_program);
    st7789_lcd_program_init(pio, sm, offset, PIN_DIN, PIN_CLK, SERIAL_CLK_DIV);

    gpio_init(PIN_CS);
    gpio_init(PIN_DC);
    gpio_init(PIN_RESET);
    gpio_init(PIN_BL);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_RESET, GPIO_OUT);
    gpio_set_dir(PIN_BL, GPIO_OUT);

    gpio_put(PIN_CS, 1);
    gpio_put(PIN_RESET, 1);
    lcd_init(pio, sm, st7789_init_seq);
    gpio_put(PIN_BL, 1);
    clrScr(0);
}

enum graphics_mode_t graphics_set_mode(enum graphics_mode_t mode) {
    graphics_mode = mode;

    if(TEXTMODE_80x30 == mode) {
        memset(graphics_buffer, 0, graphics_buffer_height * graphics_buffer_width);
    }

    sleep_ms(50);
    clrScr(0);

    return graphics_mode;
}

void graphics_set_buffer(uint8_t* buffer, uint16_t width, uint16_t height) {
    graphics_buffer = buffer;
    graphics_buffer_width = width;
    graphics_buffer_height = height;
}

void graphics_set_textbuffer(uint8_t* buffer) {
    text_buffer = buffer;
}

void graphics_set_offset(int x, int y) {
    graphics_buffer_shift_x = x;
    graphics_buffer_shift_y = y;
}

void clrScr(uint8_t color) {
    lcd_set_window(pio, sm, 0, 0,SCREEN_WIDTH,SCREEN_HEIGHT);
    st7789_start_pixels(pio, sm);
    uint32_t i = SCREEN_WIDTH * SCREEN_HEIGHT;
    do {
        st7789_lcd_put(pio, sm, color);
        st7789_lcd_put(pio, sm, color);
    }
    while (--i);
}

void draw_text(char* string, int x, int y, uint8_t color, uint8_t bgcolor) {
    uint8_t* t_buf = text_buffer + TEXTMODE_COLS * 2 * y + 2 * x;
    for (int xi = TEXTMODE_COLS * 2; xi--;) {
        if (!(*string)) break;
        *t_buf++ = *string++;
        *t_buf++ = (bgcolor << 4) | (color & 0xF);
    }
};

// TODO: Tick 60 times per second
void refresh_lcd() {
    switch (graphics_mode) {
        case TEXTMODE_80x30:
            lcd_set_window(pio, sm, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            st7789_start_pixels(pio, sm);

            for (int y = 0; y < SCREEN_HEIGHT; y++) {
                st7789_lcd_put(pio, sm, 0x00);
                st7789_lcd_put(pio, sm, 0x00);

                for (int x = 0; x < TEXTMODE_COLS; x++) {
                    const uint16_t offset = (y / 8) * (TEXTMODE_COLS * 2) + x * 2;
                    const uint8_t c = text_buffer[offset];
                    const uint8_t colorIndex = text_buffer[offset+1];
                    const uint8_t glyph_row = fnt6x8[c * 8 + y % 8];

                    for (uint_fast8_t bit = 0; bit < 6; bit++) {
                        const uint16_t color = textmode_palette[ (c && CHECK_BIT(glyph_row, bit)) ? colorIndex & 0x0F : (colorIndex >> 4) & 0x0F];
                        st7789_lcd_put(pio, sm, color >> 8);
                        st7789_lcd_put(pio, sm, color & 0xff);
                    }
                }
                st7789_lcd_put(pio, sm, 0x00);
                st7789_lcd_put(pio, sm, 0x00);
            }
            break;
        default: {
            const uint8_t* bitmap = graphics_buffer;

            lcd_set_window(pio, sm, graphics_buffer_shift_x, graphics_buffer_shift_y, graphics_buffer_width,
               graphics_buffer_height);
            st7789_start_pixels(pio, sm);

            uint32_t i = graphics_buffer_width * graphics_buffer_height;
            do {
                const uint16_t color = palette[*bitmap++];
                st7789_lcd_put(pio, sm, color >> 8);
                st7789_lcd_put(pio, sm, color & 0xff);
            }
            while (--i);
        }
    }

    //st7789_lcd_wait_idle(pio, sm);
}


void graphics_set_palette(uint8_t i, uint32_t color) {
    palette[i] = color;
}

void logMsg(char* msg) {
    // dummy
}
