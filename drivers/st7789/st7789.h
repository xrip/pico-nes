#pragma once


#ifndef TFT_RST_PIN
#define TFT_RST_PIN 8
#endif


#ifndef TFT_CS_PIN
#define TFT_CS_PIN 6
#endif


#ifndef TFT_LED_PIN
#define TFT_LED_PIN 9
#endif


#ifndef TFT_CLK_PIN
#define TFT_CLK_PIN 13
#endif 

#ifndef TFT_DATA_PIN
#define TFT_DATA_PIN 12
#endif

#ifndef TFT_DC_PIN
#define TFT_DC_PIN 10
#endif

#define TEXTMODE_COLS 53
#define TEXTMODE_ROWS 30

extern uint8_t INVERSION;

#define RGB888(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

inline static void graphics_set_bgcolor(uint32_t color888) {
    // dummy
}
inline static void graphics_set_flashmode(bool flash_line, bool flash_frame) {
    // dummy
}
void refresh_lcd();
