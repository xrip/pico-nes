#ifndef _ST7789_LCD_H_
#define _ST7789_LCD_H_

#ifndef PIN_RESET
#define PIN_RESET 6
#endif


#ifndef PIN_CS
#define PIN_CS 7
#endif


#ifndef PIN_BL
#define PIN_BL 8
#endif


#ifndef PIN_CLK
#define PIN_CLK 10
#endif 

#ifndef PIN_DIN
#define PIN_DIN 11
#endif

#ifndef PIN_DC
#define PIN_DC 13
#endif

#define TEXTMODE_COLS 53
#define TEXTMODE_ROWS 30

#define RGB888(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

enum graphics_mode_t {
    TEXTMODE_40x30,
    TEXTMODE_80x30,
    TEXTMODE_160x100,

    CGA_160x200x16,
    CGA_320x200x4,
    CGA_640x200x2,

    TGA_320x200x16,
    EGA_320x200x16x4, // planar EGA
    VGA_320x200x256,
    VGA_320x200x256x4, // planar VGA
};

const uint16_t __scratch_y("tft_textmode_palette") textmode_palette[16] = {
    //R, G, B
    RGB888(0x00,0x00, 0x00), //black
    RGB888(0x00,0x00, 0xC4), //blue
    RGB888(0x00,0xC4, 0x00), //green
    RGB888(0x00,0xC4, 0xC4), //cyan
    RGB888(0xC4,0x00, 0x00), //red
    RGB888(0xC4,0x00, 0xC4), //magenta
    RGB888(0xC4,0x7E, 0x00), //brown
    RGB888(0xC4,0xC4, 0xC4), //light gray
    RGB888(0x4E,0x4E, 0x4E), //dark gray
    RGB888(0x4E,0x4E, 0xDC), //light blue
    RGB888(0x4E,0xDC, 0x4E), //light green
    RGB888(0x4E,0xF3, 0xF3), //light cyan
    RGB888(0xDC,0x4E, 0x4E), //light red
    RGB888(0xF3,0x4E, 0xF3), //light magenta
    RGB888(0xF3,0xF3, 0x4E), //yellow
    RGB888(0xFF,0xFF, 0xFF), //white
};

void graphics_init();
enum graphics_mode_t graphics_set_mode(enum graphics_mode_t mode);
void graphics_set_buffer(uint8_t *buffer, uint16_t width, uint16_t height);
void graphics_set_offset(int x, int y);

void graphics_set_palette(uint8_t i, uint32_t color);

void graphics_set_textbuffer(uint8_t *buffer);

inline void graphics_set_bgcolor(uint32_t color888) {
// dummy
}
inline void graphics_set_flashmode(bool flash_line, bool flash_frame) {
// dummy
}
void draw_text(char* string, uint32_t x, uint32_t y, uint8_t color, uint8_t bgcolor);
void draw_window(char* title, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void clrScr(uint8_t color);
void logMsg(char * msg);
void refresh_lcd();
#endif