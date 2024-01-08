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
#include "st7789.pio.h"

// Tested with the parts that have the height of 240 and 320
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define IMAGE_SIZE 256
#define LOG_IMAGE_SIZE 8


#define SERIAL_CLK_DIV 2.f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint sm = 0;
static PIO pio = pio0;
uint16_t palette[256] = { 0 };

static const uint8_t st7789_init_seq[] = {
    1, 20, 0x01,                        // Software reset
    1, 10, 0x11,                        // Exit sleep mode
    2, 2, 0x3a, 0x55,                   // Set colour mode to 16 bit
    2, 0, 0x36, 0b01100000,             // Set MADCTL: row then column, refresh is bottom to top ????
    //2, 0, 0x36, 0x60,             // Set MADCTL: row then column, refresh is bottom to top ????
    5, 0, 0x2a, 0x00, 0x00, SCREEN_WIDTH >> 8, SCREEN_WIDTH & 0xff,   // CASET: column addresses
    5, 0, 0x2b, 0x00, 0x00, SCREEN_HEIGHT >> 8, SCREEN_HEIGHT & 0xff, // RASET: row addresses
    1, 2, 0x20,                         // Inversion on, then 10 ms delay (supposedly a hack?)
    1, 2, 0x13,                         // Normal display on, then 10 ms delay
    1, 2, 0x29,                         // Main screen turn on, then wait 500 ms
    0                                   // Terminate list
};
// Format: cmd length (including cmd byte), post delay in units of 5 ms, then cmd payload
// Note the delays have been shortened a little


static inline void lcd_set_dc_cs(bool dc, bool cs) {
    sleep_us(1);
    gpio_put_masked((1u << PIN_DC) | (1u << PIN_CS), !!dc << PIN_DC | !!cs << PIN_CS);
    sleep_us(1);
}

static inline void lcd_write_cmd(PIO pio, uint sm, const uint8_t *cmd, size_t count) {
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
                                  int16_t x, 
                                  int16_t y, 
                                  int16_t width, 
                                  int16_t height)
{

static uint8_t screen_width_cmd[]={0x2a, 0x00, 0x00, SCREEN_WIDTH >> 8, SCREEN_WIDTH & 0xff};
static uint8_t screen_height_command[]={0x2b, 0x00, 0x00, SCREEN_HEIGHT >> 8, SCREEN_HEIGHT & 0xff};
        screen_width_cmd[2]=x;
        screen_width_cmd[4]=x+width-1;

        screen_height_command[2]=y;
        screen_height_command[4]=y+height-1;
        lcd_write_cmd(pio,sm,screen_width_cmd,5);
        lcd_write_cmd(pio,sm,screen_height_command,5);


}

static inline void lcd_init(PIO pio, uint sm, const uint8_t *init_seq) {
    const uint8_t *cmd = init_seq;
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

void graphics_init()
{
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
    lcd_clear();
}

void graphics_set_buffer(uint8_t *buffer, uint16_t width, uint16_t height) {

}

void lcd_clear() {
    lcd_set_window(pio,sm,0,0,SCREEN_WIDTH,SCREEN_HEIGHT);
    st7789_start_pixels(pio, sm);
    uint32_t i = (SCREEN_WIDTH * SCREEN_HEIGHT);
    do {
        st7789_lcd_put(pio, sm, 0);
        st7789_lcd_put(pio, sm, 0);
    } while(--i);
}

void refresh_lcd(int16_t x,
                 int16_t y, 
                 int16_t width, 
                 int16_t height, 
                 const uint8_t *bitmap)
{
    lcd_set_window(pio,sm,x,y,width,height);
    st7789_start_pixels(pio, sm);
    uint32_t i = (width * height);
    do {
        uint16_t color = palette[*bitmap++];
        // uint16_t color = 0x001F;
        st7789_lcd_put(pio, sm, color >> 8);
        st7789_lcd_put(pio, sm, color & 0xff);
    } while(--i);
       
    //st7789_lcd_wait_idle(pio, sm);

}


void graphics_set_palette(uint8_t i, uint32_t color) {
    palette[i] = color;
}

void logMsg(char * msg) {
// dummy
}