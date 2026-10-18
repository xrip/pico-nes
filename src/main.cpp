#include <cstdio>
#include <cstring>
#include <cstdarg>

#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/i2c.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include <hardware/sync.h>
#include <hardware/flash.h>
#if PICO_RP2350
#include <hardware/structs/qmi.h>
#endif

#include "main.h"
#include "psram.h"

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
/* Flash retains the last cartridge across power cycles.  A ROM loaded into
 * QSPI PSRAM replaces these pointers only until the next hardware reset. */
static char psram_rom_filename[256];
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
// set by the render core once text/graphics buffers are attached to the video driver
static volatile bool render_ready = false;
uint8_t SCREEN[NES_DISP_HEIGHT][NES_DISP_WIDTH]; // 61440 bytes
uint16_t linebuffer[256];

SETTINGS settings = {
    .version = 3,
    .show_fps = false,
    .flash_line = true,
    .flash_frame = true,
    .palette = RGB333,
    .snd_vol = 4,
    .player_1_input = COMBINED,
    .player_2_input = GAMEPAD1,
    .nes_palette = 0,
    .swap_ab = false,
    .inversion = false,
    .tv_system = 0,
    .tv_color = true,
    .nes_timing = 0
};

/* All watchdog resets (including USB keyboard Ctrl+Alt+Del) take the same
 * route.  Drop clk_sys before VREG; release QMI CS1 first so PSRAM does not
 * remain in QPI mode across reset. */
extern "C" void __no_inline_not_in_flash_func(nes_safe_reset)(void) {
    watchdog_enable(100, true); // failsafe in case QMI/clock transition stalls
#if PICO_RP2350
    save_and_disable_interrupts();
    nes_psram_prepare_reset();
    set_sys_clock_khz(150000, false);
    vreg_set_voltage(VREG_VOLTAGE_1_10);
    sleep_ms(10);
#endif
    watchdog_reboot(0, 0, 10);
    while (true) tight_loop_contents();
}

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
    input_bits_t operator |(const input_bits_t& o) {
        input_bits_t res = {
            a      | o.a,
            b      | o.b,
            select | o.select,
            start  | o.start,
            right  | o.right,
            left   | o.left,
            up     | o.up,
            down   | o.down
        };
        return res;
    }
};

input_bits_t keyboard_bits = { false, false, false, false, false, false, false, false };
input_bits_t gamepad1_bits = { false, false, false, false, false, false, false, false };
static input_bits_t gamepad2_bits = { false, false, false, false, false, false, false, false };

#if USE_NESPAD

void nespad_tick() {
    nespad_read();

    gamepad1_bits.a = settings.swap_ab ? (nespad_state & DPAD_B) != 0 : (nespad_state & DPAD_A) != 0;
    gamepad1_bits.b = settings.swap_ab ? (nespad_state & DPAD_A) != 0 : (nespad_state & DPAD_B) != 0;
    gamepad1_bits.select = (nespad_state & DPAD_SELECT) != 0;
    gamepad1_bits.start = (nespad_state & DPAD_START) != 0;
    gamepad1_bits.up = (nespad_state & DPAD_UP) != 0;
    gamepad1_bits.down = (nespad_state & DPAD_DOWN) != 0;
    gamepad1_bits.left = (nespad_state & DPAD_LEFT) != 0;
    gamepad1_bits.right = (nespad_state & DPAD_RIGHT) != 0;

    // second
    gamepad2_bits.a = settings.swap_ab ? (nespad_state2 & DPAD_B) != 0 : (nespad_state2 & DPAD_A) != 0;
    gamepad2_bits.b = settings.swap_ab ? (nespad_state2 & DPAD_A) != 0 : (nespad_state2 & DPAD_B) != 0;
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

extern "C" {
    volatile bool altPressed = false;
    volatile bool ctrlPressed = false;
    volatile uint8_t fxPressedV = 0;
}

static volatile bool kbd_page_up = false;
static volatile bool kbd_page_down = false;
static volatile bool kbd_home = false;
static volatile bool kbd_end = false;

void __not_in_flash_func(process_kbd_report)(hid_keyboard_report_t const* report,
                                             hid_keyboard_report_t const* prev_report) {
    keyboard_bits.start = isInReport(report, HID_KEY_ENTER) || isInReport(report, HID_KEY_KEYPAD_ENTER);
    keyboard_bits.select = isInReport(report, HID_KEY_BACKSPACE) || isInReport(report, HID_KEY_ESCAPE) || isInReport(report, HID_KEY_KEYPAD_ADD);
    if (settings.swap_ab) {
        keyboard_bits.b = isInReport(report, HID_KEY_Z) || isInReport(report, HID_KEY_O) || isInReport(report, HID_KEY_KEYPAD_0);
        keyboard_bits.a = isInReport(report, HID_KEY_X) || isInReport(report, HID_KEY_P) || isInReport(report, HID_KEY_KEYPAD_DECIMAL);
    } else {
        keyboard_bits.a = isInReport(report, HID_KEY_Z) || isInReport(report, HID_KEY_O) || isInReport(report, HID_KEY_KEYPAD_0);
        keyboard_bits.b = isInReport(report, HID_KEY_X) || isInReport(report, HID_KEY_P) || isInReport(report, HID_KEY_KEYPAD_DECIMAL);
    }

    bool b7 = isInReport(report, HID_KEY_KEYPAD_7);
    bool b9 = isInReport(report, HID_KEY_KEYPAD_9);
    bool b1 = isInReport(report, HID_KEY_KEYPAD_1);
    bool b3 = isInReport(report, HID_KEY_KEYPAD_3);

    keyboard_bits.up = b7 || b9 || isInReport(report, HID_KEY_ARROW_UP) || isInReport(report, HID_KEY_W) || isInReport(report, HID_KEY_KEYPAD_8);
    keyboard_bits.down = b1 || b3 || isInReport(report, HID_KEY_ARROW_DOWN) || isInReport(report, HID_KEY_S) || isInReport(report, HID_KEY_KEYPAD_2) || isInReport(report, HID_KEY_KEYPAD_5);
    keyboard_bits.left = b7 || b1 || isInReport(report, HID_KEY_ARROW_LEFT) || isInReport(report, HID_KEY_A) || isInReport(report, HID_KEY_KEYPAD_4);
    keyboard_bits.right = b9 || b3 || isInReport(report, HID_KEY_ARROW_RIGHT)  || isInReport(report, HID_KEY_D) || isInReport(report, HID_KEY_KEYPAD_6);

    // file browser navigation
    kbd_page_up = isInReport(report, HID_KEY_PAGE_UP);
    kbd_page_down = isInReport(report, HID_KEY_PAGE_DOWN);
    kbd_home = isInReport(report, HID_KEY_HOME);
    kbd_end = isInReport(report, HID_KEY_END);

    altPressed = isInReport(report, HID_KEY_ALT_LEFT) || isInReport(report, HID_KEY_ALT_RIGHT);
    ctrlPressed = isInReport(report, HID_KEY_CONTROL_LEFT) || isInReport(report, HID_KEY_CONTROL_RIGHT);
    
    if (altPressed && ctrlPressed && isInReport(report, HID_KEY_DELETE)) {
        nes_safe_reset();
    }
    if (ctrlPressed || altPressed) {
        uint8_t fxPressed = 0;
        if (isInReport(report, HID_KEY_F1)) fxPressed = 1;
        else if (isInReport(report, HID_KEY_F2)) fxPressed = 2;
        else if (isInReport(report, HID_KEY_F3)) fxPressed = 3;
        else if (isInReport(report, HID_KEY_F4)) fxPressed = 4;
        else if (isInReport(report, HID_KEY_F5)) fxPressed = 5;
        else if (isInReport(report, HID_KEY_F6)) fxPressed = 6;
        else if (isInReport(report, HID_KEY_F7)) fxPressed = 7;
        else if (isInReport(report, HID_KEY_F8)) fxPressed = 8;
        fxPressedV = fxPressed;
    }
}

Ps2Kbd_Mrmltr ps2kbd(
    pio1,
    PS2KBD_GPIO_FIRST,
    process_kbd_report
);
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
int save_slot = 0;
bool load();
bool save();

void InfoNES_PadState(DWORD* pdwPad1, DWORD* pdwPad2, DWORD* pdwSystem) {
    static constexpr int LEFT = 1 << 6;
    static constexpr int RIGHT = 1 << 7;
    static constexpr int UP = 1 << 4;
    static constexpr int DOWN = 1 << 5;
    static constexpr int SELECT = 1 << 2;
    static constexpr int START = 1 << 3;
    static constexpr int A = 1 << 0;
    static constexpr int B = 1 << 1;

    if (fxPressedV) {
        if (altPressed) {
            save_slot = fxPressedV;
            load();
        } else if (ctrlPressed) {
            save_slot = fxPressedV;
            save();
        }
    }

    input_bits_t player1_state = {};
    input_bits_t player2_state = {};
    switch (settings.player_1_input) {
        case KEYBOARD:
            player1_state = keyboard_bits;
            break;
        case GAMEPAD1:
            player1_state = gamepad1_bits;
            break;
        case GAMEPAD2:
            player1_state = gamepad2_bits;
            break;
        case COMBINED:
            player1_state = keyboard_bits | gamepad1_bits;
            break;
    }
    switch (settings.player_2_input) {
        case KEYBOARD:
            player2_state = keyboard_bits;
            break;
        case GAMEPAD1:
            player2_state = gamepad1_bits;
            break;
        case GAMEPAD2:
            player2_state = gamepad2_bits;
            break;
        case COMBINED:
            player2_state = keyboard_bits | gamepad1_bits;
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
#if SOFTTV
    graphics_set_offset(0, 0); // the TV driver places the 256 pixels itself, with the real NES pixel width
#else
    graphics_set_offset(32, 0);
#endif

    updatePalette(settings.palette);
    graphics_set_flashmode(settings.flash_line, settings.flash_frame);
    render_ready = true;
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


/* The flash slot begins at 1 MiB and reserves one sector for the ROM path.
 * Keep a flash fallback for boards without a populated/working QSPI PSRAM. */
static size_t flash_rom_capacity(void) {
    return (size_t)PICO_FLASH_SIZE_BYTES - FLASH_TARGET_OFFSET - FLASH_SECTOR_SIZE;
}

bool filebrowser_loadfile(const char pathname[256]) {
    FIL file;
    FILINFO fileinfo;
    const int window_w = (int)TEXTMODE_COLS < 43 ? (int)TEXTMODE_COLS : 43;
    const int window_y = (TEXTMODE_ROWS - 5) / 2;
    const int window_x = ((int)TEXTMODE_COLS - window_w) / 2;

    draw_window("Loading ROM", window_x, window_y, window_w, 5);
    if (f_stat(pathname, &fileinfo) != FR_OK || fileinfo.fsize < 16) {
        draw_text("ROM missing or too small", window_x + 1, window_y + 2, 12, 1);
        sleep_ms(1200);
        return false;
    }

    const size_t load_size = fileinfo.fsize;
    const bool use_psram = nes_psram_available() &&
                           load_size <= nes_psram_size();
    if (!use_psram && load_size > flash_rom_capacity()) {
        draw_text("ROM exceeds available storage", window_x + 1, window_y + 2, 12, 1);
        sleep_ms(1200);
        return false;
    }
    if (f_open(&file, pathname, FA_READ) != FR_OK)
        return false;

    /* Reject malformed/unsupported ROMs before overwriting the cartridge.
     * An iNES trainer is included in the file size, not removed from RAM. */
    uint8_t header[16];
    UINT got = 0;
    const bool valid = f_read(&file, header, sizeof header, &got) == FR_OK &&
                       got == sizeof header && checkNESMagic(header) &&
                       load_size >= 16u + ((header[6] & 4u) ? 512u : 0u) +
                                        (size_t)header[4] * 16384u +
                                        (size_t)header[5] * 8192u;
    if (!valid || f_lseek(&file, 0) != FR_OK) {
        f_close(&file);
        draw_text("Invalid/unsupported NES ROM", window_x + 1, window_y + 2, 12, 1);
        sleep_ms(1200);
        return false;
    }

    draw_text(use_psram ? "Loading into QSPI PSRAM..." : "Loading into Flash...",
              window_x + 1, window_y + 2, 10, 1);
    UINT bytes_read = 0;
    size_t total_read = 0;
    FRESULT fr = FR_OK;

    if (use_psram) {
        uint8_t *dst = (uint8_t *)NES_PSRAM_BASE;
        do {
            const UINT chunk = (UINT)((load_size - total_read) > 4096u ?
                                      4096u : load_size - total_read);
            fr = f_read(&file, dst + total_read, chunk, &bytes_read);
            total_read += bytes_read;
        } while (fr == FR_OK && bytes_read != 0 && total_read < load_size);
        f_close(&file);
        if (fr != FR_OK || total_read != load_size) {
            draw_text("PSRAM ROM load failed", window_x + 1, window_y + 2, 12, 1);
            sleep_ms(1200);
            return false;
        }
        memcpy(psram_rom_filename, pathname, strlen(pathname) + 1);
        rom_filename = psram_rom_filename;
        rom = (const uint8_t *)NES_PSRAM_BASE;
        return true; // no reset: external PSRAM is not nonvolatile
    }

    /* Flash fallback: program complete padded sectors while core1 is
     * locked out.  Never write beyond the board's flash capacity. */
    static uint8_t buffer[FLASH_SECTOR_SIZE] __aligned(4);
    uint32_t target = FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE;
    bool failed = false;
    multicore_lockout_start_blocking();
    while (total_read < load_size) {
        memset(buffer, 0xff, sizeof(buffer));
        const UINT chunk = (UINT)((load_size - total_read) > sizeof(buffer) ?
                                  sizeof(buffer) : load_size - total_read);
        fr = f_read(&file, buffer, chunk, &bytes_read);
        if (fr != FR_OK || bytes_read != chunk) {
            failed = true;
            break;
        }
        total_read += bytes_read;
        const uint8_t *mapped = (const uint8_t *)(XIP_BASE + target);
        if (memcmp(mapped, buffer, sizeof(buffer)) != 0) {
            uint32_t ints = save_and_disable_interrupts();
            flash_range_erase(target, FLASH_SECTOR_SIZE);
            flash_range_program(target, buffer, FLASH_SECTOR_SIZE);
            restore_interrupts(ints);
            if (memcmp(mapped, buffer, sizeof(buffer)) != 0) {
                failed = true;
                break;
            }
        }
        target += FLASH_SECTOR_SIZE;
        gpio_put(PICO_DEFAULT_LED_PIN, (target >> 13) & 1);
    }
    multicore_lockout_end_blocking();
    f_close(&file);
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    if (failed || total_read != load_size) {
        draw_text("Flash ROM load failed", window_x + 1, window_y + 2, 12, 1);
        sleep_ms(1200);
        return false;
    }

    /* The first sector is a persistent filename, not ROM data.  Like the
     * Gamate Flash loader, do not erase an identical sector, and verify
     * every sector that was programmed before reporting success. */
    static uint8_t name_sector[FLASH_SECTOR_SIZE] __aligned(4);
    memset(name_sector, 0xff, sizeof(name_sector));
    const size_t pathname_len = strlen(pathname);
    if (pathname_len >= sizeof(name_sector)) {
        draw_text("Flash ROM path too long", window_x + 1, window_y + 2, 12, 1);
        sleep_ms(1200);
        return false;
    }
    memcpy(name_sector, pathname, pathname_len + 1);
    const uint8_t *name_flash = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    if (memcmp(name_flash, name_sector, sizeof(name_sector)) != 0) {
        multicore_lockout_start_blocking();
        const uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_TARGET_OFFSET, name_sector, FLASH_SECTOR_SIZE);
        restore_interrupts(ints);
        multicore_lockout_end_blocking();
        if (memcmp(name_flash, name_sector, sizeof(name_sector)) != 0) {
            draw_text("Flash filename verify failed", window_x + 1, window_y + 2, 12, 1);
            sleep_ms(1200);
            return false;
        }
    }
    return true;
}

typedef struct __attribute__((__packed__)) {
    bool is_directory;
    bool is_executable;
    size_t size;
    char filename[79];
} file_item_t;

constexpr int max_files = 600;
#ifdef TEXTMODE_COLS_MAX // text columns change at run time (TV): keep the file list past the largest text buffer
file_item_t* fileItems = (file_item_t *)(&SCREEN[0][0] + TEXTMODE_COLS_MAX * TEXTMODE_ROWS * 2);
#else
file_item_t* fileItems = (file_item_t *)(&SCREEN[0][0] + TEXTMODE_COLS * TEXTMODE_ROWS * 2);
#endif

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

void save_config();

bool is_start_locked() {
    FIL fd;
    char pathname[256];
    sprintf(pathname, "%s\\pico-nes.lock", HOME_DIR);
    bool res = FR_OK == f_open(&fd, pathname, FA_READ);
    f_close(&fd);
    return res;
}

void unlock_start() {
    char pathname[256];
    sprintf(pathname, "%s\\pico-nes.lock", HOME_DIR);
    f_unlink(pathname);
}

void lock_start() {
    FIL fd;
    char pathname[256];
    sprintf(pathname, "%s\\pico-nes.lock", HOME_DIR);
    FRESULT fr = f_open(&fd, pathname, FA_CREATE_NEW | FA_WRITE);
    if (fr == FR_OK) {
        UINT bw;
        f_write(&fd, &settings, sizeof(settings), &bw);
        f_close(&fd);
    }
}

bool filebrowser(const char pathname[256], const char* executables) {
    bool debounce = true;
    char basepath[256];
    char tmp[TEXTMODE_COLS + 1];
    strcpy(basepath, pathname);
    constexpr int per_page = TEXTMODE_ROWS - 3;

    DIR dir;
    FILINFO fileInfo;

    while (true) {
        memset(fileItems, 0, sizeof(file_item_t) * max_files);
        int total_files = 0;

        snprintf(tmp, TEXTMODE_COLS, "SD:\\%s", basepath);
        draw_window(tmp, 0, 0, TEXTMODE_COLS, TEXTMODE_ROWS - 1);
        draw_text(nes_psram_available() ? "QSPI PSRAM" : "FLASH",
                  TEXTMODE_COLS - 11, 0, 10, 1);
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
            size_t sz = fileInfo.fsize >> 10;
            fileItems[total_files].size = sz;
            /* Allow Flash fallback for ROMs that are larger than the
             * detected PSRAM but still fit the board's Flash slot. */
            fileItems[total_files].is_executable =
                (fileInfo.fsize <= flash_rom_capacity() ||
                 (nes_psram_available() && fileInfo.fsize <= nes_psram_size())) &&
                isExecutable(fileInfo.fname, executables);
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
                return false;
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

            if (total_files > 0 && (kbd_page_down || kbd_page_up || kbd_home || kbd_end)) {
                const int last = total_files - 1;
                const int per_page_half = per_page >> 1;
                const int max_offset = total_files > per_page_half ? total_files - per_page_half : 0;
                int pos = offset + current_item;
                if (kbd_page_down) {
                    pos += per_page_half;
                    offset += per_page_half;
                }
                else if (kbd_page_up) {
                    pos -= per_page_half;
                    offset -= per_page_half;
                }
                else if (kbd_home) {
                    pos = 0;
                    offset = 0;
                }
                else {
                    pos = last;
                    offset = max_offset;
                }
                if (pos > last) pos = last;
                if (pos < 0) pos = 0;
                if (offset > max_offset) offset = max_offset;
                if (offset < 0) offset = 0;
                if (pos < offset) offset = pos;
                if (pos >= offset + per_page_half) offset = pos - per_page_half + 1;
                current_item = pos - offset;
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
                    char fullpath[256];
                    if (snprintf(fullpath, sizeof fullpath, "%s\\%s", basepath,
                                 file_at_cursor.filename) >= (int)sizeof fullpath)
                        continue;
                    if (filebrowser_loadfile(fullpath)) {
                        save_config();
                        unlock_start();
                        if (rom == (const uint8_t *)NES_PSRAM_BASE) {
                            /* PSRAM contents are lost on power-off; do not
                             * auto-resume the older Flash ROM after reset. */
                            lock_start();
                            return true;
                        }
                        nes_safe_reset(); // Flash ROM: boot from persistent image
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
                    snprintf(tmp, TEXTMODE_COLS - 2, " Size: %dKb, File %lu of %d ", item.size, offset + i + 1, total_files);
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

// NES palette index $0F is black. Rows 0-3 and 236-239 are never drawn by InfoNES and
// SCREEN is shared with the text mode, so clear it before showing the game picture.
static void set_game_graphics_mode() {
    // graphics_set_mode() clears the buffer itself (VGA: memset 0 = NES grey,
    // HDMI: clrScr() writes text-mode spaces), so fill with black after it
    graphics_set_mode(GRAPHICSMODE_DEFAULT);
    memset(&SCREEN[0][0], 0x0F, sizeof SCREEN);
}

// NTSC / PAL / Dendy: the menu setting, or for "Auto" the ROM header,
// then the "(E)" / "(Europe)" tag of the file name for old iNES headers without a TV flag
static BYTE detect_nes_region() {
    if (settings.nes_timing)
        return settings.nes_timing - 1;
    BYTE region = InfoNES_HeaderRegion();
    if (region == NES_REGION_NTSC && (NesHeader.byInfo2 & 0x0c) != 0x08 &&
        (strstr(rom_filename, "(E)") || strstr(rom_filename, "(Europe)") || strstr(rom_filename, "(PAL)")))
        region = NES_REGION_PAL;
    return region;
}

int InfoNES_Video() {
    if (!parseROM(reinterpret_cast<const uint8_t *>(rom))) {
        menu();
        return 0; // TODO: 1?
    }
    InfoNES_SetRegion(detect_nes_region());
    if (InfoNES_Reset() < 0) {
        menu();
        return 1; // TODO: ?
    }
    set_game_graphics_mode();
    return 0;
}

int InfoNES_Menu() {
    graphics_set_mode(TEXTMODE_DEFAULT);
    const bool loaded = filebrowser(HOME_DIR, (char *)"nes");
    set_game_graphics_mode();
    return loaded ? 0 : 1;
}

#ifdef TFT
extern "C" uint8_t INVERSION;
#endif

void load_config() {
    char pathname[256];
    sprintf(pathname, "%s\\pico-nes.cfg", HOME_DIR);
    FIL fd;
    FRESULT fr = f_open(&fd, pathname, FA_READ);
    if (fr != FR_OK) {
        return;
    }
    UINT br;
    f_read(&fd, &settings, sizeof(settings), &br);
    f_close(&fd);
    // older (shorter) configs leave the TV fields at their defaults
    if (settings.tv_system > 1) settings.tv_system = 0;
    if (settings.nes_timing > 3) settings.nes_timing = 0;
#ifdef TFT
    INVERSION = settings.inversion;
#endif
}

void save_config() {
    char pathname[256];
    sprintf(pathname, "%s\\pico-nes.cfg", HOME_DIR);
    FIL fd;
    FRESULT fr = f_open(&fd, pathname, FA_CREATE_ALWAYS | FA_WRITE);
    UINT bw;
    f_write(&fd, &settings, sizeof(settings), &bw);
    f_close(&fd);
}

typedef bool (*menu_callback_t)();

typedef struct __attribute__((__packed__)) {
    const char* text;
    menu_type_e type;
    const void* value;
    menu_callback_t callback;
    uint8_t max_value;
    char value_list[15][10];
} MenuItem;

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
    { "Player 1: %s", ARRAY, &settings.player_1_input, nullptr, 3, { "Keyboard ", "Gamepad 1", "Gamepad 2", "Combined " } },
    { "Player 2: %s", ARRAY, &settings.player_2_input, nullptr, 2, { "Keyboard ", "Gamepad 1", "Gamepad 2" } },
    { "Swap AB <> BA: %s", ARRAY, &settings.swap_ab, nullptr, 1, {"NO ", "YES"} },
    { "" },
    { "Volume: %d", INT, &settings.snd_vol, nullptr, 8 },
#if VGA
{ "" },
    { "Flash line: %s", ARRAY, &settings.flash_line, nullptr,1, { "NO ", "YES" } },
    { "Flash frame: %s", ARRAY, &settings.flash_frame, nullptr,1, { "NO ", "YES" } },
    { "VGA Mode: %s", ARRAY, &settings.palette, nullptr,1, { "RGB333", "RGB222" } },
#endif
#if TFT
    { "" },
    { "Investion %s", ARRAY, &settings.inversion, nullptr, 1, { "NO ", "YES" } },
#endif
#if SOFTTV
    { "" },
    { "TV system %s", ARRAY, &settings.tv_system, nullptr, 1, { "PAL ", "NTSC" } },
    { "Colors: %s", ARRAY, &settings.tv_color, nullptr, 1, { "NO ", "YES" } },
#endif
    { "" },
    { "NES Palette: %s", ARRAY, &settings.nes_palette, nullptr, 1, { "Default ", "Colorful" } },
    { "Timing: %s", ARRAY, &settings.nes_timing, nullptr, 3, { "Auto ", "NTSC ", "PAL  ", "Dendy" } },
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
                            nes_safe_reset();
                        break;
                    case ROM_SELECT:
                        if (gamepad1_bits.start || keyboard_bits.start) {
                            exit = true;
                            /* A new PSRAM ROM starts on the next InfoNES_Main
                             * iteration; cancelling selection resumes this one. */
                            if (InfoNES_Menu() == 0)
                                return ROM_SELECT;
                            break;
                        }
                        break;
                    case USB_DEVICE:
                        if (gamepad1_bits.start || keyboard_bits.start) {
                            clrScr(1);
                            draw_text((char *)"Mount me as USB drive...", 30, 15, 7, 1);
                            // in_flash_drive();
                            nes_safe_reset();
                        }
                }
                if (nullptr != item->callback && (gamepad1_bits.start || keyboard_bits.start)) {
                    exit = item->callback();
                }
            }
            static char result[80];
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
        if (gamepad1_bits.b || (gamepad1_bits.select && !gamepad1_bits.start) ||
            keyboard_bits.b || (keyboard_bits.select && !keyboard_bits.start)
        ) {
            exit = true;
        }
        #ifdef TFT
        if (INVERSION != settings.inversion) {
            save_config();
            f_unmount("");
            nes_safe_reset();
        }
        #endif
        sleep_ms(100);
    }
    graphics_set_flashmode(settings.flash_line, settings.flash_frame);
#if SOFTTV
    graphics_set_tv(settings.tv_system == 0, settings.tv_color);
#endif
    if (ROM) InfoNES_SetRegion(detect_nes_region()); // timing may have been changed in the menu

    updatePalette(settings.palette);
    set_game_graphics_mode();
    save_config();
    return 0;
}


int InfoNES_LoadFrame() {
    if ((keyboard_bits.start || gamepad1_bits.start) && (keyboard_bits.select || gamepad1_bits.select)) {
        if (menu() == ROM_SELECT) {
            return -2; // restart emulation for the newly selected PSRAM ROM
        }
        //if(selected == USB_DEVICE) {
        //    return -2; // TODO: enum
        //}
    }
    frames++;

    return 0;
}

// connection is possible 00->00 (external pull down)
static int test_0000_case(uint32_t pin0, uint32_t pin1, int res) {
    gpio_init(pin0);
    gpio_set_dir(pin0, GPIO_OUT);
    sleep_ms(33);
    gpio_put(pin0, 1);

    gpio_init(pin1);
    gpio_set_dir(pin1, GPIO_IN);
    gpio_pull_down(pin1); /// external pulled down (so, just to ensure)
    sleep_ms(33);
    if ( gpio_get(pin1) ) { // 1 -> 1, looks really connected
        res |= (1 << 5) | 1;
    }
    gpio_deinit(pin0);
    gpio_deinit(pin1);
    return res;
}

// connection is possible 01->01 (no external pull up/down)
static int test_0101_case(uint32_t pin0, uint32_t pin1, int res) {
    gpio_init(pin0);
    gpio_set_dir(pin0, GPIO_OUT);
    sleep_ms(33);
    gpio_put(pin0, 1);

    gpio_init(pin1);
    gpio_set_dir(pin1, GPIO_IN);
    gpio_pull_down(pin1);
    sleep_ms(33);
    if ( gpio_get(pin1) ) { // 1 -> 1, looks really connected
        res |= (1 << 5) | 1;
    }
    gpio_deinit(pin0);
    gpio_deinit(pin1);
    return res;
}

// connection is possible 11->11 (externally pulled up)
static int test_1111_case(uint32_t pin0, uint32_t pin1, int res) {
    gpio_init(pin0);
    gpio_set_dir(pin0, GPIO_OUT);
    sleep_ms(33);
    gpio_put(pin0, 0);

    gpio_init(pin1);
    gpio_set_dir(pin1, GPIO_IN);
    gpio_pull_up(pin1); /// external pulled up (so, just to ensure)
    sleep_ms(33);
    if ( !gpio_get(pin1) ) { // 0 -> 0, looks really connected
        res |= 1;
    }
    gpio_deinit(pin0);
    gpio_deinit(pin1);
    return res;
}

static int testPins(uint32_t pin0, uint32_t pin1) {
    int res = 0b000000;
    /// do not try to test butter psram this way
#ifdef BUTTER_PSRAM_GPIO
    if (pin0 == BUTTER_PSRAM_GPIO || pin1 == BUTTER_PSRAM_GPIO) return res;
#endif
    if (pin0 == PICO_DEFAULT_LED_PIN || pin1 == PICO_DEFAULT_LED_PIN) return res; // LED
    if (pin0 == 23 || pin1 == 23) return res; // SMPS Power
    if (pin0 == 24 || pin1 == 24) return res; // VBus sense
    // try pull down case (passive)
    gpio_init(pin0);
    gpio_set_dir(pin0, GPIO_IN);
    gpio_pull_down(pin0);
    gpio_init(pin1);
    gpio_set_dir(pin1, GPIO_IN);
    gpio_pull_down(pin1);
    sleep_ms(33);
    int pin0vPD = gpio_get(pin0);
    int pin1vPD = gpio_get(pin1);
    gpio_deinit(pin0);
    gpio_deinit(pin1);
    /// try pull up case (passive)
    gpio_init(pin0);
    gpio_set_dir(pin0, GPIO_IN);
    gpio_pull_up(pin0);
    gpio_init(pin1);
    gpio_set_dir(pin1, GPIO_IN);
    gpio_pull_up(pin1);
    sleep_ms(33);
    int pin0vPU = gpio_get(pin0);
    int pin1vPU = gpio_get(pin1);
    gpio_deinit(pin0);
    gpio_deinit(pin1);

    res = (pin0vPD << 4) | (pin0vPU << 3) | (pin1vPD << 2) | (pin1vPU << 1);

    if (pin0vPD == 1) {
        if (pin0vPU == 1) { // pin0vPD == 1 && pin0vPU == 1
            if (pin1vPD == 1) { // pin0vPD == 1 && pin0vPU == 1 && pin1vPD == 1
                if (pin1vPU == 1) { // pin0vPD == 1 && pin0vPU == 1 && pin1vPD == 1 && pin1vPU == 1
                    // connection is possible 11->11 (externally pulled up)
                    return test_1111_case(pin0, pin1, res);
                } else { // pin0vPD == 1 && pin0vPU == 1 && pin1vPD == 1 && pin1vPU == 0
                    // connection is impossible
                    return res;
                }
            } else { // pin0vPD == 1 && pin0vPU == 1 && pin1vPD == 0
                if (pin1vPU == 1) { // pin0vPD == 1 && pin0vPU == 1 && pin1vPD == 0 && pin1vPU == 1
                    // connection is impossible
                    return res;
                } else { // pin0vPD == 1 && pin0vPU == 1 && pin1vPD == 0 && pin1vPU == 0
                    // connection is impossible
                    return res;
                }
            }
        } else {  // pin0vPD == 1 && pin0vPU == 0
            if (pin1vPD == 1) { // pin0vPD == 1 && pin0vPU == 0 && pin1vPD == 1
                if (pin1vPU == 1) { // pin0vPD == 1 && pin0vPU == 0 && pin1vPD == 1 && pin1vPU == 1
                    // connection is impossible
                    return res;
                } else { // pin0vPD == 1 && pin0vPU == 0 && pin1vPD == 1 && pin1vPU == 0
                    // connection is possible 10->10 (pulled up on down, and pulled down on up?)
                    return res |= (1 << 5) | 1; /// NOT SURE IT IS POSSIBLE TO TEST SUCH CASE (TODO: think about real cases)
                }
            } else { // pin0vPD == 1 && pin0vPU == 0 && pin1vPD == 0
                if (pin1vPU == 1) { // pin0vPD == 1 && pin0vPU == 0 && pin1vPD == 0 && pin1vPU == 1
                    // connection is impossible
                    return res;
                } else { // pin0vPD == 1 && pin0vPU == 0 && pin1vPD == 0 && pin1vPU == 0
                    // connection is impossible
                    return res;
                }
            }
        }
    } else { // pin0vPD == 0
        if (pin0vPU == 1) { // pin0vPD == 0 && pin0vPU == 1
            if (pin1vPD == 1) { // pin0vPD == 0 && pin0vPU == 1 && pin1vPD == 1
                if (pin1vPU == 1) { // pin0vPD == 0 && pin0vPU == 1 && pin1vPD == 1 && pin1vPU == 1
                    // connection is impossible
                    return res;
                } else { // pin0vPD == 0 && pin0vPU == 1 && pin1vPD == 1 && pin1vPU == 0
                    // connection is impossible
                    return res;
                }
            } else { // pin0vPD == 0 && pin0vPU == 1 && pin1vPD == 0
                if (pin1vPU == 1) { // pin0vPD == 0 && pin0vPU == 1 && pin1vPD == 0 && pin1vPU == 1
                    // connection is possible 01->01 (no external pull up/down)
                    return test_0101_case(pin0, pin1, res);
                } else { // pin0vPD == 0 && pin0vPU == 1 && pin1vPD == 0 && pin1vPU == 0
                    // connection is impossible
                    return res;
                }
            }
        } else {  // pin0vPD == 0 && pin0vPU == 0
            if (pin1vPD == 1) { // pin0vPD == 0 && pin0vPU == 0 && pin1vPD == 1
                if (pin1vPU == 1) { // pin0vPD == 0 && pin0vPU == 0 && pin1vPD == 1 && pin1vPU == 1
                    // connection is impossible
                    return res;
                } else { // pin0vPD == 0 && pin0vPU == 0 && pin1vPD == 1 && pin1vPU == 0
                    // connection is impossible
                    return res;
                }
            } else { // pin0vPD == 0 && pin0vPU == 0 && pin1vPD == 0
                if (pin1vPU == 1) { // pin0vPD == 0 && pin0vPU == 0 && pin1vPD == 0 && pin1vPU == 1
                    // connection is impossible
                    return res;
                } else { // pin0vPD == 0 && pin0vPU == 0 && pin1vPD == 0 && pin1vPU == 0
                    // connection is possible 00->00 (externally pulled down)
                    return test_0000_case(pin0, pin1, res);
                }
            }
        }
    }
    return res;
}

#if PICO_RP2350
/* Match Gamate's frequency-dependent QMI M0 timing instead of assuming that
 * the same hard-coded dividers are safe at every CPU_FREQ.  Reconfigure the
 * Flash bus BEFORE increasing clk_sys; the old boot clock stays safe, too. */
static void __not_in_flash_func(nes_flash_timings)(uint32_t target_hz) {
    const uint32_t max_flash_hz = 88u * 1000000u;
    uint32_t divisor = (target_hz + max_flash_hz - 1u) / max_flash_hz;
    if (divisor < 2u && target_hz > 100u * 1000000u)
        divisor = 2u;
    uint32_t rxdelay = divisor;
    if (target_hz / divisor > 100u * 1000000u)
        ++rxdelay;
    qmi_hw->m[0].timing = 0x60007000u |
                          (rxdelay << QMI_M0_TIMING_RXDELAY_LSB) |
                          (divisor << QMI_M0_TIMING_CLKDIV_LSB);
}
#endif

int main() {
#if !PICO_RP2040
    vreg_disable_voltage_limit();
    vreg_set_voltage(VREG_VOLTAGE_1_60);
    sleep_ms(33);
    nes_flash_timings(CPU_FREQ * 1000000u);
    set_sys_clock_khz(CPU_FREQ * KHZ, false);
#else
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    set_sys_clock_khz(CPU_FREQ * KHZ, true);
#endif

    /* Configure QMI CS1 before either core starts using ROM pointers.  On
     * RP2040 or boards without PSRAM, the loader retains its Flash fallback. */
    nes_psram_init();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    for (int i = 0; i < 6; i++) {
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

    #if defined(VGA) || defined(HDMI) // SELECT_VGA lives in vga-nextgen: not linked for TFT / TV / SOFTTV
#if ZERO2
    SELECT_VGA = false; // RP2350-PiZero has only mini-HDMI, no VGA
#else
    uint8_t link = testPins(VGA_BASE_PIN, VGA_BASE_PIN + 1);
    SELECT_VGA = (link == 0) || (link == 0x1F);
#endif
    #endif

    // board_init();
    tuh_init(BOARD_TUH_RHPORT);

    memset(&SCREEN[0][0], 0, sizeof SCREEN);
#if USE_PS2_KBD
    ps2kbd.init_gpio();
#endif
#if USE_NESPAD
    /* CS1 on some RP2350A boards shares a header pin with the external NES
     * controller.  Do not let PIO drive the PSRAM chip select.  USB and PS/2
     * input remain available when the external NESpad cannot be enabled. */
    const int cs1_pin = nes_psram_cs1_gpio();
    if (cs1_pin < 0 || (cs1_pin != NES_GPIO_CLK && cs1_pin != NES_GPIO_LAT &&
                        cs1_pin != NES_GPIO_DATA && cs1_pin != NES_GPIO_DATA + 1)) {
        nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);
    } else {
        printf("NESpad disabled: GPIO %d is in use by QSPI PSRAM CS1\n", cs1_pin);
    }
#endif
    f_mount(&fs, "", 1);
    f_mkdir(HOME_DIR);
    load_config();
#if SOFTTV
    graphics_set_tv(settings.tv_system == 0, settings.tv_color);
#endif

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_core);
    sem_release(&vga_start_semaphore);
    // Do not draw the first menu before the video driver has its text buffer:
    // draw_text() into a NULL text_buffer is lost (the file browser frame is drawn only once).
    while (!render_ready) tight_loop_contents();

#ifndef BUILD_IN_GAMES
    if (is_start_locked() || !parseROM(reinterpret_cast<const uint8_t *>(rom))) {
        InfoNES_Menu();
    }
#endif
    lock_start();
    bool start_from_game = InfoNES_Main(true);
    while (1) {
        sleep_ms(500);
        start_from_game = InfoNES_Main(start_from_game);
    }
}
