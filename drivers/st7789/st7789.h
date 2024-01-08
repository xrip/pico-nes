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


void graphics_init();
void graphics_set_buffer(uint8_t *buffer, uint16_t width, uint16_t height);

void graphics_set_palette(uint8_t i, uint32_t color);

inline void graphics_set_offset(int x, int y) {
// dummy
}

inline void graphics_set_textbuffer(uint8_t *buffer) {
// dummy
}

inline void graphics_set_bgcolor(uint32_t color888) {
// dummy
}
inline void graphics_set_flashmode(bool flash_line, bool flash_frame) {
// dummy
}

void logMsg(char * msg);
void refresh_lcd(int16_t x,
                        int16_t y,
                        int16_t width,
                        int16_t height,
                        const uint8_t *bitmap);
#endif