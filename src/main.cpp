#include <cstdio>
#include <cstring>
#include <cstdarg>

#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/i2c.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include <hardware/sync.h>
#include <hardware/flash.h>

#include <InfoNES.h>
#include <InfoNES_System.h>
#include <bsp/board_api.h>

#include "InfoNES_Mapper.h"

#include "graphics.h"

#include "audio.h"
#include "ff.h"

#if USE_PS2_KBD

#include "ps2kbd_mrmltr.h"

#endif
#if USE_NESPAD

#include "nespad.h"

#endif

#pragma GCC optimize("Ofast")

#define HOME_DIR (char*)"\\NES"

#ifdef BUILD_IN_GAMES
#include "lzwSource.h"
#include "lzw.h"
size_t get_rom4prog_size() { return sizeof(rom); }
#else
#define FLASH_TARGET_OFFSET (1024 * 1024)
const char* rom_filename = (const char *)(XIP_BASE + FLASH_TARGET_OFFSET);
const uint8_t* rom = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET) + 4096;
size_t get_rom4prog_size() { return FLASH_TARGET_OFFSET - 4096; }

class Decoder {
};
#endif
uint32_t get_rom4prog() { return (uint32_t)rom; }
bool cursor_blink_state = false;
uint8_t CURSOR_X, CURSOR_Y = 0;
uint8_t manager_started = false;

enum menu_type_e {
    NONE,
    INT,
    TEXT,
    SAVE, LOAD,
    ARRAY,
    RESET,
    RETURN,
    SUBMENU,
    ROM_SELECT,
    USB_DEVICE,
};

struct semaphore vga_start_semaphore;
uint8_t SCREEN[NES_DISP_HEIGHT][NES_DISP_WIDTH]; // 61440 bytes
uint16_t linebuffer[256];

enum PALETTES {
    RGB333,
    RBG222,
};

enum INPUT {
    KEYBOARD,
    GAMEPAD1,
    GAMEPAD2,
};

typedef struct __attribute__((__packed__)) {
    uint8_t version;
    bool show_fps;
    bool flash_line;
    bool flash_frame;
    PALETTES palette;
    uint8_t snd_vol;
    INPUT player_1_input;
    INPUT player_2_input;
    uint8_t nes_palette;
} SETTINGS;

SETTINGS settings = {
    .version = 2,
    .show_fps = false,
    .flash_line = true,
    .flash_frame = true,
    .palette = RGB333,
    .snd_vol = 8,
    .player_1_input = GAMEPAD1,
    .player_2_input = KEYBOARD,
    .nes_palette = 0,
};

static FATFS fs, fs1;

FATFS* getFlashInDriveFATFSptr() {
    return &fs1;
}

FATFS* getSDCardFATFSptr() {
    return &fs;
}

i2s_config_t i2s_config;

char fps_text[3] = { "0" };
int start_time;
int frames;

const int __not_in_flash_func(NesPalette888)[] = {
    /**/
    RGB888(0x7c, 0x7c, 0x7c),
    RGB888(0x00, 0x00, 0xfc),
    RGB888(0x00, 0x00, 0xbc),
    RGB888(0x44, 0x28, 0xbc),

    RGB888(0x94, 0x00, 0x84),
    RGB888(0xa8, 0x00, 0x20),
    RGB888(0xa8, 0x10, 0x00),
    RGB888(0x88, 0x14, 0x00),

    RGB888(0x50, 0x30, 0x00),
    RGB888(0x00, 0x78, 0x00),
    RGB888(0x00, 0x68, 0x00),
    RGB888(0x00, 0x58, 0x00),

    RGB888(0x00, 0x40, 0x58),
    RGB888(0x00, 0x00, 0x00),
    RGB888(0x00, 0x00, 0x00),
    RGB888(0x00, 0x00, 0x00),

    RGB888(0xbc, 0xbc, 0xbc),
    RGB888(0x00, 0x78, 0xf8),
    RGB888(0x00, 0x58, 0xf8),
    RGB888(0x68, 0x44, 0xfc),

    RGB888(0xd8, 0x00, 0xcc),
    RGB888(0xe4, 0x00, 0x58),
    RGB888(0xf8, 0x38, 0x00),
    RGB888(0xe4, 0x5c, 0x10),

    RGB888(0xac, 0x7c, 0x00),
    RGB888(0x00, 0xb8, 0x00),
    RGB888(0x00, 0xa8, 0x00),
    RGB888(0x00, 0xa8, 0x44),

    RGB888(0x00, 0x88, 0x88),
    RGB888(0x00, 0x00, 0x00),
    RGB888(0x00, 0x00, 0x00),
    RGB888(0x00, 0x00, 0x00),

    RGB888(0xf8, 0xf8, 0xf8),
    RGB888(0x3c, 0xbc, 0xfc),
    RGB888(0x68, 0x88, 0xfc),
    RGB888(0x98, 0x78, 0xf8),

    RGB888(0xf8, 0x78, 0xf8),
    RGB888(0xf8, 0x58, 0x98),
    RGB888(0xf8, 0x78, 0x58),
    RGB888(0xfc, 0xa0, 0x44),

    RGB888(0xf8, 0xb8, 0x00),
    RGB888(0xb8, 0xf8, 0x18),
    RGB888(0x58, 0xd8, 0x54),
    RGB888(0x58, 0xf8, 0x98),

    RGB888(0x00, 0xe8, 0xd8),
    RGB888(0x78, 0x78, 0x78),
    RGB888(0x00, 0x00, 0x00),
    RGB888(0x00, 0x00, 0x00),

    RGB888(0xfc, 0xfc, 0xfc),
    RGB888(0xa4, 0xe4, 0xfc),
    RGB888(0xb8, 0xb8, 0xf8),
    RGB888(0xd8, 0xb8, 0xf8),

    RGB888(0xf8, 0xb8, 0xf8),
    RGB888(0xf8, 0xa4, 0xc0),
    RGB888(0xf0, 0xd0, 0xb0),
    RGB888(0xfc, 0xe0, 0xa8),

    RGB888(0xf8, 0xd8, 0x78),
    RGB888(0xd8, 0xf8, 0x78),
    RGB888(0xb8, 0xf8, 0xb8),
    RGB888(0xb8, 0xf8, 0xd8),

    RGB888(0x00, 0xfc, 0xfc),
    RGB888(0xf8, 0xd8, 0xf8),
    RGB888(0x00, 0x00, 0x00),
    RGB888(0x00, 0x00, 0x00),
    /**/
    /* Matthew Conte's Palette */
    /**/
    RGB888(0x80, 0x80, 0x80),
    RGB888(0x00, 0x00, 0xbb),
    RGB888(0x37, 0x00, 0xbf),
    RGB888(0x84, 0x00, 0xa6),
    RGB888(0xbb, 0x00, 0x6a),
    RGB888(0xb7, 0x00, 0x1e),
    RGB888(0xb3, 0x00, 0x00),
    RGB888(0x91, 0x26, 0x00),
    RGB888(0x7b, 0x2b, 0x00),
    RGB888(0x00, 0x3e, 0x00),
    RGB888(0x00, 0x48, 0x0d),
    RGB888(0x00, 0x3c, 0x22),
    RGB888(0x00, 0x2f, 0x66),
    RGB888(0x00, 0x00, 0x00),
    RGB888(0x05, 0x05, 0x05),
    RGB888(0x05, 0x05, 0x05),
    RGB888(0xc8, 0xc8, 0xc8),
    RGB888(0x00, 0x59, 0xff),
    RGB888(0x44, 0x3c, 0xff),
    RGB888(0xb7, 0x33, 0xcc),
    RGB888(0xff, 0x33, 0xaa),
    RGB888(0xff, 0x37, 0x5e),
    RGB888(0xff, 0x37, 0x1a),
    RGB888(0xd5, 0x4b, 0x00),
    RGB888(0xc4, 0x62, 0x00),
    RGB888(0x3c, 0x7b, 0x00),
    RGB888(0x1e, 0x84, 0x15),
    RGB888(0x00, 0x95, 0x66),
    RGB888(0x00, 0x84, 0xc4),
    RGB888(0x11, 0x11, 0x11),
    RGB888(0x09, 0x09, 0x09),
    RGB888(0x09, 0x09, 0x09),
    RGB888(0xff, 0xff, 0xff),
    RGB888(0x00, 0x95, 0xff),
    RGB888(0x6f, 0x84, 0xff),
    RGB888(0xd5, 0x6f, 0xff),
    RGB888(0xff, 0x77, 0xcc),
    RGB888(0xff, 0x6f, 0x99),
    RGB888(0xff, 0x7b, 0x59),
    RGB888(0xff, 0x91, 0x5f),
    RGB888(0xff, 0xa2, 0x33),
    RGB888(0xa6, 0xbf, 0x00),
    RGB888(0x51, 0xd9, 0x6a),
    RGB888(0x4d, 0xd5, 0xae),
    RGB888(0x00, 0xd9, 0xff),
    RGB888(0x66, 0x66, 0x66),
    RGB888(0x0d, 0x0d, 0x0d),
    RGB888(0x0d, 0x0d, 0x0d),
    RGB888(0xff, 0xff, 0xff),
    RGB888(0x84, 0xbf, 0xff),
    RGB888(0xbb, 0xbb, 0xff),
    RGB888(0xd0, 0xbb, 0xff),
    RGB888(0xff, 0xbf, 0xea),
    RGB888(0xff, 0xbf, 0xcc),
    RGB888(0xff, 0xc4, 0xb7),
    RGB888(0xff, 0xcc, 0xae),
    RGB888(0xff, 0xd9, 0xa2),
    RGB888(0xcc, 0xe1, 0x99),
    RGB888(0xae, 0xee, 0xb7),
    RGB888(0xaa, 0xf7, 0xee),
    RGB888(0xb3, 0xee, 0xff),
    RGB888(0xdd, 0xdd, 0xdd),
    RGB888(0x11, 0x11, 0x11),
    RGB888(0x11, 0x11, 0x11),
    /**/
};

struct st_palettes {
   char name[32];
   char desc[32];
   uint32_t data[64];
};

static const struct st_palettes extra_palettes[] = {
   { "asqrealc", "AspiringSquire's Real palette",
      { 0x6c6c6c, 0x00268e, 0x0000a8, 0x400094,
       0x700070, 0x780040, 0x700000, 0x621600,
       0x442400, 0x343400, 0x005000, 0x004444,
       0x004060, 0x000000, 0x101010, 0x101010,
       0xbababa, 0x205cdc, 0x3838ff, 0x8020f0,
       0xc000c0, 0xd01474, 0xd02020, 0xac4014,
       0x7c5400, 0x586400, 0x008800, 0x007468,
       0x00749c, 0x202020, 0x101010, 0x101010,
       0xffffff, 0x4ca0ff, 0x8888ff, 0xc06cff,
       0xff50ff, 0xff64b8, 0xff7878, 0xff9638,
       0xdbab00, 0xa2ca20, 0x4adc4a, 0x2ccca4,
       0x1cc2ea, 0x585858, 0x101010, 0x101010,
       0xffffff, 0xb0d4ff, 0xc4c4ff, 0xe8b8ff,
       0xffb0ff, 0xffb8e8, 0xffc4c4, 0xffd4a8,
       0xffe890, 0xf0f4a4, 0xc0ffc0, 0xacf4f0,
       0xa0e8ff, 0xc2c2c2, 0x202020, 0x101010 }
   },
   { "nintendo-vc", "Virtual Console palette",
      { 0x494949, 0x00006a, 0x090063, 0x290059,
        0x42004a, 0x490000, 0x420000, 0x291100,
        0x182700, 0x003010, 0x003000, 0x002910,
        0x012043, 0x000000, 0x000000, 0x000000,
        0x747174, 0x003084, 0x3101ac, 0x4b0194,
        0x64007b, 0x6b0039, 0x6b2101, 0x5a2f00,
        0x424900, 0x185901, 0x105901, 0x015932,
        0x01495a, 0x101010, 0x000000, 0x000000,
        0xadadad, 0x4a71b6, 0x6458d5, 0x8450e6,
        0xa451ad, 0xad4984, 0xb5624a, 0x947132,
        0x7b722a, 0x5a8601, 0x388e31, 0x318e5a,
        0x398e8d, 0x383838, 0x000000, 0x000000,
        0xb6b6b6, 0x8c9db5, 0x8d8eae, 0x9c8ebc,
        0xa687bc, 0xad8d9d, 0xae968c, 0x9c8f7c,
        0x9c9e72, 0x94a67c, 0x84a77b, 0x7c9d84,
        0x73968d, 0xdedede, 0x000000, 0x000000 }
   },
   { "rgb", "Nintendo RGB PPU palette",
      { 0x6D6D6D, 0x002492, 0x0000DB, 0x6D49DB,
        0x92006D, 0xB6006D, 0xB62400, 0x924900,
        0x6D4900, 0x244900, 0x006D24, 0x009200,
        0x004949, 0x000000, 0x000000, 0x000000,
        0xB6B6B6, 0x006DDB, 0x0049FF, 0x9200FF,
        0xB600FF, 0xFF0092, 0xFF0000, 0xDB6D00,
        0x926D00, 0x249200, 0x009200, 0x00B66D,
        0x009292, 0x242424, 0x000000, 0x000000,
        0xFFFFFF, 0x6DB6FF, 0x9292FF, 0xDB6DFF,
        0xFF00FF, 0xFF6DFF, 0xFF9200, 0xFFB600,
        0xDBDB00, 0x6DDB00, 0x00FF00, 0x49FFDB,
        0x00FFFF, 0x494949, 0x000000, 0x000000,
        0xFFFFFF, 0xB6DBFF, 0xDBB6FF, 0xFFB6FF,
        0xFF92FF, 0xFFB6B6, 0xFFDB92, 0xFFFF49,
        0xFFFF6D, 0xB6FF49, 0x92FF6D, 0x49FFDB,
        0x92DBFF, 0x929292, 0x000000, 0x000000 }
   },
   { "yuv-v3", "FBX's YUV-V3 palette",
      { 0x666666, 0x002A88, 0x1412A7, 0x3B00A4,
        0x5C007E, 0x6E0040, 0x6C0700, 0x561D00,
        0x333500, 0x0C4800, 0x005200, 0x004C18,
        0x003E5B, 0x000000, 0x000000, 0x000000,
        0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE,
        0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
        0x6B6D00, 0x388700, 0x0D9300, 0x008C47,
        0x007AA0, 0x000000, 0x000000, 0x000000,
        0xFFFFFF, 0x64B0FF, 0x9290FF, 0xC676FF,
        0xF26AFF, 0xFF6ECC, 0xFF8170, 0xEA9E22,
        0xBCBE00, 0x88D800, 0x5CE430, 0x45E082,
        0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
        0xFFFFFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF,
        0xFAC2FF, 0xFFC4EA, 0xFFCCC5, 0xF7D8A5,
        0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC,
        0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000 }
   },
   { "unsaturated-final", "FBX's Unsaturated-Final palette",
      { 0x676767, 0x001F8E, 0x23069E, 0x40008E,
        0x600067, 0x67001C, 0x5B1000, 0x432500,
        0x313400, 0x074800, 0x004F00, 0x004622,
        0x003A61, 0x000000, 0x000000, 0x000000,
        0xB3B3B3, 0x205ADF, 0x5138FB, 0x7A27EE,
        0xA520C2, 0xB0226B, 0xAD3702, 0x8D5600,
        0x6E7000, 0x2E8A00, 0x069200, 0x008A47,
        0x037B9B, 0x101010, 0x000000, 0x000000,
        0xFFFFFF, 0x62AEFF, 0x918BFF, 0xBC78FF,
        0xE96EFF, 0xFC6CCD, 0xFA8267, 0xE29B26,
        0xC0B901, 0x84D200, 0x58DE38, 0x46D97D,
        0x49CED2, 0x494949, 0x000000, 0x000000,
        0xFFFFFF, 0xC1E3FF, 0xD5D4FF, 0xE7CCFF,
        0xFBC9FF, 0xFFC7F0, 0xFFD0C5, 0xF8DAAA,
        0xEBE69A, 0xD1F19A, 0xBEF7AF, 0xB6F4CD,
        0xB7F0EF, 0xB2B2B2, 0x000000, 0x000000 }
   },
   { "sony-cxa2025as-us", "Sony CXA2025AS US palette",
      { 0x585858, 0x00238C, 0x00139B, 0x2D0585,
        0x5D0052, 0x7A0017, 0x7A0800, 0x5F1800,
        0x352A00, 0x093900, 0x003F00, 0x003C22,
        0x00325D, 0x000000, 0x000000, 0x000000,
        0xA1A1A1, 0x0053EE, 0x153CFE, 0x6028E4,
        0xA91D98, 0xD41E41, 0xD22C00, 0xAA4400,
        0x6C5E00, 0x2D7300, 0x007D06, 0x007852,
        0x0069A9, 0x000000, 0x000000, 0x000000,
        0xFFFFFF, 0x1FA5FE, 0x5E89FE, 0xB572FE,
        0xFE65F6, 0xFE6790, 0xFE773C, 0xFE9308,
        0xC4B200, 0x79CA10, 0x3AD54A, 0x11D1A4,
        0x06BFFE, 0x424242, 0x000000, 0x000000,
        0xFFFFFF, 0xA0D9FE, 0xBDCCFE, 0xE1C2FE,
        0xFEBCFB, 0xFEBDD0, 0xFEC5A9, 0xFED18E,
        0xE9DE86, 0xC7E992, 0xA8EEB0, 0x95ECD9,
        0x91E4FE, 0xACACAC, 0x000000, 0x000000 }
   },
   { "pal", "PAL palette",
      { 0x808080, 0x0000BA, 0x3700BF, 0x8400A6,
        0xBB006A, 0xB7001E, 0xB30000, 0x912600,
        0x7B2B00, 0x003E00, 0x00480D, 0x003C22,
        0x002F66, 0x000000, 0x050505, 0x050505,
        0xC8C8C8, 0x0059FF, 0x443CFF, 0xB733CC,
        0xFE33AA, 0xFE375E, 0xFE371A, 0xD54B00,
        0xC46200, 0x3C7B00, 0x1D8415, 0x009566,
        0x0084C4, 0x111111, 0x090909, 0x090909,
        0xFEFEFE, 0x0095FF, 0x6F84FF, 0xD56FFF,
        0xFE77CC, 0xFE6F99, 0xFE7B59, 0xFE915F,
        0xFEA233, 0xA6BF00, 0x51D96A, 0x4DD5AE,
        0x00D9FF, 0x666666, 0x0D0D0D, 0x0D0D0D,
        0xFEFEFE, 0x84BFFF, 0xBBBBFF, 0xD0BBFF,
        0xFEBFEA, 0xFEBFCC, 0xFEC4B7, 0xFECCAE,
        0xFED9A2, 0xCCE199, 0xAEEEB7, 0xAAF8EE,
        0xB3EEFF, 0xDDDDDD, 0x111111, 0x111111 }
   },
   { "bmf-final2", "BMF's Final 2 palette",
      { 0x525252, 0x000080, 0x08008A, 0x2C007E,
        0x4A004E, 0x500006, 0x440000, 0x260800,
        0x0A2000, 0x002E00, 0x003200, 0x00260A,
        0x001C48, 0x000000, 0x000000, 0x000000,
        0xA4A4A4, 0x0038CE, 0x3416EC, 0x5E04DC,
        0x8C00B0, 0x9A004C, 0x901800, 0x703600,
        0x4C5400, 0x0E6C00, 0x007400, 0x006C2C,
        0x005E84, 0x000000, 0x000000, 0x000000,
        0xFFFFFF, 0x4C9CFF, 0x7C78FF, 0xA664FF,
        0xDA5AFF, 0xF054C0, 0xF06A56, 0xD68610,
        0xBAA400, 0x76C000, 0x46CC1A, 0x2EC866,
        0x34C2BE, 0x3A3A3A, 0x000000, 0x000000,
        0xFFFFFF, 0xB6DAFF, 0xC8CAFF, 0xDAC2FF,
        0xF0BEFF, 0xFCBCEE, 0xFAC2C0, 0xF2CCA2,
        0xE6DA92, 0xCCE68E, 0xB8EEA2, 0xAEEABE,
        0xAEE8E2, 0xB0B0B0, 0x000000, 0x000000 }
   },
   { "bmf-final3", "BMF's Final 3 palette",
      { 0x686868, 0x001299, 0x1A08AA, 0x51029A,
        0x7E0069, 0x8E001C, 0x7E0301, 0x511800,
        0x1F3700, 0x014E00, 0x005A00, 0x00501C,
        0x004061, 0x000000, 0x000000, 0x000000,
        0xB9B9B9, 0x0C5CD7, 0x5035F0, 0x8919E0,
        0xBB0CB3, 0xCE0C61, 0xC02B0E, 0x954D01,
        0x616F00, 0x1F8B00, 0x01980C, 0x00934B,
        0x00819B, 0x000000, 0x000000, 0x000000,
        0xFFFFFF, 0x63B4FF, 0x9B91FF, 0xD377FF,
        0xEF6AFF, 0xF968C0, 0xF97D6C, 0xED9B2D,
        0xBDBD16, 0x7CDA1C, 0x4BE847, 0x35E591,
        0x3FD9DD, 0x606060, 0x000000, 0x000000,
        0xFFFFFF, 0xACE7FF, 0xD5CDFF, 0xEDBAFF,
        0xF8B0FF, 0xFEB0EC, 0xFDBDB5, 0xF9D28E,
        0xE8EB7C, 0xBBF382, 0x99F7A2, 0x8AF5D0,
        0x92F4F1, 0xBEBEBE, 0x000000, 0x000000 }
   },
   { "smooth-fbx", "FBX's Smooth palette",
      { 0x6A6D6A, 0x001380, 0x1E008A, 0x39007A,
        0x550056, 0x5A0018, 0x4F1000, 0x3D1C00,
        0x253200, 0x003D00, 0x004000, 0x003924,
        0x002E55, 0x000000, 0x000000, 0x000000,
        0xB9BCB9, 0x1850C7, 0x4B30E3, 0x7322D6,
        0x951FA9, 0x9D285C, 0x983700, 0x7F4C00,
        0x5E6400, 0x227700, 0x027E02, 0x007645,
        0x006E8A, 0x000000, 0x000000, 0x000000,
        0xFFFFFF, 0x68A6FF, 0x8C9CFF, 0xB586FF,
        0xD975FD, 0xE377B9, 0xE58D68, 0xD49D29,
        0xB3AF0C, 0x7BC211, 0x55CA47, 0x46CB81,
        0x47C1C5, 0x4A4D4A, 0x000000, 0x000000,
        0xFFFFFF, 0xCCEAFF, 0xDDDEFF, 0xECDAFF,
        0xF8D7FE, 0xFCD6F5, 0xFDDBCF, 0xF9E7B5,
        0xF1F0AA, 0xDAFAA9, 0xC9FFBC, 0xC3FBD7,
        0xC4F6F6, 0xBEC1BE, 0x000000, 0x000000 }
   },
   { "composite-direct-fbx", "FBX's Composite Direct palette",
      { 0x656565, 0x00127D, 0x18008E, 0x360082,
        0x56005D, 0x5A0018, 0x4F0500, 0x381900,
        0x1D3100, 0x003D00, 0x004100, 0x003B17,
        0x002E55, 0x000000, 0x000000, 0x000000,
        0xAFAFAF, 0x194EC8, 0x472FE3, 0x6B1FD7,
        0x931BAE, 0x9E1A5E, 0x993200, 0x7B4B00,
        0x5B6700, 0x267A00, 0x008200, 0x007A3E,
        0x006E8A, 0x000000, 0x000000, 0x000000,
        0xFFFFFF, 0x64A9FF, 0x8E89FF, 0xB676FF,
        0xE06FFF, 0xEF6CC4, 0xF0806A, 0xD8982C,
        0xB9B40A, 0x83CB0C, 0x5BD63F, 0x4AD17E,
        0x4DC7CB, 0x4C4C4C, 0x000000, 0x000000,
        0xFFFFFF, 0xC7E5FF, 0xD9D9FF, 0xE9D1FF,
        0xF9CEFF, 0xFFCCF1, 0xFFD4CB, 0xF8DFB1,
        0xEDEAA4, 0xD6F4A4, 0xC5F8B8, 0xBEF6D3,
        0xBFF1F1, 0xB9B9B9, 0x000000, 0x000000 }
   },
   { "pvm-style-d93-fbx", "FBX's PVM Style D93 palette",
      { 0x696B63, 0x001774, 0x1E0087, 0x340073,
        0x560057, 0x5E0013, 0x531A00, 0x3B2400,
        0x243000, 0x063A00, 0x003F00, 0x003B1E,
        0x00334E, 0x000000, 0x000000, 0x000000,
        0xB9BBB3, 0x1453B9, 0x4D2CDA, 0x671EDE,
        0x98189C, 0x9D2344, 0xA03E00, 0x8D5500,
        0x656D00, 0x2C7900, 0x008100, 0x007D42,
        0x00788A, 0x000000, 0x000000, 0x000000,
        0xFFFFFF, 0x69A8FF, 0x9691FF, 0xB28AFA,
        0xEA7DFA, 0xF37BC7, 0xF28E59, 0xE6AD27,
        0xD7C805, 0x90DF07, 0x64E53C, 0x45E27D,
        0x48D5D9, 0x4E5048, 0x000000, 0x000000,
        0xFFFFFF, 0xD2EAFF, 0xE2E2FF, 0xE9D8FF,
        0xF5D2FF, 0xF8D9EA, 0xFADEB9, 0xF9E89B,
        0xF3F28C, 0xD3FA91, 0xB8FCA8, 0xAEFACA,
        0xCAF3F3, 0xBEC0B8, 0x000000, 0x000000 }
   },
   { "ntsc-hardware-fbx", "FBX's NTSC Hardware palette",
      { 0x6A6D6A, 0x001380, 0x1E008A, 0x39007A,
        0x550056, 0x5A0018, 0x4F1000, 0x382100,
        0x213300, 0x003D00, 0x004000, 0x003924,
        0x002E55, 0x000000, 0x000000, 0x000000,
        0xB9BCB9, 0x1850C7, 0x4B30E3, 0x7322D6,
        0x951FA9, 0x9D285C, 0x963C00, 0x7A5100,
        0x5B6700, 0x227700, 0x027E02, 0x007645,
        0x006E8A, 0x000000, 0x000000, 0x000000,
        0xFFFFFF, 0x68A6FF, 0x9299FF, 0xB085FF,
        0xD975FD, 0xE377B9, 0xE58D68, 0xCFA22C,
        0xB3AF0C, 0x7BC211, 0x55CA47, 0x46CB81,
        0x47C1C5, 0x4A4D4A, 0x000000, 0x000000,
        0xFFFFFF, 0xCCEAFF, 0xDDDEFF, 0xECDAFF,
        0xF8D7FE, 0xFCD6F5, 0xFDDBCF, 0xF9E7B5,
        0xF1F0AA, 0xDAFAA9, 0xC9FFBC, 0xC3FBD7,
        0xC4F6F6, 0xBEC1BE, 0x000000, 0x000000 }
   },
   { "nes-classic-fbx-fs", "FBX's NES-Classic FS palette",
      { 0x60615F, 0x000083, 0x1D0195, 0x340875,
        0x51055E, 0x56000F, 0x4C0700, 0x372308,
        0x203A0B, 0x0F4B0E, 0x194C16, 0x02421E,
        0x023154, 0x000000, 0x000000, 0x000000,
        0xA9AAA8, 0x104BBF, 0x4712D8, 0x6300CA,
        0x8800A9, 0x930B46, 0x8A2D04, 0x6F5206,
        0x5C7114, 0x1B8D12, 0x199509, 0x178448,
        0x206B8E, 0x000000, 0x000000, 0x000000,
        0xFBFBFB, 0x6699F8, 0x8974F9, 0xAB58F8,
        0xD557EF, 0xDE5FA9, 0xDC7F59, 0xC7A224,
        0xA7BE03, 0x75D703, 0x60E34F, 0x3CD68D,
        0x56C9CC, 0x414240, 0x000000, 0x000000,
        0xFBFBFB, 0xBED4FA, 0xC9C7F9, 0xD7BEFA,
        0xE8B8F9, 0xF5BAE5, 0xF3CAC2, 0xDFCDA7,
        0xD9E09C, 0xC9EB9E, 0xC0EDB8, 0xB5F4C7,
        0xB9EAE9, 0xABABAB, 0x000000, 0x000000 }
   },
   { "nescap", "RGBSource's NESCAP palette",
      { 0x646365, 0x001580, 0x1D0090, 0x380082,
        0x56005D, 0x5A001A, 0x4F0900, 0x381B00,
        0x1E3100, 0x003D00, 0x004100, 0x003A1B,
        0x002F55, 0x000000, 0x000000, 0x000000,
        0xAFADAF, 0x164BCA, 0x472AE7, 0x6B1BDB,
        0x9617B0, 0x9F185B, 0x963001, 0x7B4800,
        0x5A6600, 0x237800, 0x017F00, 0x00783D,
        0x006C8C, 0x000000, 0x000000, 0x000000,
        0xFFFFFF, 0x60A6FF, 0x8F84FF, 0xB473FF,
        0xE26CFF, 0xF268C3, 0xEF7E61, 0xD89527,
        0xBAB307, 0x81C807, 0x57D43D, 0x47CF7E,
        0x4BC5CD, 0x4C4B4D, 0x000000, 0x000000,
        0xFFFFFF, 0xC2E0FF, 0xD5D2FF, 0xE3CBFF,
        0xF7C8FF, 0xFEC6EE, 0xFECEC6, 0xF6D7AE,
        0xE9E49F, 0xD3ED9D, 0xC0F2B2, 0xB9F1CC,
        0xBAEDED, 0xBAB9BB, 0x000000, 0x000000 }
   },
   { "wavebeam", "nakedarthur's Wavebeam palette",
      { 0X6B6B6B, 0X001B88, 0X21009A, 0X40008C,
        0X600067, 0X64001E, 0X590800, 0X481600,
        0X283600, 0X004500, 0X004908, 0X00421D,
        0X003659, 0X000000, 0X000000, 0X000000,
        0XB4B4B4, 0X1555D3, 0X4337EF, 0X7425DF,
        0X9C19B9, 0XAC0F64, 0XAA2C00, 0X8A4B00,
        0X666B00, 0X218300, 0X008A00, 0X008144,
        0X007691, 0X000000, 0X000000, 0X000000,
        0XFFFFFF, 0X63B2FF, 0X7C9CFF, 0XC07DFE,
        0XE977FF, 0XF572CD, 0XF4886B, 0XDDA029,
        0XBDBD0A, 0X89D20E, 0X5CDE3E, 0X4BD886,
        0X4DCFD2, 0X525252, 0X000000, 0X000000,
        0XFFFFFF, 0XBCDFFF, 0XD2D2FF, 0XE1C8FF,
        0XEFC7FF, 0XFFC3E1, 0XFFCAC6, 0XF2DAAD,
        0XEBE3A0, 0XD2EDA2, 0XBCF4B4, 0XB5F1CE,
        0XB6ECF1, 0XBFBFBF, 0X000000, 0X000000 }
   }
};

void updatePalette(PALETTES palette) {
    for (uint8_t i = 0; i < 64; i++) {
        if (palette == RGB333) {
            graphics_set_palette(i, extra_palettes[settings.nes_palette].data[i]);
        }
        else {
            uint32_t c = extra_palettes[settings.nes_palette].data[i];
            uint8_t r = (c >> (16 + 6)) & 0x3;
            uint8_t g = (c >> (8 + 6)) & 0x3;
            uint8_t b = (c >> (0 + 6)) & 0x3;
            r *= 42 * 2;
            g *= 42 * 2;
            b *= 42 * 2;
            graphics_set_palette(i, RGB888(r, g, b));
        }
    }
}

struct input_bits_t {
    bool a: true;
    bool b: true;
    bool select: true;
    bool start: true;
    bool right: true;
    bool left: true;
    bool up: true;
    bool down: true;
};

input_bits_t keyboard_bits = { false, false, false, false, false, false, false, false };
input_bits_t gamepad1_bits = { false, false, false, false, false, false, false, false };
static input_bits_t gamepad2_bits = { false, false, false, false, false, false, false, false };

#if USE_NESPAD

void nespad_tick() {
    nespad_read();

    gamepad1_bits.a = (nespad_state & DPAD_A) != 0;
    gamepad1_bits.b = (nespad_state & DPAD_B) != 0;
    gamepad1_bits.select = (nespad_state & DPAD_SELECT) != 0;
    gamepad1_bits.start = (nespad_state & DPAD_START) != 0;
    gamepad1_bits.up = (nespad_state & DPAD_UP) != 0;
    gamepad1_bits.down = (nespad_state & DPAD_DOWN) != 0;
    gamepad1_bits.left = (nespad_state & DPAD_LEFT) != 0;
    gamepad1_bits.right = (nespad_state & DPAD_RIGHT) != 0;

    // second
    gamepad2_bits.a = (nespad_state2 & DPAD_A) != 0;
    gamepad2_bits.b = (nespad_state2 & DPAD_B) != 0;
    gamepad2_bits.select = (nespad_state2 & DPAD_SELECT) != 0;
    gamepad2_bits.start = (nespad_state2 & DPAD_START) != 0;
    gamepad2_bits.up = (nespad_state2 & DPAD_UP) != 0;
    gamepad2_bits.down = (nespad_state2 & DPAD_DOWN) != 0;
    gamepad2_bits.left = (nespad_state2 & DPAD_LEFT) != 0;
    gamepad2_bits.right = (nespad_state2 & DPAD_RIGHT) != 0;
}
#endif

#if USE_PS2_KBD
static bool isInReport(hid_keyboard_report_t const* report, const unsigned char keycode) {
    for (unsigned char i: report->keycode) {
        if (i == keycode) {
            return true;
        }
    }
    return false;
}

void __not_in_flash_func(process_kbd_report)(hid_keyboard_report_t const* report,
                                             hid_keyboard_report_t const* prev_report) {
    keyboard_bits.start = isInReport(report, HID_KEY_ENTER);
    keyboard_bits.select = isInReport(report, HID_KEY_BACKSPACE);
    keyboard_bits.a = isInReport(report, HID_KEY_Z);
    keyboard_bits.b = isInReport(report, HID_KEY_X);
    keyboard_bits.up = isInReport(report, HID_KEY_ARROW_UP);
    keyboard_bits.down = isInReport(report, HID_KEY_ARROW_DOWN);
    keyboard_bits.left = isInReport(report, HID_KEY_ARROW_LEFT);
    keyboard_bits.right = isInReport(report, HID_KEY_ARROW_RIGHT);
}

Ps2Kbd_Mrmltr ps2kbd(
    pio1,
    0,
    process_kbd_report);
#endif

inline bool checkNESMagic(const uint8_t* data) {
    bool ok = false;
    int nIdx;
    if (memcmp(data, "NES\x1a", 4) == 0) {
        uint8_t MapperNo = *(data + 6) >> 4;
        for (nIdx = 0; MapperTable[nIdx].nMapperNo != -1; ++nIdx) {
            if (MapperTable[nIdx].nMapperNo == MapperNo) {
                ok = true;
                break;
            }
        }
    }
    return ok;
}

static int rapidFireMask = 0;
static int rapidFireMask2 = 0;
static int rapidFireCounter = 0;
static int rapidFireCounter2 = 0;

void InfoNES_PadState(DWORD* pdwPad1, DWORD* pdwPad2, DWORD* pdwSystem) {
    static constexpr int LEFT = 1 << 6;
    static constexpr int RIGHT = 1 << 7;
    static constexpr int UP = 1 << 4;
    static constexpr int DOWN = 1 << 5;
    static constexpr int SELECT = 1 << 2;
    static constexpr int START = 1 << 3;
    static constexpr int A = 1 << 0;
    static constexpr int B = 1 << 1;
    input_bits_t player1_state = {};
    input_bits_t player2_state = {};
    switch (settings.player_1_input) {
        case 0:
            player1_state = keyboard_bits;
            break;
        case 1:
            player1_state = gamepad1_bits;
            break;
        case 2:
            player1_state = gamepad2_bits;
            break;
    }
    switch (settings.player_2_input) {
        case 0:
            player2_state = keyboard_bits;
            break;
        case 1:
            player2_state = gamepad1_bits;
            break;
        case 2:
            player2_state = gamepad2_bits;
            break;
    }
    int gamepad_state = (player1_state.left ? LEFT : 0) |
                        (player1_state.right ? RIGHT : 0) |
                        (player1_state.up ? UP : 0) |
                        (player1_state.down ? DOWN : 0) |
                        (player1_state.start ? START : 0) |
                        (player1_state.select ? SELECT : 0) |
                        (player1_state.a ? A : 0) |
                        (player1_state.b ? B : 0) |
                        0;
    ++rapidFireCounter;
    bool reset = false;
    auto&dst = *pdwPad1;
    int rv = gamepad_state;
    if (rapidFireCounter & 2) {
        // 15 fire/sec
        rv &= ~rapidFireMask;
    }
    dst = rv;
    gamepad_state = (player2_state.left ? LEFT : 0) |
                    (player2_state.right ? RIGHT : 0) |
                    (player2_state.up ? UP : 0) |
                    (player2_state.down ? DOWN : 0) |
                    (player2_state.start ? START : 0) |
                    (player2_state.select ? SELECT : 0) |
                    (player2_state.a ? A : 0) |
                    (player2_state.b ? B : 0) |
                    0;
    ++rapidFireCounter2;
    auto&dst2 = *pdwPad2;
    rv = gamepad_state;
    if (rapidFireCounter2 & 2) {
        // 15 fire/sec
        rv &= ~rapidFireMask2;
    }
    dst2 = rv;
    *pdwSystem = reset ? PAD_SYS_QUIT : 0;
}

void InfoNES_MessageBox(const char* pszMsg, ...) {
    printf("[MSG]");
    va_list args;
    va_start(args, pszMsg);
    vprintf(pszMsg, args);
    va_end(args);
    printf("\n");
}

void InfoNES_Error(const char* pszMsg, ...) {
    printf("[Error]");
    va_list args;
    va_start(args, pszMsg);
    // vsnprintf(ErrorMessage, ERRORMESSAGESIZE, pszMsg, args);
    // printf("%s", ErrorMessage);
    va_end(args);
    printf("\n");
}

bool parseROM(const uint8_t* nesFile) {
    memcpy(&NesHeader, nesFile, sizeof(NesHeader));
    if (!checkNESMagic(NesHeader.byID)) {
        return false;
    }
    nesFile += sizeof(NesHeader);
    memset(SRAM, 0, SRAM_SIZE);
    if (NesHeader.byInfo1 & 4) {
        memcpy(&SRAM[0x1000], nesFile, 512);
        nesFile += 512;
    }
    auto romSize = NesHeader.byRomSize * 0x4000;
    ROM = (BYTE *)nesFile;
    nesFile += romSize;
    if (NesHeader.byVRomSize > 0) {
        auto vromSize = NesHeader.byVRomSize * 0x2000;
        VROM = (BYTE *)nesFile;
        nesFile += vromSize;
    }
    return true;
}

void InfoNES_ReleaseRom() {
    ROM = nullptr;
    VROM = nullptr;
}

void InfoNES_SoundInit() {
    i2s_config = i2s_get_default_config();
    i2s_config.sample_freq = 44100;
    i2s_config.dma_trans_count = (uint16_t)i2s_config.sample_freq / 60;
    i2s_volume(&i2s_config, 0);
    i2s_init(&i2s_config);
}

int InfoNES_SoundOpen(int samples_per_sync, int sample_rate) {
    return 0;
}

void InfoNES_SoundClose() {
}

#define buffermax ((44100 / 60)*2)
int __not_in_flash_func(InfoNES_GetSoundBufferSize)() { return buffermax; }


void InfoNES_SoundOutput(int samples, const BYTE* wave1, const BYTE* wave2, const BYTE* wave3, const BYTE* wave4,
                         const BYTE* wave5) {
    static int16_t samples_out[2][buffermax * 2];
    static int i_active_buf = 0;
    static int inx = 0;
    static int max = 0;
    static int min = 30000;
    for (int i = 0; i < samples; i++) {
        int r, l;
        int w1 = *wave1++;
        int w2 = *wave2++;
        int w3 = *wave3++;
        int w4 = *wave4++;
        int w5 = *wave5++;
        l = w1 * 6 + w2 * 3 + w3 * 5 + w4 * 3 * 17 + w5 * 2 * 32;
        r = w1 * 3 + w2 * 6 + w3 * 5 + w4 * 3 * 17 + w5 * 2 * 32;
        max = MAX(l, max);
        min = MIN(l, min);
        l -= 4000;
        r -= 4000;
        samples_out[i_active_buf][inx * 2] = (int16_t)l * settings.snd_vol;
        samples_out[i_active_buf][inx * 2 + 1] = (int16_t)r * settings.snd_vol;
        if (inx++ >= i2s_config.dma_trans_count) {
            inx = 0;
            i2s_dma_write(&i2s_config, reinterpret_cast<const int16_t *>(samples_out[i_active_buf]));
            i_active_buf ^= 1;
        }
    }
}

void __not_in_flash_func(InfoNES_PreDrawLine)(int line) {
    InfoNES_SetLineBuffer(linebuffer, NES_DISP_WIDTH);
}

void __not_in_flash_func(InfoNES_PostDrawLine)(int line) {
    for (int x = 0; x < NES_DISP_WIDTH; x++) SCREEN[line][x] = (uint8_t)linebuffer[x];
    // if (settings.show_fps && line < 16) draw_fps(fps_text, line, 255);
}

/* Renderer loop on Pico's second core */
void __scratch_x("render") render_core() {
    multicore_lockout_victim_init();
    graphics_init();

    auto* buffer = &SCREEN[0][0];
    graphics_set_buffer(buffer, NES_DISP_WIDTH, NES_DISP_HEIGHT);
    graphics_set_textbuffer(buffer);
    graphics_set_bgcolor(0x000000);
    // graphics_set_offset(0, 0);
    graphics_set_offset(32, 0);

    updatePalette(settings.palette);
    graphics_set_flashmode(settings.flash_line, settings.flash_frame);
    sem_acquire_blocking(&vga_start_semaphore);

    // 60 FPS loop
#define frame_tick (16666)
    uint64_t tick = time_us_64();
#ifdef TFT
    uint64_t last_renderer_tick = tick;
#endif
    uint64_t last_input_tick = tick;
    while (true) {
#ifdef TFT
        if (tick >= last_renderer_tick + frame_tick) {
            refresh_lcd();
            last_renderer_tick = tick;
        }
#endif
        if (tick >= last_input_tick + frame_tick * 1) {
            ps2kbd.tick();
            nespad_tick();
            last_input_tick = tick;
        }
        tick = time_us_64();


        tuh_task();
        //hid_app_task();
        tight_loop_contents();
    }

    __unreachable();
}


bool filebrowser_loadfile(const char pathname[256]) {
    UINT bytes_read = 0;
    FIL file;

    constexpr int window_y = (TEXTMODE_ROWS - 5) / 2;
    constexpr int window_x = (TEXTMODE_COLS - 43) / 2;

    draw_window("Loading firmware", window_x, window_y, 43, 5);

    FILINFO fileinfo;
    f_stat(pathname, &fileinfo);

    if (16384 - 64 << 10 < fileinfo.fsize) {
        draw_text("ERROR: ROM too large! Canceled!!", window_x + 1, window_y + 2, 13, 1);
        sleep_ms(5000);
        return false;
    }


    draw_text("Loading...", window_x + 1, window_y + 2, 10, 1);
    sleep_ms(500);


    multicore_lockout_start_blocking();
    auto flash_target_offset = FLASH_TARGET_OFFSET + 4096;

    flash_range_erase(flash_target_offset, fileinfo.fsize);

    if (FR_OK == f_open(&file, pathname, FA_READ)) {
        uint8_t buffer[FLASH_PAGE_SIZE];

        do {
            f_read(&file, &buffer, FLASH_PAGE_SIZE, &bytes_read);

            if (bytes_read) {
                uint32_t ints = save_and_disable_interrupts();
                flash_range_program(flash_target_offset, buffer, FLASH_PAGE_SIZE);
                restore_interrupts(ints);

                gpio_put(PICO_DEFAULT_LED_PIN, flash_target_offset >> 13 & 1);

                flash_target_offset += FLASH_PAGE_SIZE;
            }
        }
        while (bytes_read != 0);

        gpio_put(PICO_DEFAULT_LED_PIN, true);
    }
    f_close(&file);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(FLASH_TARGET_OFFSET, reinterpret_cast<const uint8_t *>(pathname), 256);
    restore_interrupts(ints);

    multicore_lockout_end_blocking();
    return true;
}


typedef struct __attribute__((__packed__)) {
    bool is_directory;
    bool is_executable;
    size_t size;
    char filename[79];
} file_item_t;

constexpr int max_files = 600;
file_item_t* fileItems = (file_item_t *)(&SCREEN[0][0] + TEXTMODE_COLS * TEXTMODE_ROWS * 2);

int compareFileItems(const void* a, const void* b) {
    const auto* itemA = (file_item_t *)a;
    const auto* itemB = (file_item_t *)b;
    // Directories come first
    if (itemA->is_directory && !itemB->is_directory)
        return -1;
    if (!itemA->is_directory && itemB->is_directory)
        return 1;
    // Sort files alphabetically
    return strcmp(itemA->filename, itemB->filename);
}

static inline bool isExecutable(const char pathname[256], const char* extensions) {
    const char* extension = strrchr(pathname, '.');
    if (extension == nullptr) {
        return false;
    }
    extension++; // Move past the '.' character

    const char* token = strtok((char *)extensions, "|"); // Tokenize the extensions string using '|'

    while (token != nullptr) {
        if (strcmp(extension, token) == 0) {
            return true;
        }
        token = strtok(NULL, "|");
    }

    return false;
}

void filebrowser(const char pathname[256], const char* executables) {
    bool debounce = true;
    char basepath[256];
    char tmp[TEXTMODE_COLS + 1];
    strcpy(basepath, pathname);
    constexpr int per_page = TEXTMODE_ROWS - 3;

    DIR dir;
    FILINFO fileInfo;

    if (FR_OK != f_mount(&fs, "SD", 1)) {
        draw_text("SD Card not inserted or SD Card error!", 0, 0, 12, 0);
        while (true);
    }

    while (true) {
        memset(fileItems, 0, sizeof(file_item_t) * max_files);
        int total_files = 0;

        snprintf(tmp, TEXTMODE_COLS, "SD:\\%s", basepath);
        draw_window(tmp, 0, 0, TEXTMODE_COLS, TEXTMODE_ROWS - 1);
        memset(tmp, ' ', TEXTMODE_COLS);

#ifndef TFT
        draw_text(tmp, 0, 29, 0, 0);
        auto off = 0;
        draw_text("START", off, 29, 7, 0);
        off += 5;
        draw_text(" Run at cursor ", off, 29, 0, 3);
        off += 16;
        draw_text("SELECT", off, 29, 7, 0);
        off += 6;
        draw_text(" Run previous  ", off, 29, 0, 3);
        off += 16;
        draw_text("ARROWS", off, 29, 7, 0);
        off += 6;
        draw_text(" Navigation    ", off, 29, 0, 3);
        off += 16;
        draw_text("A/F10", off, 29, 7, 0);
        off += 5;
        draw_text(" USB DRV ", off, 29, 0, 3);
#endif

        if (FR_OK != f_opendir(&dir, basepath)) {
            draw_text("Failed to open directory", 1, 1, 4, 0);
            while (true);
        }

        if (strlen(basepath) > 0) {
            strcpy(fileItems[total_files].filename, "..\0");
            fileItems[total_files].is_directory = true;
            fileItems[total_files].size = 0;
            total_files++;
        }

        while (f_readdir(&dir, &fileInfo) == FR_OK &&
               fileInfo.fname[0] != '\0' &&
               total_files < max_files
        ) {
            // Set the file item properties
            fileItems[total_files].is_directory = fileInfo.fattrib & AM_DIR;
            fileItems[total_files].size = fileInfo.fsize;
            fileItems[total_files].is_executable = isExecutable(fileInfo.fname, executables);
            strncpy(fileItems[total_files].filename, fileInfo.fname, 78);
            total_files++;
        }
        f_closedir(&dir);

        qsort(fileItems, total_files, sizeof(file_item_t), compareFileItems);

        if (total_files > max_files) {
            draw_text(" Too many files!! ", TEXTMODE_COLS - 17, 0, 12, 3);
        }

        int offset = 0;
        int current_item = 0;

        while (true) {
            sleep_ms(100);

            if (!debounce) {
                debounce = !gamepad1_bits.start && !keyboard_bits.start;
            }

            // ESCAPE
            if (gamepad1_bits.select || keyboard_bits.select) {
                return;
            }

            /*
            // F10
            if (nespad_state & DPAD_A || input == 0x44) {
                constexpr int window_x = (TEXTMODE_COLS - 40) / 2;
                constexpr int window_y = (TEXTMODE_ROWS - 4) / 2;
                draw_window("SD Cardreader mode ", window_x, window_y, 40, 4);
                draw_text("Mounting SD Card. Use safe eject ", window_x + 1, window_y + 1, 13, 1);
                draw_text("to conitinue...", window_x + 1, window_y + 2, 13, 1);

                sleep_ms(500);

                init_pico_usb_drive();

                while (!tud_msc_ejected()) {
                    pico_usb_drive_heartbeat();
                }

                int post_cicles = 1000;
                while (--post_cicles) {
                    sleep_ms(1);
                    pico_usb_drive_heartbeat();
                }
            }
            */

            if (gamepad1_bits.down || keyboard_bits.down) {
                if (offset + (current_item + 1) < total_files) {
                    if (current_item + 1 < per_page) {
                        current_item++;
                    }
                    else {
                        offset++;
                    }
                }
            }

            if (gamepad1_bits.up || keyboard_bits.up) {
                if (current_item > 0) {
                    current_item--;
                }
                else if (offset > 0) {
                    offset--;
                }
            }

            if (gamepad1_bits.right || keyboard_bits.right) {
                offset += per_page;
                if (offset + (current_item + 1) > total_files) {
                    offset = total_files - (current_item + 1);
                }
            }

            if (gamepad1_bits.left || keyboard_bits.left) {
                if (offset > per_page) {
                    offset -= per_page;
                }
                else {
                    offset = 0;
                    current_item = 0;
                }
            }

            if (debounce && (gamepad1_bits.start || keyboard_bits.start)) {
                auto file_at_cursor = fileItems[offset + current_item];

                if (file_at_cursor.is_directory) {
                    if (strcmp(file_at_cursor.filename, "..") == 0) {
                        const char* lastBackslash = strrchr(basepath, '\\');
                        if (lastBackslash != nullptr) {
                            const size_t length = lastBackslash - basepath;
                            basepath[length] = '\0';
                        }
                    }
                    else {
                        sprintf(basepath, "%s\\%s", basepath, file_at_cursor.filename);
                    }
                    debounce = false;
                    break;
                }

                if (file_at_cursor.is_executable) {
                    sprintf(tmp, "%s\\%s", basepath, file_at_cursor.filename);

                    if (filebrowser_loadfile(tmp)) {
                        watchdog_enable(0, true);
                        // FIXME!!!
                        return;
                    }
                }
            }

            for (int i = 0; i < per_page; i++) {
                const auto item = fileItems[offset + i];
                uint8_t color = 11;
                uint8_t bg_color = 1;

                if (i == current_item) {
                    color = 0;
                    bg_color = 3;
                    memset(tmp, 0xCD, TEXTMODE_COLS - 2);
                    tmp[TEXTMODE_COLS - 2] = '\0';
                    draw_text(tmp, 1, per_page + 1, 11, 1);
                    snprintf(tmp, TEXTMODE_COLS - 2, " Size: %iKb, File %lu of %i ", item.size / 1024, offset + i + 1,
                             total_files);
                    draw_text(tmp, 2, per_page + 1, 14, 3);
                }

                const auto len = strlen(item.filename);
                color = item.is_directory ? 15 : color;
                color = item.is_executable ? 10 : color;
                //color = strstr((char *)rom_filename, item.filename) != nullptr ? 13 : color;
                memset(tmp, ' ', TEXTMODE_COLS - 2);
                tmp[TEXTMODE_COLS - 2] = '\0';
                memcpy(&tmp, item.filename, len < TEXTMODE_COLS - 2 ? len : TEXTMODE_COLS - 2);
                draw_text(tmp, 1, i + 1, color, bg_color);
            }
        }
    }
}

int menu();

int InfoNES_Video() {
    if (!parseROM(reinterpret_cast<const uint8_t *>(rom))) {
        menu();
        return 0; // TODO: 1?
    }
    if (InfoNES_Reset() < 0) {
        menu();
        return 1; // TODO: ?
    }
    graphics_set_mode(GRAPHICSMODE_DEFAULT);
    return 0;
}

int InfoNES_Menu() {
    graphics_set_mode(TEXTMODE_DEFAULT);
    filebrowser(HOME_DIR, (char *)"nes");
    graphics_set_mode(GRAPHICSMODE_DEFAULT);
    return 1;
}

void load_config() {
    char pathname[256];
    sprintf(pathname, "%s\\emulator.cfg", "NES");
    FRESULT fr = f_mount(&fs, "", 1);
    FIL fd;
    fr = f_open(&fd, pathname, FA_READ);
    if (fr != FR_OK) {
        return;
    }
    UINT br;
    f_read(&fd, &settings, sizeof(settings), &br);
    f_close(&fd);
}

void save_config() {
    char pathname[256];
    sprintf(pathname, "%s\\emulator.cfg", "NES");
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        FIL fd;
        fr = f_open(&fd, pathname, FA_CREATE_ALWAYS | FA_WRITE);
        if (FR_OK != fr) {
            UINT bw;
            f_write(&fd, &settings, sizeof(settings), &bw);
            f_close(&fd);
        }
    }
}

typedef bool (*menu_callback_t)();

typedef struct __attribute__((__packed__)) {
    const char* text;
    menu_type_e type;
    const void* value;
    menu_callback_t callback;
    uint8_t max_value;
    char value_list[16][10];
} MenuItem;

#if SOFTTV
typedef struct tv_out_mode_t {
    // double color_freq;
    float color_index;
    COLOR_FREQ_t c_freq;
    enum graphics_mode_t mode_bpp;
    g_out_TV_t tv_system;
    NUM_TV_LINES_t N_lines;
    bool cb_sync_PI_shift_lines;
    bool cb_sync_PI_shift_half_frame;
} tv_out_mode_t;
extern tv_out_mode_t tv_out_mode;

bool color_mode=true;
bool toggle_color() {
    color_mode=!color_mode;
    if(color_mode) {
        tv_out_mode.color_index= 1.0f;
    } else {
        tv_out_mode.color_index= 0.0f;
    }

    return true;
}
#endif
int save_slot = 0;



bool save() {
    char pathname[255];

    if (save_slot) {
        sprintf(pathname, "%s_%d", rom_filename, save_slot);
    }
    else {
        sprintf(pathname, "%s", rom_filename);
    }

    save_state(pathname);
    return true;
}

bool load() {
    char pathname[255];

    if (save_slot) {
        sprintf(pathname, "%s_%d", rom_filename, save_slot);
    }
    else {
        sprintf(pathname, "%s", rom_filename);
    }

    load_state(pathname);
    return true;
}

const MenuItem menu_items[] = {
    { "Player 1: %s", ARRAY, &settings.player_1_input, nullptr, 2, { "Keyboard ", "Gamepad 1", "Gamepad 2" } },
    { "Player 2: %s", ARRAY, &settings.player_2_input, nullptr, 2, { "Keyboard ", "Gamepad 1", "Gamepad 2" } },
    { "" },
    { "Volume: %d", INT, &settings.snd_vol, nullptr, 8 },
#if VGA
{ "" },
    { "Flash line: %s", ARRAY, &settings.flash_line, nullptr,1, { "NO ", "YES" } },
    { "Flash frame: %s", ARRAY, &settings.flash_frame, nullptr,1, { "NO ", "YES" } },
    { "VGA Mode: %s", ARRAY, &settings.palette, nullptr,1, { "RGB333", "RGB222" } },
#endif
#if SOFTTV
    { "" },
    { "TV system %s", ARRAY, &tv_out_mode.tv_system, nullptr, 1, { "PAL ", "NTSC" } },
    { "TV Lines %s", ARRAY, &tv_out_mode.N_lines, nullptr, 3, { "624", "625", "524", "525" } },
    { "Freq %s", ARRAY, &tv_out_mode.c_freq, nullptr, 1, { "3.579545", "4.433619" } },
    { "Colors: %s", ARRAY, &color_mode, &toggle_color, 1, { "NO ", "YES" } },
    { "Shift lines %s", ARRAY, &tv_out_mode.cb_sync_PI_shift_lines, nullptr, 1, { "NO ", "YES" } },
    { "Shift half frame %s", ARRAY, &tv_out_mode.cb_sync_PI_shift_half_frame, nullptr, 1, { "NO ", "YES" } },
#endif
    { "" },
    { "NES Palette: %s", ARRAY, &settings.nes_palette, nullptr, 15,
        {
            "asqrealc",
            "nintendo",
            "rgb      ",
            "yuv-v3   ",
            "unsaturat",
            "cxa2025as",
            "pal      ",
            "bmf2     ",
            "bmf3     ",
            "smoothfbx",
            "composite",
            "pvm      ",
            "ntsc     ",
            "classic  ",
            "nescap   ",
            "wavebeam "
        }
    },
    { "" },
    { "Save state: %i", INT, &save_slot, &save, 5 },
    { "Load state: %i", INT, &save_slot, &load, 5 },
    { "" },
    { "Back to ROM select", ROM_SELECT },
    { "Return to game", RETURN },
};
#define MENU_ITEMS_NUMBER (sizeof(menu_items) / sizeof (MenuItem))

int menu() {
    bool exit = false;
    graphics_set_mode(TEXTMODE_DEFAULT);
    char footer[TEXTMODE_COLS];
    snprintf(footer, TEXTMODE_COLS, ":: %s ::", PICO_PROGRAM_NAME);
    draw_text(footer, TEXTMODE_COLS / 2 - strlen(footer) / 2, 0, 11, 1);
    snprintf(footer, TEXTMODE_COLS, ":: %s build %s %s ::", PICO_PROGRAM_VERSION_STRING, __DATE__,
             __TIME__);
    draw_text(footer, TEXTMODE_COLS / 2 - strlen(footer) / 2, TEXTMODE_ROWS - 1, 11, 1);
    int current_item = 0;
    while (!exit) {
        sleep_ms(25);
        if ((gamepad1_bits.down || keyboard_bits.down) != 0) {
            current_item = (current_item + 1) % MENU_ITEMS_NUMBER;
            if (menu_items[current_item].type == NONE)
                current_item++;
        }
        if ((gamepad1_bits.up || keyboard_bits.up) != 0) {
            current_item = (current_item - 1 + MENU_ITEMS_NUMBER) % MENU_ITEMS_NUMBER;
            if (menu_items[current_item].type == NONE)
                current_item--;
        }
        for (int i = 0; i < MENU_ITEMS_NUMBER; i++) {
            uint8_t y = i + ((TEXTMODE_ROWS - MENU_ITEMS_NUMBER) >> 1);
            uint8_t x = TEXTMODE_COLS / 2 - 10;
            uint8_t color = 0xFF;
            uint8_t bg_color = 0x00;
            if (current_item == i) {
                color = 0x01;
                bg_color = 0xFF;
            }
            const MenuItem* item = &menu_items[i];
            if (i == current_item) {
                switch (item->type) {
                    case INT:
                    case ARRAY:
                        if (item->max_value != 0) {
                            auto* value = (uint8_t *)item->value;
                            if ((gamepad1_bits.right || keyboard_bits.right) && *value < item->max_value) {
                                (*value)++;
                            }
                            if ((gamepad1_bits.left || keyboard_bits.left) && *value > 0) {
                                (*value)--;
                            }
                        }
                        break;
                    case SAVE:
                        if (gamepad1_bits.start || keyboard_bits.start) {
                            save_state((char *)rom_filename);
                            exit = true;
                        }
                        break;
                    case LOAD:
                        if (gamepad1_bits.start || keyboard_bits.start) {
                            load_state((char *)rom_filename);
                            exit = true;
                        }
                        break;
                    case RETURN:
                        if (gamepad1_bits.start || keyboard_bits.start)
                            exit = true;
                        break;
                    case RESET:
                        if (gamepad1_bits.start || keyboard_bits.start)
                            watchdog_enable(100, true);
                        break;

                    case ROM_SELECT:
                        if (gamepad1_bits.start || keyboard_bits.start) {
                            exit = true;
                            return InfoNES_Menu();
                        }
                        break;
                    case USB_DEVICE:
                        if (gamepad1_bits.start || keyboard_bits.start) {
                            clrScr(1);
                            draw_text((char *)"Mount me as USB drive...", 30, 15, 7, 1);
                            // in_flash_drive();
                            watchdog_enable(100, true);
                            exit = true;
                        }
                }
                if (nullptr != item->callback && (gamepad1_bits.start || keyboard_bits.start)) {
                    exit = item->callback();
                }
            }
            static char result[TEXTMODE_COLS];
            switch (item->type) {
                case INT:
                    snprintf(result, TEXTMODE_COLS, item->text, *(uint8_t *)item->value);
                    break;
                case ARRAY:
                    snprintf(result, TEXTMODE_COLS, item->text, item->value_list[*(uint8_t *)item->value]);
                    break;
                case TEXT:
                    snprintf(result, TEXTMODE_COLS, item->text, item->value);
                    break;
                case NONE:
                    color = 6;
                default:
                    snprintf(result, TEXTMODE_COLS, "%s", item->text);
            }
            draw_text(result, x, y, color, bg_color);
        }
        sleep_ms(100);
    }
    graphics_set_flashmode(settings.flash_line, settings.flash_frame);

    updatePalette(settings.palette);
    graphics_set_mode(GRAPHICSMODE_DEFAULT);
    save_config();
    return 0;
}


int InfoNES_LoadFrame() {
    if ((keyboard_bits.start || gamepad1_bits.start) && (keyboard_bits.select || gamepad1_bits.select)) {
        if (menu() == ROM_SELECT) {
            return -1;
        }
        //if(selected == USB_DEVICE) {
        //    return -2; // TODO: enum
        //}
    }
    frames++;

    return 0;
}

int main() {
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    set_sys_clock_khz(378 * KHZ, true);

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    for (int i = 0; i < 6; i++) {
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

    // board_init();
    tuh_init(BOARD_TUH_RHPORT);

    memset(&SCREEN[0][0], 0, sizeof SCREEN);
#if USE_PS2_KBD
    ps2kbd.init_gpio();
#endif
#if USE_NESPAD
    nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);
#endif
    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_core);
    sem_release(&vga_start_semaphore);
    load_config();

#ifndef BUILD_IN_GAMES
    if (!parseROM(reinterpret_cast<const uint8_t *>(rom)) && f_mount(&fs, "", 1) == FR_OK) {
        InfoNES_Menu();
    }
#endif
    bool start_from_game = InfoNES_Main(true);
    while (1) {
        sleep_ms(500);
        start_from_game = InfoNES_Main(start_from_game);
    }
}
