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
#include "InfoNES_Mapper.h"

extern "C" {
#include "vga.h"
#include "usb.h"
}

#include "audio.h"
#include "f_util.h"
#include "ff.h"

#if USE_PS2_KBD

#include "ps2kbd_mrmltr.h"

#endif
#if USE_NESPAD

#include "nespad.h"

#endif

#pragma GCC optimize("Ofast")

#define HOME_DIR (char*)"\\NES"

#define BUILD_IN_GAMES
#ifdef BUILD_IN_GAMES
#include "lzwSource.h"
#include "lzw.h"
size_t get_rom4prog_size() { return sizeof(rom); }
uint32_t get_rom4prog() { return (uint32_t)rom; }
#endif
#ifndef BUILD_IN_GAMES
#define FLASH_TARGET_OFFSET (1024 * 1024)
const char *rom_filename = (const char *) (XIP_BASE + FLASH_TARGET_OFFSET);
const uint8_t *rom = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET) + 4096;
class Decoder {};
#endif

enum menu_type_e {
    NONE,
    INT,
    TEXT,
    ARRAY,
    SAVE,
    LOAD,
    RESET,
    RETURN,
    ROM_SELECT,
    USB_DEVICE
};

struct semaphore vga_start_semaphore;
uint8_t SCREEN[NES_DISP_HEIGHT][NES_DISP_WIDTH]; // 61440 bytes
uint16_t linebuffer[256];
extern uint8_t fnt8x16[];

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
        .version = 1,
        .show_fps = false,
        .flash_line = true,
        .flash_frame = true,
        .palette = RGB333,
        .snd_vol = 8,
        .player_1_input = KEYBOARD,
        .player_2_input = GAMEPAD1,
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

#define RGB888(r, g, b) ((r<<16) | (g << 8 ) | b )
const BYTE NesPalette[64] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
};

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
                        0x808080, 0x0000bb, 0x3700bf, 0x8400a6,
                        0xbb006a, 0xb7001e, 0xb30000, 0x912600,
                        0x7b2b00, 0x003e00, 0x00480d, 0x003c22,
                        0x002f66, 0x000000, 0x050505, 0x050505,

                        0xc8c8c8, 0x0059ff, 0x443cff, 0xb733cc,
                        0xff33aa, 0xff375e, 0xff371a, 0xd54b00,
                        0xc46200, 0x3c7b00, 0x1e8415, 0x009566,
                        0x0084c4, 0x111111, 0x090909, 0x090909,

                        0xffffff, 0x0095ff, 0x6f84ff, 0xd56fff,
                        0xff77cc, 0xff6f99, 0xff7b59, 0xff915f,
                        0xffa233, 0xa6bf00, 0x51d96a, 0x4dd5ae,
                        0x00d9ff, 0x666666, 0x0d0d0d, 0x0d0d0d,

                        0xffffff, 0x84bfff, 0xbbbbff, 0xd0bbff,
                        0xffbfea, 0xffbfcc, 0xffc4b7, 0xffccae,
                        0xffd9a2, 0xcce199, 0xaeeeb7, 0xaaf7ee,
                        0xb3eeff, 0xdddddd, 0x111111, 0x111111,
                        /**/
        };

void updatePalette(PALETTES palette) {
    for (uint8_t i = 0; i < 64; i++) {
        if (palette == RGB333) {
            setVGA_color_palette(i, NesPalette888[i+(64*settings.nes_palette)]);
        } else {
            uint32_t c = NesPalette888[i+(64*settings.nes_palette)];
            uint8_t r = (c >> (16 + 6)) & 0x3;
            uint8_t g = (c >> (8 + 6)) & 0x3;
            uint8_t b = (c >> (0 + 6)) & 0x3;
            r *= 42 * 2;
            g *= 42 * 2;
            b *= 42 * 2;
            setVGA_color_palette(i, RGB888(r, g, b));
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
static input_bits_t keyboard_bits = { false, false, false, false, false, false, false, false };
static input_bits_t gamepad1_bits = { false, false, false, false, false, false, false, false };
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
static bool isInReport(hid_keyboard_report_t const *report, const unsigned char keycode) {
    for (unsigned char i: report->keycode) {
        if (i == keycode) {
            return true;
        }
    }
    return false;
}

void __not_in_flash_func(process_kbd_report)(hid_keyboard_report_t const *report, hid_keyboard_report_t const *prev_report) {
    keyboard_bits.start = isInReport(report, HID_KEY_ENTER);
    keyboard_bits.select = isInReport(report, HID_KEY_BACKSPACE);
    keyboard_bits.a = isInReport(report, HID_KEY_Z);
    keyboard_bits.b = isInReport(report, HID_KEY_X);
    keyboard_bits.up = isInReport(report, HID_KEY_ARROW_UP);
    keyboard_bits.down = isInReport(report, HID_KEY_ARROW_DOWN);
    keyboard_bits.left = isInReport(report, HID_KEY_ARROW_LEFT);
    keyboard_bits.right = isInReport(report, HID_KEY_ARROW_RIGHT);
    prev_report = prev_report;
}

Ps2Kbd_Mrmltr ps2kbd(
        pio1,
        0,
        process_kbd_report);
#endif

inline bool checkNESMagic(const uint8_t *data) {
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

void InfoNES_PadState(DWORD *pdwPad1, DWORD *pdwPad2, DWORD *pdwSystem) {
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
    auto &dst = *pdwPad1;
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
    auto &dst2 = *pdwPad2;
    rv = gamepad_state;
    if (rapidFireCounter2 & 2) {
        // 15 fire/sec
        rv &= ~rapidFireMask2;
    }
    dst2 = rv;
    *pdwSystem = reset ? PAD_SYS_QUIT : 0;
}

void InfoNES_MessageBox(const char *pszMsg, ...) {
    printf("[MSG]");
    va_list args;
    va_start(args, pszMsg);
    vprintf(pszMsg, args);
    va_end(args);
    printf("\n");
}

void InfoNES_Error(const char *pszMsg, ...) {
    printf("[Error]");
    va_list args;
    va_start(args, pszMsg);
    // vsnprintf(ErrorMessage, ERRORMESSAGESIZE, pszMsg, args);
    // printf("%s", ErrorMessage);
    va_end(args);
    printf("\n");
}

bool parseROM(const uint8_t *nesFile) {
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
    ROM = (BYTE *) nesFile;
    nesFile += romSize;
    if (NesHeader.byVRomSize > 0) {
        auto vromSize = NesHeader.byVRomSize * 0x2000;
        VROM = (BYTE *) nesFile;
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
    i2s_config.dma_trans_count = (uint16_t)i2s_config.sample_freq / 50;
    i2s_volume(&i2s_config, 0);
    i2s_init(&i2s_config);
}

int InfoNES_SoundOpen(int samples_per_sync, int sample_rate) {
    return 0;
}

void InfoNES_SoundClose() {
}

int __not_in_flash_func(InfoNES_GetSoundBufferSize)() { return 735; }

#define buffermax 1280

void InfoNES_SoundOutput(int samples, const BYTE *wave1, const BYTE *wave2, const BYTE *wave3, const BYTE *wave4,
                         const BYTE *wave5) {
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

void __inline draw_fps(const char fps[3], uint8_t y, uint8_t color) {
    for (uint8_t x = 0; x < 3; x++) {
        uint8_t glyph_col = fnt8x16[(fps[x] << 4) + y];
        for (uint8_t bit = 0; bit < 8; bit++)
            if ((glyph_col >> bit) & 1)
                SCREEN[y][(NES_DISP_WIDTH - 8 * 2) + 8 * x + bit] = color;
    }
}

#define X2(a) (a | (a << 8))
void __not_in_flash_func(InfoNES_PostDrawLine)(int line ){
   for(int x = 0; x< NES_DISP_WIDTH; x++) SCREEN[line][x] = linebuffer[x];
   if (settings.show_fps && line < 16) draw_fps(fps_text, line, 255);
}

#define CHECK_BIT(var, pos) (((var)>>(pos)) & 1)

/* Renderer loop on Pico's second core */
void __time_critical_func(render_core)() {
    initVGA();
    auto *buffer = reinterpret_cast<uint8_t *>(&SCREEN[0][0]);
    setVGAbuf(buffer, NES_DISP_WIDTH, NES_DISP_HEIGHT);
    uint8_t *text_buf = buffer + 1000;
    setVGA_text_buf(text_buf, &text_buf[80 * 30]);
    setVGA_bg_color(63);
    setVGAbuf_pos(32, 0);
    updatePalette(settings.palette);
    setVGA_color_flash_mode(settings.flash_line, settings.flash_frame);
    sem_acquire_blocking(&vga_start_semaphore);
}

typedef struct __attribute__((__packed__)) {
    bool is_directory;
    bool is_executable;
    size_t size;
    char filename[80];
} FileItem;

int compareFileItems(const void *a, const void *b) {
    auto *itemA = (FileItem *) a;
    auto *itemB = (FileItem *) b;
    // Directories come first
    if (itemA->is_directory && !itemB->is_directory)
        return -1;
    if (!itemA->is_directory && itemB->is_directory)
        return 1;
    // Sort files alphabetically
    return strcmp(itemA->filename, itemB->filename);
}

void __inline draw_window(char *title, int x, int y, int width, int height) {
    char textline[80];
    width--;
    height--;
    // Рисуем рамки
    // ═══
    memset(textline, 0xCD, width);
    // ╔ ╗ 188 ╝ 200 ╚
    textline[0] = 0xC9;
    textline[width] = 0xBB;
    draw_text(textline, x, y, 11, 1);
    draw_text(title, (80 - strlen(title)) >> 1, 0, 0, 3);
    textline[0] = 0xC8;
    textline[width] = 0xBC;
    draw_text(textline, x, height - y, 11, 1);
    memset(textline, ' ', width);
    textline[0] = textline[width] = 0xBA;
    for (int i = 1; i < height; i++) {
        draw_text(textline, x, i, 11, 1);
    }
}

typedef struct {
	DWORD	compressed;
    int     remainsDecompressed;
} FILE_LZW;
#ifdef LZW_INCLUDE
static LZWBlockInputStream * pIs = 0;
#endif
FRESULT in_open (
    FILE_LZW* fp,			/* Pointer to the blank file object */
    char* fn
) {
#ifdef LZW_INCLUDE
    size_t fptr = 0;
    DWORD token = toDWORD((char*)lz4source, fptr);
    DWORD numberOfFiles = token & 0xFF;
    DWORD bbOffset = token >> 8;
    fptr += 4;
    DWORD fileNum = 0;
    DWORD compressed = 0;
    DWORD compressedOff = 0;
    fp->compressed = 0;
    while (fileNum++ < numberOfFiles) {
        int i = 5; // ignore trailing "\NES\"
        while (fn[i++] == lz4source[fptr] && lz4source[fptr] != 0) {
            fptr++;
        }
        if (lz4source[fptr++] == 0) { // file was found
            compressed = toDWORD((char*)lz4source, fptr); fptr += 4;
            fp->remainsDecompressed = toDWORD((char*)lz4source, fptr); fptr += 4;
            fp->compressed = compressed;
            fptr = (size_t)lz4source + bbOffset + compressedOff;
            if (pIs) {
                delete pIs;
            }
            pIs = new LZWBlockInputStream((char*)fptr, (const char*)rom);
            return FR_OK;
        } else {
            while(lz4source[fptr++] != 0) {}
            compressed = toDWORD((char*)lz4source, fptr); fptr += 8;
            compressedOff += compressed;
        }
    }
#endif
    return FR_NO_FILE;
}

FRESULT in_read (
	FILE_LZW* fp, 	/* Open file to be read */
	char*     buff,	/* Data buffer to store the read data */
	size_t    btr,	/* Number of bytes to read */
	UINT*     br	/* Number of bytes read */
) {
#ifdef LZW_INCLUDE
    if (fp->remainsDecompressed <= 0) {
        *br = 0;
    } else {
        int decompressed = btr;
        size_t read = pIs->read(buff, btr, &decompressed);
        fp->remainsDecompressed -= decompressed;
        *br = read;
    }
#endif
    return FR_OK;
}

void flash_range_erase2(uint32_t addr, size_t sz) {
    // char tmp[80]; sprintf(tmp, "Erase 0x%X len: 0x%X", addr, sz); logMsg(tmp);
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(addr - XIP_BASE, sz);
    restore_interrupts(interrupts);
    gpio_put(PICO_DEFAULT_LED_PIN, false);
}
void flash_range_program2(uint32_t addr, const u_int8_t * buff, size_t sz) {
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_program(addr - XIP_BASE, buff, sz);
    restore_interrupts(interrupts);
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    //char tmp[80]; sprintf(tmp, "Flashed 0x%X len: 0x%X from 0x%X", addr, sz, buff); logMsg(tmp);
}

char* get_shared_ram() {
    return (char*)SCREEN;
}
size_t get_shared_ram_size() {
    return sizeof(SCREEN) & 0xfffff000;
}
char* get_rom_filename() {
    return (char*)rom_filename;
} 

void filebrowser_loadfile(char *pathname, bool built_in) {
    setVGAmode(VGA640x480_text_80_30);
    clrScr(1);
    if (strcmp((char*)rom_filename, pathname) == 0) {
        logMsg((char*)"Launching last rom");
        return;
    }
    restore_clean_fat(); // in case we write into space for flash drive, it is required to remove old FAT info
    FIL file;
    UINT bytesRead;
    char tmp[80];
    uint32_t addr = (uint32_t)rom_filename;
    sprintf(tmp, "Writing %s rom to flash %x", pathname, addr); logMsg(tmp);
    FRESULT result = built_in ? in_open((FILE_LZW*)&file, pathname) : f_open(&file, pathname, FA_READ);
    if (result == FR_OK) {
        flash_range_erase2(addr, 4096);
        flash_range_program2(addr, reinterpret_cast<const uint8_t *>(pathname), 256);
        size_t bufsize = get_shared_ram_size();
        BYTE * buffer = (BYTE*)get_shared_ram();
        addr = (uint32_t)rom;
        while(true) {
            result = !built_in
              ? f_read(&file, buffer, bufsize, &bytesRead)
              : in_read((FILE_LZW*)&file, (char*)buffer, bufsize, &bytesRead);
            if (result == FR_OK) {
                if (bytesRead == 0) {
                    logMsg((char*)"Done");
                    break;
                }
                flash_range_erase2(addr, bufsize);
                flash_range_program2(addr, buffer, bufsize);
                addr += bufsize;
            } else {
                sprintf(tmp, "Error reading rom: %d", result); logMsg(tmp);
                break;
            }
        }
#ifdef LZW_INCLUDE
        if (pIs) { delete pIs; pIs = 0; }
#endif
        if (!built_in) f_close(&file);
    }
}

FRESULT in_opendir(DIR* dp) {
    dp->blk_ofs = 4; // offset to filename
    dp->clust = 0; // file number
    return FR_OK;
}

FRESULT in_closedir(DIR* dp) {
    dp->blk_ofs = 0xFFFFFFFF;
    dp->clust = 0;
    return FR_OK;
}

FRESULT in_readdir (
	DIR*     dp,		/* Pointer to the open directory object */
	FILINFO* fno		/* Pointer to file information to return */
) {
#ifdef BUILD_IN_GAMES
    dp->clust++;
    DWORD numberOfFiles = toDWORD((char*)lz4source, 0) & 0xFF;
    if (dp->clust > numberOfFiles) {
        return FR_NO_FILE;
    }
    DWORD fnz = 0;
    while (lz4source[dp->blk_ofs] != 0) {
        fno->fname[fnz++] = lz4source[dp->blk_ofs++];
    }
    fno->fname[fnz] = 0; dp->blk_ofs++; // traling zero
    dp->blk_ofs += 4; // ignore compressedSize there
    fno->fsize = toDWORD((char*)lz4source, dp->blk_ofs); dp->blk_ofs += 4;
    fno->fattrib = AM_RDO;
#endif
    return FR_OK;    
}

void filebrowser(
    char *path,
    char *executable
) {
    setVGAmode(VGA640x480_text_80_30);
    bool debounce = true;
    clrScr(1);
    char basepath[256];
    char tmp[256];
    strcpy(basepath, path);
    constexpr int per_page = 27;
    auto *fileItems = reinterpret_cast<FileItem *>(&SCREEN[0][0] + (1024 * 6));
    constexpr int maxfiles = (sizeof(SCREEN) - (1024 * 6)) / sizeof(FileItem);
    DIR dir, dir1;
    FILINFO fileInfo;
    FRESULT result = f_mount(&fs, "", 1);
    int built_in = false;
    if (FR_OK != result) {
        printf("f_mount error: %s (%d)\r\n", FRESULT_str(result), result);
        draw_text((char*)"No SD Card detected", 1, 1, 4, 1);
#ifdef BUILD_IN_GAMES
        built_in = true;
        sleep_ms(200);
#endif
#ifndef BUILD_IN_GAMES
        while (1) { sleep_ms(100); /*TODO: reboot? */}
#endif
    }
    FRESULT result1 = f_mount(&fs1, "F:", 0);
    if (FR_OK != result1) {
        sprintf(tmp, "f_mount error: %s (%d)", FRESULT_str(result1), result1); logMsg(tmp);
        while (1) { sleep_ms(100); /*TODO: reboot? */}
    }
    while (1) {
        int total_files = 0;
        memset(fileItems, 0, maxfiles * sizeof(FileItem));
        sprintf(tmp, !built_in ? " SDCARD:\\%s " : " DEFAULT:\\%s ", basepath);
        draw_window(tmp, 0, 0, 80, 29);
        memset(tmp, ' ', 80);
        draw_text(tmp, 0, 29, 0, 0);
        auto off = 0;
        draw_text((char*)"START", off, 29, 7, 0); off += 5;
        draw_text((char*)" Run at cursor ", off, 29, 0, 3); off += 16;
        draw_text((char*)"SELECT", off, 29, 7, 0); off += 6;
        draw_text((char*)" Run previous  ", off, 29, 0, 3); off += 16;
        draw_text((char*)"ARROWS", off, 29, 7, 0); off += 6;
        draw_text((char*)" Navigation    ", off, 29, 0, 3); off += 16;
        draw_text((char*)"A/Z", off, 29, 7, 0); off += 3;
        draw_text((char*)" USB DRV ", off, 29, 0, 3);
        // Open the directory
        if ((built_in ? in_opendir(&dir) : f_opendir(&dir, basepath)) != FR_OK) {
            draw_text((char*)"Failed to open directory", 1, 1, 4, 0);
            while (1) { sleep_ms(100); }
        }
        if (!built_in && strlen(basepath) > 0) {
            strcpy(fileItems[total_files].filename, "..\0");
            fileItems[total_files].is_directory = true;
            fileItems[total_files].size = 0;
            total_files++;
        }
        while ((built_in ? in_readdir(&dir, &fileInfo) : f_readdir(&dir, &fileInfo)) == FR_OK &&
               fileInfo.fname[0] != '\0' &&
               total_files < maxfiles
        ) {
            // Set the file item properties
            fileItems[total_files].is_directory = fileInfo.fattrib & AM_DIR;
            fileItems[total_files].size = fileInfo.fsize;
            // Extract the extension from the file name
            char *extension = strrchr(fileInfo.fname, '.');
            if (extension != NULL && strncmp(executable, extension + 1, 3) == 0) {
                fileItems[total_files].is_executable = 1;
            }
            strncpy(fileItems[total_files].filename, fileInfo.fname, 80);
            total_files++;
        }
        // in_flash drive
        result1 = f_opendir(&dir1, "F:\\");
        if (result1 != FR_OK) {
            sprintf(tmp, "f_opendir(F:\\) error: %s (%d)", FRESULT_str(result1), result1); logMsg(tmp);
            while (1) { sleep_ms(100); }
        }
        while (f_readdir(&dir1, &fileInfo) == FR_OK &&
               fileInfo.fname[0] != '\0' &&
               total_files < maxfiles
        ) {
            // Set the file item properties
            fileItems[total_files].is_directory = fileInfo.fattrib & AM_DIR;
            fileItems[total_files].size = fileInfo.fsize;
            // Extract the extension from the file name
            char *extension = strrchr(fileInfo.fname, '.');
            if (extension != NULL && strncmp(executable, extension + 1, 3) == 0) {
                fileItems[total_files].is_executable = 1;
            }
            strncpy(fileItems[total_files].filename, fileInfo.fname, 80);
            total_files++;
        }                
        qsort(fileItems, total_files, sizeof(FileItem), compareFileItems);
        // Cleanup
        built_in ? in_closedir(&dir) : f_closedir(&dir);
        if (total_files > 500) {
            draw_text((char*)" files > 500!!! ", 80 - 17, 0, 12, 3);
        }
        uint8_t color, bg_color;
        uint32_t offset = 0;
        uint32_t current_item = 0;
        while (1) {
            ps2kbd.tick();
            nespad_tick();
            sleep_ms(25);
            nespad_tick();
            if (!debounce) {
                debounce = !(keyboard_bits.start || gamepad1_bits.start);
            }
            if (keyboard_bits.select || gamepad1_bits.select) {
                gpio_put(PICO_DEFAULT_LED_PIN, true);
                return;
            }
            if (keyboard_bits.a || gamepad1_bits.a) {
                clrScr(1);
                draw_text((char*)"Mount me as USB drive...", 30, 15, 7, 1);
                in_flash_drive();
                watchdog_enable(100, true);
            }
            if (keyboard_bits.down || gamepad1_bits.down) {
                if ((offset + (current_item + 1) < total_files)) {
                    if ((current_item + 1) < per_page) {
                        current_item++;
                    } else {
                        offset++;
                    }
                }
            }
            if (keyboard_bits.up || gamepad1_bits.up) {
                if (current_item > 0) {
                    current_item--;
                } else if (offset > 0) {
                    offset--;
                }
            }
            if (keyboard_bits.right || gamepad1_bits.right) {
                offset += per_page;
                if (offset + (current_item + 1) > total_files) {
                    offset = total_files - (current_item + 1);
                }
            }
            if (keyboard_bits.left || gamepad1_bits.left) {
                if (offset > per_page) {
                    offset -= per_page;
                } else {
                    offset = 0;
                    current_item = 0;
                }
            }
            if (debounce && (keyboard_bits.start || gamepad1_bits.start)) {
                auto file_at_cursor = fileItems[offset + current_item];
                if (file_at_cursor.is_directory) {
                    if (strcmp(file_at_cursor.filename, "..") == 0) {
                        char *lastBackslash = strrchr(basepath, '\\');
                        if (lastBackslash != nullptr) {
                            size_t length = lastBackslash - basepath;
                            basepath[length] = '\0';
                        }
                    } else {
                        sprintf(basepath, "%s\\%s", basepath, file_at_cursor.filename);
                    }
                    debounce = false;
                    break;
                }
                if (file_at_cursor.is_executable) {
                    sprintf(tmp, "%s\\%s", basepath, file_at_cursor.filename);
                    return filebrowser_loadfile(tmp, built_in);
                }
            }
            for (int i = 0; i < per_page; i++) {
                auto item = fileItems[offset + i];
                color = 11;
                bg_color = 1;
                if (i == current_item) {
                    color = 0;
                    bg_color = 3;
                    memset(tmp, 0xCD, 78);
                    tmp[78] = '\0';
                    draw_text(tmp, 1, per_page + 1, 11, 1);
                    sprintf(tmp, " Size: %iKb, File %lu of %i ", item.size / 1024, offset + i + 1, total_files);
                    draw_text(tmp, (80 - strlen(tmp)) >> 1, per_page + 1, 14, 3);
                }
                auto len = strlen(item.filename);
                color = item.is_directory ? 15 : color;
                color = item.is_executable ? 10 : color;
                color = strstr((char*)rom_filename, item.filename) != nullptr ? 13 : color;
                memset(tmp, ' ', 78);
                tmp[78] = '\0';
                memcpy(&tmp, item.filename, len < 78 ? len : 78);
                draw_text(tmp, 1, i + 1, color, bg_color);
            }
            sleep_ms(100);
        }
    }
}

int InfoNES_Video() {
    setVGAmode(VGA640x480_text_80_30);
    clrScr(1);    
    if (!parseROM(reinterpret_cast<const uint8_t *>(rom))) {
        logMsg((char*)"NES file parse error.");
        return 1;
    }
    if (InfoNES_Reset() < 0) {
        logMsg((char*)"NES reset error.");
        return 1; 
    }
    memset(SCREEN, 63, sizeof(SCREEN));    
    setVGAmode(VGA640x480div2);
    return 0;
}

int InfoNES_Menu() {
    setVGAmode(VGA640x480_text_80_30);
    clrScr(1);
    filebrowser(HOME_DIR, (char*)"nes");
    return 0;
}

void load_config() {
    char pathname[256];
    sprintf(pathname, "%s\\emulator.cfg", HOME_DIR);
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
    sprintf(pathname, "%s\\emulator.cfg", HOME_DIR);
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

typedef struct __attribute__((__packed__)) {
    const char *text;
    menu_type_e type;
    const void *value;
    uint8_t max_value;
    char value_list[5][10];
} MenuItem;

static menu_type_e last_menu_type = menu_type_e::ROM_SELECT;

#define MENU_ITEMS_NUMBER 17
const MenuItem menu_items[MENU_ITEMS_NUMBER] = {
        { "Player 1: %s",        ARRAY, &settings.player_1_input, 2, { "Keyboard ", "Gamepad 1", "Gamepad 2" }},
        { "Player 2: %s",        ARRAY, &settings.player_2_input, 2, { "Keyboard ", "Gamepad 1", "Gamepad 2" }},
        { "" },
        { "Volume: %d",          INT,   &settings.snd_vol,        8 },
        { "" },
        { "Flash line: %s",      ARRAY, &settings.flash_line,     1, { "NO ",       "YES" }},
        { "Flash frame: %s",     ARRAY, &settings.flash_frame,    1, { "NO ",       "YES" }},
        { "VGA Mode: %s",        ARRAY, &settings.palette,        1, { "RGB333",    "RGB222" }},
        { "NES Palette: %s",     ARRAY, &settings.nes_palette,    1, { "default ",    "palette1" }},
        { "" },
        { "Save state",          SAVE },
        { "Load state",          LOAD },
        { "" },
        { "Reset",               RESET },
        { "Return to game",      RETURN },
        { "ROM select",          ROM_SELECT },
        { "From USB device",     USB_DEVICE }
};

int menu() {
    bool exit = false;
    clrScr(0);
    setVGAmode(VGA640x480_text_80_30);
    char footer[80];
    sprintf(footer, ":: %s %s build %s %s ::", PICO_PROGRAM_NAME, PICO_PROGRAM_VERSION_STRING, __DATE__, __TIME__);
    draw_text(footer, (sizeof(footer) - strlen(footer)) >> 1, 0, 11, 1);
    int current_item = 0;
    while (!exit) {
        ps2kbd.tick();
        nespad_read();
        sleep_ms(25);
        nespad_read();
        if ((nespad_state & DPAD_DOWN || keyboard_bits.down) != 0) {
            current_item = (current_item + 1) % MENU_ITEMS_NUMBER;
            if (menu_items[current_item].type == NONE)
                current_item++;
        }
        if ((nespad_state & DPAD_UP || keyboard_bits.up) != 0) {
            current_item = (current_item - 1 + MENU_ITEMS_NUMBER) % MENU_ITEMS_NUMBER;
            if (menu_items[current_item].type == NONE)
                current_item--;
        }
        for (int i = 0; i < MENU_ITEMS_NUMBER; i++) {
            uint8_t y = i + ((30 - MENU_ITEMS_NUMBER) >> 1);
            uint8_t x = 30;
            uint8_t color = 0xFF;
            uint8_t bg_color = 0x00;
            if (current_item == i) {
                color = 0x01;
                bg_color = 0xFF;
            }
            const MenuItem *item = &menu_items[i];
            if (i == current_item) {
                last_menu_type = item->type;
                switch (item->type) {
                    case INT:
                    case ARRAY:
                        if (item->max_value != 0) {
                            auto *value = (uint8_t *) item->value;
                            if ((nespad_state & DPAD_RIGHT || keyboard_bits.right) && *value < item->max_value) {
                                (*value)++;
                            }
                            if ((nespad_state & DPAD_LEFT || keyboard_bits.left) && *value > 0) {
                                (*value)--;
                            }
                        }
                        break;
                    case SAVE:
                        if (nespad_state & DPAD_START || keyboard_bits.start) {
                            save_state((char*)rom_filename);
                            exit = true;
                        }
                        break;
                    case LOAD:
                        if (nespad_state & DPAD_START || keyboard_bits.start) {
                            load_state((char*)rom_filename);
                            exit = true;
                        }
                        break;
                    case RETURN:
                        if (nespad_state & DPAD_START || keyboard_bits.start)
                            exit = true;
                        break;
                    case RESET:
                        if (nespad_state & DPAD_START || keyboard_bits.start)
                            watchdog_enable(100, true);
                    case ROM_SELECT:
                        if (nespad_state & DPAD_START || keyboard_bits.start) {
                            return ROM_SELECT;
                        }
                    case USB_DEVICE:
                        if (nespad_state & DPAD_START || keyboard_bits.start) {
                            clrScr(1);
                            draw_text((char*)"Mount me as USB drive...", 30, 15, 7, 1);
                            in_flash_drive();
                            watchdog_enable(100, true);
                            exit = true;
                        }
                }
            }
            static char result[80];
            switch (item->type) {
                case INT:
                    sprintf(result, item->text, *(uint8_t *) item->value);
                    break;
                case ARRAY:
                    sprintf(result, item->text, item->value_list[*(uint8_t *) item->value]);
                    break;
                case TEXT:
                    sprintf(result, item->text, item->value);
                    break;
                default:
                    sprintf(result, "%s", item->text);
            }
            draw_text(result, x, y, color, bg_color);
        }
        sleep_ms(100);
    }
    memset(SCREEN, 63, sizeof SCREEN);
    setVGA_color_flash_mode(settings.flash_line, settings.flash_frame);
    updatePalette(settings.palette);
    setVGAmode(VGA640x480div2);
    save_config();
    return 0;
}


int InfoNES_LoadFrame() {
#if USE_PS2_KBD
    ps2kbd.tick();
#endif
#if USE_NESPAD
    nespad_tick();
#endif
    if ((keyboard_bits.start || gamepad1_bits.start) && (keyboard_bits.select || gamepad1_bits.select)) {
        auto selected = menu();
        if(selected == ROM_SELECT) {
            return -1;
        }
        //if(selected == USB_DEVICE) {
        //    return -2; // TODO: enum
        //}
    }
    frames++;
    if (settings.show_fps && frames >= 60) {
        uint64_t end_time;
        uint32_t diff;
        uint8_t fps;
        end_time = time_us_64();
        diff = end_time - start_time;
        fps = ((uint64_t) frames * 1000 * 1000) / diff;
        sprintf(fps_text, "%i", fps);
        frames = 0;
        start_time = time_us_64();
    }
    return 0;
}

int main() {
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(33);
    set_sys_clock_khz(272000, true);
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    for (int i = 0; i < 6; i++) {
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }
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
    sleep_ms(50);
    bool start_from_game = InfoNES_Main(true);
    while(1) {
        sleep_ms(500);
        start_from_game = InfoNES_Main(start_from_game);
    }
}
