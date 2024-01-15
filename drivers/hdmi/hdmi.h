#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "inttypes.h"
#include "stdbool.h"

#include "hardware/pio.h"

#define PIO_VIDEO pio0
#define PIO_VIDEO_ADDR pio0
#define VIDEO_DMA_IRQ (DMA_IRQ_0)

#define TEXTMODE_COLS 53
#define TEXTMODE_ROWS 30

#define RGB888(r, g, b) ((r<<16) | (g << 8 ) | b )

#define beginVideo_PIN (6)

#define HDMI_PIN_invert_diffpairs (1)
#define HDMI_PIN_RGB_notBGR (1)
#define beginHDMI_PIN_data (beginVideo_PIN+2)
#define beginHDMI_PIN_clk (beginVideo_PIN)


// TODO: Сделать настраиваемо
const uint8_t __scratch_y("hdmi_textmode_palette") textmode_palette[16] = {
    200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215
};

enum graphics_mode_t {
    TEXTMODE_53x30,
    TEXTMODE_80x30,
    TEXTMODE_160x100,

    CGA_160x200x16,
    CGA_320x200x4,
    CGA_640x200x2,

    TGA_320x200x16,
    EGA_320x200x16x4,
    // planar EGA
    VGA_320x200x256,
    VGA_320x240x256,
    VGA_320x200x256x4,
    // planar VGA
};

// выделение и настройка общих ресурсов - 4 DMA канала, PIO программ и 2 SM
void graphics_init();

//выбор видеорежима
enum graphics_mode_t graphics_set_mode(enum graphics_mode_t mode);


void graphics_set_buffer(uint8_t* buffer, uint16_t width, uint16_t height);

void graphics_set_offset(int x, int y);

void graphics_set_bgcolor(uint32_t color888); //определяем зарезервированный цвет в палитре
void v_drv_set_bg_color(uint8_t color); //используем номер цвета палитры
void graphics_set_palette(uint8_t i, uint32_t color888);

void v_drv_set_txt_palette(uint8_t i_color, uint8_t pal_inx);


void graphics_set_textbuffer(uint8_t* buffer);


//придумать как передавать доп параметры

//добавить режим прозрачного текста
// void setVGA_color_flash_mode(bool flash_line,bool flash_frame);
// void setVGA_color_palette_222(uint8_t i_color, uint32_t color888);
void graphics_set_flashmode(bool flash_line, bool flash_frame);

//на момент вызова этих функций должны быть определены текстовые буферы верного размера
void clrScr(uint8_t color);

void draw_text(char* string, uint32_t x, uint32_t y, uint8_t color, uint8_t bgcolor);

void draw_text_len(char* str, int x, int y, uint len, uint8_t color, uint8_t bgcolor);

void draw_window(char* title, uint32_t x, uint32_t y, uint32_t width, uint32_t height);


inline void graphics_set_flashmode(bool flash_line, bool flash_frame) {
    // dummy
}

void logMsg(char* msg);
#ifdef __cplusplus
}
#endif
