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

#include "graphics.h"

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

void updatePalette(PALETTES palette) {
    for (uint8_t i = 0; i < 64; i++) {
        if (palette == RGB333) {
            graphics_set_palette(i, NesPalette888[i + (64 * settings.nes_palette)]);
        }
        else {
            uint32_t c = NesPalette888[i + (64 * settings.nes_palette)];
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
    prev_report = prev_report;
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

#define buffermax (44100 / 60)*2
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

#define X2(a) (a | (a << 8))

void __not_in_flash_func(InfoNES_PostDrawLine)(int line) {
    for (int x = 0; x < NES_DISP_WIDTH; x++) SCREEN[line][x] = (uint8_t)linebuffer[x];
    // if (settings.show_fps && line < 16) draw_fps(fps_text, line, 255);
}

/* Renderer loop on Pico's second core */
void __scratch_x("render") render_core() {
#if TFT || HDMI
    multicore_lockout_victim_init();
#endif
    graphics_init();

    auto* buffer = &SCREEN[0][0];
    graphics_set_buffer(buffer, NES_DISP_WIDTH, NES_DISP_HEIGHT);
    graphics_set_textbuffer(buffer);
    graphics_set_bgcolor(0x000000);
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

        tight_loop_contents();
    }

    __unreachable();
}

typedef struct {
    DWORD compressed;
    int remainsDecompressed;
} FILE_LZW;
#ifdef LZW_INCLUDE
static LZWBlockInputStream * pIs = 0;
#endif
FRESULT in_open(
    FILE_LZW* fp, /* Pointer to the blank file object */
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

FRESULT in_read(
    FILE_LZW* fp, /* Open file to be read */
    char* buff, /* Data buffer to store the read data */
    size_t btr, /* Number of bytes to read */
    UINT* br /* Number of bytes read */
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

void flash_range_program2(uint32_t addr, const u_int8_t* buff, size_t sz) {
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_program(addr - XIP_BASE, buff, sz);
    restore_interrupts(interrupts);
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    //char tmp[80]; sprintf(tmp, "Flashed 0x%X len: 0x%X from 0x%X", addr, sz, buff); logMsg(tmp);
}

char* get_shared_ram() {
    return (char *)SCREEN;
}

size_t get_shared_ram_size() {
    return sizeof(SCREEN) & 0xfffff000;
}

char* get_rom_filename() {
    return (char *)rom_filename;
}

bool filebrowser_loadfile(char* pathname, bool built_in) {
    draw_text("LOADING...", 0, 0, 15, 0);
    sleep_ms(32);
    if (strcmp((char *)rom_filename, pathname) == 0) {
        return false;
    }
    //restore_clean_fat(); // in case we write into space for flash drive, it is required to remove old FAT info
    FIL file;
    UINT bytesRead;
    auto addr = (uint32_t)rom_filename;

    FRESULT result = built_in ? in_open((FILE_LZW *)&file, pathname) : f_open(&file, pathname, FA_READ);
    if (result == FR_OK) {
#if TFT || HDMI
        multicore_lockout_start_blocking();
#endif
        flash_range_erase2(addr, 4096);
        flash_range_program2(addr, reinterpret_cast<const uint8_t *>(pathname), 256);
        size_t bufsize = 8192;
        BYTE* buffer = (BYTE *)get_shared_ram();
        addr = (uint32_t)rom;
        while (true) {
            result = !built_in
                         ? f_read(&file, buffer, bufsize, &bytesRead)
                         : in_read((FILE_LZW *)&file, (char *)buffer, bufsize, &bytesRead);
            if (result == FR_OK) {
                if (bytesRead == 0) {
                    break;
                }
                flash_range_erase2(addr, bufsize);
                flash_range_program2(addr, buffer, bufsize);
                addr += bufsize;
            }
            else {
                break;
            }
        }
#ifdef LZW_INCLUDE
        if (pIs) { delete pIs; pIs = 0; }
#endif
        if (!built_in) f_close(&file);
#if TFT || HDMI
        multicore_lockout_end_blocking();
#endif
    }
    // FIXME! Починить графический драйвер при загрузке
    //watchdog_enable(100, true);
    return true;
}

typedef struct __attribute__((__packed__)) {
    bool is_directory;
    bool is_executable;
    size_t size;
    char filename[79];
} file_item_t;

constexpr int max_files = 600;
file_item_t * fileItems = (file_item_t *)(&SCREEN[0][0] + TEXTMODE_COLS*TEXTMODE_ROWS*2);

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

void __not_in_flash_func(filebrowser)(const char pathname[256], const char* executables) {
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
                debounce = !(nespad_state & DPAD_START) && !keyboard_bits.start;
            }

            // ESCAPE
            if (nespad_state & DPAD_SELECT || keyboard_bits.select) {
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

            if (nespad_state & DPAD_DOWN || keyboard_bits.down) {
                if (offset + (current_item + 1) < total_files) {
                    if (current_item + 1 < per_page) {
                        current_item++;
                    }
                    else {
                        offset++;
                    }
                }
            }

            if (nespad_state & DPAD_UP || keyboard_bits.up) {
                if (current_item > 0) {
                    current_item--;
                }
                else if (offset > 0) {
                    offset--;
                }
            }

            if (nespad_state & DPAD_RIGHT || keyboard_bits.right) {
                offset += per_page;
                if (offset + (current_item + 1) > total_files) {
                    offset = total_files - (current_item + 1);
                }
            }

            if (nespad_state & DPAD_LEFT || keyboard_bits.left) {
                if (offset > per_page) {
                    offset -= per_page;
                }
                else {
                    offset = 0;
                    current_item = 0;
                }
            }

            if (debounce && ((nespad_state & DPAD_START) != 0 || keyboard_bits.start)) {
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

                    if (filebrowser_loadfile(tmp, false)) {
                        watchdog_enable(0, true);
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
    filebrowser(HOME_DIR, (char *)"nes");
    return 0;
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

typedef struct __attribute__((__packed__)) {
    const char* text;
    menu_type_e type;
    const void* value;
    uint8_t max_value;
    char value_list[5][10];
} MenuItem;


const MenuItem menu_items[] = {
    { "Player 1: %s", ARRAY, &settings.player_1_input, 2, { "Keyboard ", "Gamepad 1", "Gamepad 2" } },
    { "Player 2: %s", ARRAY, &settings.player_2_input, 2, { "Keyboard ", "Gamepad 1", "Gamepad 2" } },
    { "" },
    { "Volume: %d", INT, &settings.snd_vol, 8 },
#if VGA
    { "Flash line: %s", ARRAY, &settings.flash_line, 1, { "NO ", "YES" } },
    { "Flash frame: %s", ARRAY, &settings.flash_frame, 1, { "NO ", "YES" } },
    { "VGA Mode: %s", ARRAY, &settings.palette, 1, { "RGB333", "RGB222" } },
#else
{ "" },
#endif
    { "NES Palette: %s", ARRAY, &settings.nes_palette, 1, { "Default ", "Colorful" } },
    { "" },
    { "Save gamestate", SAVE },
    { "Load gamestate", LOAD },
    { "" },
{ "Back to ROM select", ROM_SELECT },
{ "Load from USB device", USB_DEVICE },
{ "" },
    { "Reset game", RESET },
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
    draw_text(footer, TEXTMODE_COLS / 2 - strlen(footer) / 2, TEXTMODE_ROWS-1, 11, 1);
    int current_item = 0;
    while (!exit) {
        sleep_ms(25);
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
                            save_state((char *)rom_filename);
                            exit = true;
                        }
                        break;
                    case LOAD:
                        if (nespad_state & DPAD_START || keyboard_bits.start) {
                            load_state((char *)rom_filename);
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
                        break;

                    case ROM_SELECT:
                        if (nespad_state & DPAD_START || keyboard_bits.start) {
                            exit = true;
                            return InfoNES_Menu();
                        }
                        break;
                    case USB_DEVICE:
                        if (nespad_state & DPAD_START || keyboard_bits.start) {
                            clrScr(1);
                            draw_text((char *)"Mount me as USB drive...", 30, 15, 7, 1);
                            // in_flash_drive();
                            watchdog_enable(100, true);
                            exit = true;
                        }
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
    sleep_ms(33);
    set_sys_clock_khz(378 * KHZ, true);

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    for (int i = 0; i < 6; i++) {
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

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
