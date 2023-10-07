#include <cstdio>
#include "pico/stdlib.h"
#include "pico/runtime.h"
#include "hardware/pio.h"
#include "hardware/i2c.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include <hardware/sync.h>
#include <hardware/pwm.h>
#include <pico/multicore.h>
#include <hardware/flash.h>
#include <cstring>
#include <cstdarg>

#include "pico/multicore.h"

#include <InfoNES.h>
#include <InfoNES_System.h>
#include "InfoNES_Mapper.h"
#include "InfoNES_pAPU.h"

#include "vga.h"
#include "audio.h"
#include "f_util.h"
#include "ff.h"
#include "VGA_ROM_F16.h"

#if USE_PS2_KBD

#include "ps2kbd_mrmltr.h"

#endif
#if USE_NESPAD

#include "nespad.h"

#endif

#pragma GCC optimize("Ofast")
static const sVmode *vmode = nullptr;
struct semaphore vga_start_semaphore;
uint8_t SCREEN[NES_DISP_HEIGHT][NES_DISP_WIDTH];
char textmode[30][80];
uint8_t colors[30][80];

bool show_fps = false;

typedef enum {
    RESOLUTION_NATIVE,
    RESOLUTION_TEXTMODE,
} resolution_t;
resolution_t resolution = RESOLUTION_TEXTMODE;

static FATFS fs;

bool saveSettingsAndReboot = false;
#define STATUSINDICATORSTRING "STA"
#define VOLUMEINDICATORSTRING "VOL"

#define FLASH_TARGET_OFFSET (1024 * 1024)
const char *rom_filename = (const char*) (XIP_BASE + FLASH_TARGET_OFFSET);
const uint8_t *rom = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET)+4096;
//static constexpr uintptr_t rom = 0x10110000;         // Location of .nes rom or tar archive with .nes roms
static constexpr uintptr_t NES_BATTERY_SAVE_ADDR = 0x100D0000; // 256K

#define X2(a) (a | (a << 8))
#define VGA_RGB_222(r, g, b) ((r << 4) | (g << 2) | b)

const BYTE __not_in_flash_func(NesPalette)
[64] = {
VGA_RGB_222(0x7c >> 6, 0x7c >> 6, 0x7c >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0xfc >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0xbc >> 6),
VGA_RGB_222(0x44 >> 6, 0x28 >> 6, 0xbc >> 6),

VGA_RGB_222(0x94 >> 6, 0x00 >> 6, 0x84 >> 6),
VGA_RGB_222(0xa8 >> 6, 0x00 >> 6, 0x20 >> 6),
VGA_RGB_222(0xa8 >> 6, 0x10 >> 6, 0x00 >> 6),
VGA_RGB_222(0x88 >> 6, 0x14 >> 6, 0x00 >> 6),

VGA_RGB_222(0x50 >> 6, 0x30 >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0x78 >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0x68 >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0x58 >> 6, 0x00 >> 6),

VGA_RGB_222(0x00 >> 6, 0x40 >> 6, 0x58 >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0x00 >> 6),

VGA_RGB_222(0xbc >> 6, 0xbc >> 6, 0xbc >> 6),
VGA_RGB_222(0x00 >> 6, 0x78 >> 6, 0xf8 >> 6),
VGA_RGB_222(0x00 >> 6, 0x58 >> 6, 0xf8 >> 6),
VGA_RGB_222(0x68 >> 6, 0x44 >> 6, 0xfc >> 6),

VGA_RGB_222(0xd8 >> 6, 0x00 >> 6, 0xcc >> 6),
VGA_RGB_222(0xe4 >> 6, 0x00 >> 6, 0x58 >> 6),
VGA_RGB_222(0xf8 >> 6, 0x38 >> 6, 0x00 >> 6),
VGA_RGB_222(0xe4 >> 6, 0x5c >> 6, 0x10 >> 6),

VGA_RGB_222(0xac >> 6, 0x7c >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0xb8 >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0xa8 >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0xa8 >> 6, 0x44 >> 6),

VGA_RGB_222(0x00 >> 6, 0x88 >> 6, 0x88 >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0x00 >> 6),

VGA_RGB_222(0xf8 >> 6, 0xf8 >> 6, 0xf8 >> 6),
VGA_RGB_222(0x3c >> 6, 0xbc >> 6, 0xfc >> 6),
VGA_RGB_222(0x68 >> 6, 0x88 >> 6, 0xfc >> 6),
VGA_RGB_222(0x98 >> 6, 0x78 >> 6, 0xf8 >> 6),

VGA_RGB_222(0xf8 >> 6, 0x78 >> 6, 0xf8 >> 6),
VGA_RGB_222(0xf8 >> 6, 0x58 >> 6, 0x98 >> 6),
VGA_RGB_222(0xf8 >> 6, 0x78 >> 6, 0x58 >> 6),
VGA_RGB_222(0xfc >> 6, 0xa0 >> 6, 0x44 >> 6),

VGA_RGB_222(0xf8 >> 6, 0xb8 >> 6, 0x00 >> 6),
VGA_RGB_222(0xb8 >> 6, 0xf8 >> 6, 0x18 >> 6),
VGA_RGB_222(0x58 >> 6, 0xd8 >> 6, 0x54 >> 6),
VGA_RGB_222(0x58 >> 6, 0xf8 >> 6, 0x98 >> 6),

VGA_RGB_222(0x00 >> 6, 0xe8 >> 6, 0xd8 >> 6),
VGA_RGB_222(0x78 >> 6, 0x78 >> 6, 0x78 >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0x00 >> 6),

VGA_RGB_222(0xfc >> 6, 0xfc >> 6, 0xfc >> 6),
VGA_RGB_222(0xa4 >> 6, 0xe4 >> 6, 0xfc >> 6),
VGA_RGB_222(0xb8 >> 6, 0xb8 >> 6, 0xf8 >> 6),
VGA_RGB_222(0xd8 >> 6, 0xb8 >> 6, 0xf8 >> 6),

VGA_RGB_222(0xf8 >> 6, 0xb8 >> 6, 0xf8 >> 6),
VGA_RGB_222(0xf8 >> 6, 0xa4 >> 6, 0xc0 >> 6),
VGA_RGB_222(0xf0 >> 6, 0xd0 >> 6, 0xb0 >> 6),
VGA_RGB_222(0xfc >> 6, 0xe0 >> 6, 0xa8 >> 6),

VGA_RGB_222(0xf8 >> 6, 0xd8 >> 6, 0x78 >> 6),
VGA_RGB_222(0xd8 >> 6, 0xf8 >> 6, 0x78 >> 6),
VGA_RGB_222(0xb8 >> 6, 0xf8 >> 6, 0xb8 >> 6),
VGA_RGB_222(0xb8 >> 6, 0xf8 >> 6, 0xd8 >> 6),

VGA_RGB_222(0x00 >> 6, 0xfc >> 6, 0xfc >> 6),
VGA_RGB_222(0xf8 >> 6, 0xd8 >> 6, 0xf8 >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0x00 >> 6),
VGA_RGB_222(0x00 >> 6, 0x00 >> 6, 0x00 >> 6),
};

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
static input_bits_t gamepad_bits = { false, false, false, false, false, false, false, false };

#if USE_NESPAD

void nespad_tick() {
    nespad_read();

    gamepad_bits.a = (nespad_state & DPAD_A) != 0;
    gamepad_bits.b = (nespad_state & DPAD_B) != 0;
    gamepad_bits.select = (nespad_state & DPAD_SELECT) != 0;
    gamepad_bits.start = (nespad_state & DPAD_START) != 0;
    gamepad_bits.up = (nespad_state & DPAD_UP) != 0;
    gamepad_bits.down = (nespad_state & DPAD_DOWN) != 0;
    gamepad_bits.left = (nespad_state & DPAD_LEFT) != 0;
    gamepad_bits.right = (nespad_state & DPAD_RIGHT) != 0;
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
    /* printf("HID key report modifiers %2.2X report ", report->modifier);
    for (unsigned char i: report->keycode)
        printf("%2.2X", i);
    printf("\r\n");
     */
    keyboard_bits.start = isInReport(report, HID_KEY_ENTER);
    keyboard_bits.select = isInReport(report, HID_KEY_BACKSPACE);
    keyboard_bits.a = isInReport(report, HID_KEY_Z);
    keyboard_bits.b = isInReport(report, HID_KEY_X);
    keyboard_bits.up = isInReport(report, HID_KEY_ARROW_UP);
    keyboard_bits.down = isInReport(report, HID_KEY_ARROW_DOWN);
    keyboard_bits.left = isInReport(report, HID_KEY_ARROW_LEFT);
    keyboard_bits.right = isInReport(report, HID_KEY_ARROW_RIGHT);
    //-------------------------------------------------------------------------
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

void draw_text(char *text, uint8_t x, uint8_t y, uint8_t color, uint8_t bgcolor) {
    uint8_t len = strlen(text);
    len = len < 80 ? len : 80;
    memcpy(&textmode[y][x], text, len);
    memset(&colors[y][x], (color << 4) | (bgcolor & 0xF), len);
}

uint32_t getCurrentNVRAMAddr() {
    int slot = 0;

    printf("SRAM slot %d\n", slot);
    // Save Games are stored towards address stored roms.
    // calculate address of save game slot
    // slot 0 is reserved. (Some state variables are stored at this location)
    uint32_t saveLocation = NES_BATTERY_SAVE_ADDR + SRAM_SIZE * (slot + 1);
    if (saveLocation >= FLASH_TARGET_OFFSET) {
        printf("No more save slots available, (Requested slot = %d)", slot);
        return {};
    }
    return saveLocation;
}

// Positions in SRAM for storing state variables
#define STATUSINDICATORPOS 0
#define GAMEINDEXPOS 3
#define ADVANCEPOS 4
#define VOLUMEINDICATORPOS 5

// Save NES Battery RAM (about 58 Games exist with save battery)
// Problem: First call to saveNVRAM  after power up is ok
// Second call  causes a crash in flash_range_erase()
// Because of this we reserve one flash block for saving state of current played game, selected action and sound settings.
// Then the RP2040 will always be rebooted
// After reboot, the state will be restored.
void saveNVRAM(uint8_t statevar, char advance) {
    static_assert((SRAM_SIZE & (FLASH_SECTOR_SIZE - 1)) == 0);
    printf("save SRAM and/or settings\n");
    if (!SRAMwritten && !saveSettingsAndReboot) {
        printf("  SRAM not updated and no audio settings changed.\n");
        return;
    }

    // Disable core 1 to prevent RP2040 from crashing while writing to flash.
    printf("  resetting Core 1\n");
    multicore_reset_core1();
    uint32_t ints = save_and_disable_interrupts();

    uint32_t addr = getCurrentNVRAMAddr();
    uint32_t ofs = addr - XIP_BASE;
    if (addr) {
        printf("  write SRAM to flash %x --> %lx\n", addr, ofs);
        flash_range_erase(ofs, SRAM_SIZE);
        flash_range_program(ofs, SRAM, SRAM_SIZE);
    }
    // Save state variables
    // - Current game index
    // - Advance (+ Next, - Previous, B Build-in game, R  Reset)
    // - Speaker mode
    // - Volume
    printf("  write state variables and sound settings to flash\n");

    SRAM[STATUSINDICATORPOS] = STATUSINDICATORSTRING[0];
    SRAM[STATUSINDICATORPOS + 1] = STATUSINDICATORSTRING[1];
    SRAM[STATUSINDICATORPOS + 2] = STATUSINDICATORSTRING[2];
    SRAM[GAMEINDEXPOS] = statevar;
    SRAM[ADVANCEPOS] = advance;
    SRAM[VOLUMEINDICATORPOS] = VOLUMEINDICATORSTRING[0];
    SRAM[VOLUMEINDICATORPOS + 1] = VOLUMEINDICATORSTRING[1];
    SRAM[VOLUMEINDICATORPOS + 2] = VOLUMEINDICATORSTRING[2];

    // first block of flash is reserved for storing state variables
    uint32_t state = NES_BATTERY_SAVE_ADDR - XIP_BASE;
    flash_range_erase(state, SRAM_SIZE);
    flash_range_program(state, SRAM, SRAM_SIZE);

    printf("  done\n");
    printf("  Rebooting...\n");
    restore_interrupts(ints);
    // Reboot after SRAM is flashed
    watchdog_enable(100, 1);
    while (1);

    SRAMwritten = false;
    // reboot
}

bool loadNVRAM() {
    if (auto addr = getCurrentNVRAMAddr()) {
        printf("load SRAM %x\n", addr);
        memcpy(SRAM, reinterpret_cast<void *>(addr), SRAM_SIZE);
    }
    SRAMwritten = false;
    return true;
}

void loadState() {
    memcpy(SRAM, reinterpret_cast<void *>(NES_BATTERY_SAVE_ADDR), SRAM_SIZE);
}

static int rapidFireMask = 0;
static int rapidFireCounter = 0;

void InfoNES_PadState(DWORD *pdwPad1, DWORD *pdwPad2, DWORD *pdwSystem) {
    static constexpr int LEFT = 1 << 6;
    static constexpr int RIGHT = 1 << 7;
    static constexpr int UP = 1 << 4;
    static constexpr int DOWN = 1 << 5;
    static constexpr int SELECT = 1 << 2;
    static constexpr int START = 1 << 3;
    static constexpr int A = 1 << 0;
    static constexpr int B = 1 << 1;

    int gamepad_state = ((keyboard_bits.left || gamepad_bits.left) ? LEFT : 0) |
                        ((keyboard_bits.right || gamepad_bits.right) ? RIGHT : 0) |
                        ((keyboard_bits.up || gamepad_bits.up) ? UP : 0) |
                        ((keyboard_bits.down || gamepad_bits.down) ? DOWN : 0) |
                        ((keyboard_bits.start || gamepad_bits.start) ? START : 0) |
                        ((keyboard_bits.select || gamepad_bits.select) ? SELECT : 0) |
                        ((keyboard_bits.a || gamepad_bits.a) ? A : 0) |
                        ((keyboard_bits.b || gamepad_bits.b) ? B : 0) |
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


    //prevButtons = gamepad_state;

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

i2s_config_t i2s_config;
void InfoNES_SoundInit() {
    i2s_config = i2s_get_default_config();
    i2s_config.sample_freq = 44100;
    i2s_config.dma_trans_count = i2s_config.sample_freq / 50;
    i2s_volume(&i2s_config, 0);
    i2s_init(&i2s_config);
}


int InfoNES_SoundOpen(int samples_per_sync, int sample_rate) {
    return 0;
}

void InfoNES_SoundClose() {
}

int __not_in_flash_func(InfoNES_GetSoundBufferSize)
()
{
return 735;
}

#define buffermax 1280
void InfoNES_SoundOutput(int samples, const BYTE *wave1, const BYTE *wave2, const BYTE *wave3, const BYTE *wave4,
                         const BYTE *wave5) {
    static int16_t samples_out[2][buffermax*2];
    static int i_active_buf=0;
    static int inx=0;




    for (int i = 0; i < samples; i++)
    {
        int r,l;

        //mono
//            l=(((unsigned char)wave1[i] + (unsigned char)wave2[i] + (unsigned char)wave3[i] + (unsigned char)wave4[i] + (unsigned char)wave5[i])-640)<<5;
//            r=l;

        int w1 = *wave1++;
        int w2 = *wave2++;
        int w3 = *wave3++;
        int w4 = *wave4++;
        int w5 = *wave5++;
        //            w3 = w2 = w4 = w5 = 0;
        l = w1 * 6 + w2 * 3 + w3 * 5 + w4 * 3 * 17 + w5 * 2 * 32;
        r = w1 * 3 + w2 * 6 + w3 * 5 + w4 * 3 * 17 + w5 * 2 * 32;

        samples_out[i_active_buf][inx*2]=l*2;
        samples_out[i_active_buf][inx*2+1]=r*2;
        if(inx++>=i2s_config.dma_trans_count)
        {
            inx=0;
            i2s_dma_write(&i2s_config, reinterpret_cast<const int16_t *>(samples_out[i_active_buf]));
            i_active_buf^=1;

        }
    }

}


WORD lb[256];


void __not_in_flash_func(InfoNES_PreDrawLine)(int line){
InfoNES_SetLineBuffer(lb, NES_DISP_WIDTH);
}

#define X2(a) (a | (a << 8))
void __not_in_flash_func(InfoNES_PostDrawLine)(int line){
for(int x = 0;x< NES_DISP_WIDTH; x++) SCREEN[line][x] = lb[x];
}


#define CHECK_BIT(var, pos) (((var)>>(pos)) & 1)

/* Renderer loop on Pico's second core */
void __time_critical_func(render_loop)() {
    multicore_lockout_victim_init();
    VgaLineBuf *linebuf;
    printf("Video on Core#%i running...\n", get_core_num());

    sem_acquire_blocking(&vga_start_semaphore);
    VgaInit(vmode, 640, 480);
    uint32_t y;
    while (linebuf = get_vga_line()) {
        y = linebuf->row;

        switch (resolution) {
            case RESOLUTION_TEXTMODE:
                for (uint8_t x = 0; x < 80; x++) {
                    uint8_t glyph_row = VGA_ROM_F16[(textmode[y / 16][x] * 16) + y % 16];
                    uint8_t color = colors[y / 16][x];

                    for (uint8_t bit = 0; bit < 8; bit++) {
                        if (CHECK_BIT(glyph_row, bit)) {
                            // FOREGROUND
                            linebuf->line[8 * x + bit] = (color >> 4) & 0xF;
                        } else {
                            // BACKGROUND
                            linebuf->line[8 * x + bit] = color & 0xF;
                        }
                    }
                }
                break;
            case RESOLUTION_NATIVE:
                for (int x = 0; x < NES_DISP_WIDTH * 2; x += 2)
                    (uint16_t &) linebuf->line[64 + x] = X2(SCREEN[y][x >> 1]);
                // SHOW FPS
                if (show_fps && y < 16) {
                    for (uint8_t x = 77; x < 80; x++) {
                        uint8_t glyph_row = VGA_ROM_F16[(textmode[y / 16][x] * 16) + y % 16];

                        for (uint8_t bit = 0; bit < 8; bit++) {
                            if (CHECK_BIT(glyph_row, bit)) {
                                // FOREGROUND
                                linebuf->line[8 * x + bit] = 11;
                            } else {
                                linebuf->line[8 * x + bit] = 0;
                            }
                        }
                    }
                }
        }
    }
}


void fileselector_load(char *pathname) {
    if (strcmp(rom_filename, pathname) == 0) {
        printf("Launching last rom");
        return;
    }

    FIL file;
    size_t bufsize = sizeof(SCREEN)&0xfffff000;
    BYTE *buffer = (BYTE *) SCREEN;
    auto offset = FLASH_TARGET_OFFSET;
    UINT bytesRead;

    printf("Writing %s rom to flash %x\r\n", pathname, offset);
    FRESULT result = f_open(&file, pathname, FA_READ);


    if (result == FR_OK) {
        uint32_t interrupts = save_and_disable_interrupts();
        multicore_lockout_start_blocking();

        // TODO: Save it after success loading to prevent corruptions
        printf("Flashing %d bytes to flash address %x\r\n", 256, offset);
        flash_range_erase(offset, 4096);
        flash_range_program(offset, reinterpret_cast<const uint8_t *>(pathname), 256);

        offset += 4096;
        for (;;) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            result = f_read(&file, buffer, bufsize, &bytesRead);
            if (result == FR_OK) {
                if (bytesRead == 0) {
                    break;
                }

                printf("Flashing %d bytes to flash address %x\r\n", bytesRead, offset);

                printf("Erasing...");
                // Disable interupts, erase, flash and enable interrupts
                gpio_put(PICO_DEFAULT_LED_PIN, false);
                flash_range_erase(offset, bufsize);
                printf("  -> Flashing...\r\n");
                flash_range_program(offset, buffer, bufsize);

                offset += bufsize;
            } else {
                printf("Error reading rom: %d\n", result);
                break;
            }
        }

        f_close(&file);
        restore_interrupts(interrupts);
        multicore_lockout_end_blocking();
    }
}

uint16_t fileselector_display_page(char filenames[28][256], uint16_t page_number) {
    memset(&textmode, 0x00, sizeof(textmode));
    memset(&colors, 0x00, sizeof(colors));
    char footer[80];
    sprintf(footer, "=================== PAGE #%i -> NEXT PAGE / <- PREV. PAGE ====================", page_number);
    draw_text(footer, 0, 14, 3, 11);

    DIR directory;
    FILINFO file;
    FRESULT result = f_mount(&fs, "", 1);
    if (FR_OK != result) {
        printf("E f_mount error: %s (%d)\n", FRESULT_str(result), result);
        return 0;
    }

    /* clear the filenames array */
    for (uint8_t ifile = 0; ifile < 28; ifile++) {
        strcpy(filenames[ifile], "");
    }

    uint16_t total_files = 0;
    result = f_findfirst(&directory, &file, "NES\\", "*.nes");

    /* skip the first N pages */
    if (page_number > 0) {
        while (total_files < page_number * 14 && result == FR_OK && file.fname[0]) {
            total_files++;
            result = f_findnext(&directory, &file);
        }
    }

    /* store the filenames of this page */
    total_files = 0;
    while (total_files < 14 && result == FR_OK && file.fname[0]) {
        strcpy(filenames[total_files], file.fname);
        total_files++;
        result = f_findnext(&directory, &file);
    }
    f_closedir(&directory);

    for (uint8_t ifile = 0; ifile < total_files; ifile++) {
        char pathname[255];
        uint8_t color =  0x0d;
        sprintf(pathname, "NES\\%s", filenames[ifile]);

        if (strcmp(pathname, rom_filename) != 0) {
            color = 0xFF;
        }
        draw_text(filenames[ifile], 0, ifile, color, 0x00);
    }
    return total_files;
}

void fileselector() {
    uint16_t page_number = 0;
    char filenames[30][256];

    printf("Selecting ROM\r\n");

    /* display the first page with up to 22 rom files */
    uint16_t total_files = fileselector_display_page(filenames, page_number);

    /* select the first rom */
    uint8_t current_file = 0;

    uint8_t color = 0xFF;
    draw_text(filenames[current_file], 0, current_file, color, 0xF8);

    while (true) {
        char pathname[255];
        sprintf(pathname, "NES\\%s", filenames[current_file]);

        if (strcmp(pathname, rom_filename) != 0) {
            color = 0xFF;
        } else {
            color = 0x0d;
        }
#if USE_PS2_KBD
        ps2kbd.tick();
#endif

#if USE_NESPAD
        nespad_tick();
#endif
        sleep_ms(33);
#if USE_NESPAD
        nespad_tick();
#endif
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
        if (keyboard_bits.select || gamepad_bits.select) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            break;
        }

        if (keyboard_bits.start || gamepad_bits.start || gamepad_bits.a || gamepad_bits.b) {
            /* copy the rom from the SD card to flash and start the game */
            fileselector_load(pathname);
            break;
        }
        if (keyboard_bits.down || gamepad_bits.down) {
            /* select the next rom */
            draw_text(filenames[current_file], 0, current_file, color, 0x00);
            current_file++;
            if (current_file >= total_files)
                current_file = 0;
            draw_text(filenames[current_file], 0, current_file, color, 0xF8);
            sleep_ms(150);
        }
        if (keyboard_bits.up || gamepad_bits.up) {
            /* select the previous rom */
            draw_text(filenames[current_file], 0, current_file, color, 0x00);
            if (current_file == 0) {
                current_file = total_files - 1;
            } else {
                current_file--;
            }
            draw_text(filenames[current_file], 0, current_file, color, 0xF8);
            sleep_ms(150);
        }
        if (keyboard_bits.right || gamepad_bits.right) {
            /* select the next page */
            page_number++;
            total_files = fileselector_display_page(filenames, page_number);
            if (total_files == 0) {
                /* no files in this page, go to the previous page */
                page_number--;
                total_files = fileselector_display_page(filenames, page_number);
            }
            /* select the first file */
            current_file = 0;
            draw_text(filenames[current_file], 0, current_file, color, 0xF8);
            sleep_ms(150);
        }
        if ((keyboard_bits.left || gamepad_bits.left) && page_number > 0) {
            /* select the previous page */
            page_number--;
            total_files = fileselector_display_page(filenames, page_number);
            /* select the first file */
            current_file = 0;
            draw_text(filenames[current_file], 0, current_file, color, 0xF8);
            sleep_ms(150);
        }
        tight_loop_contents();
    }
}


bool loadAndReset() {
    fileselector();
    memset(&textmode, 0x00, sizeof(textmode));
    memset(&colors, 0x00, sizeof(colors));
    memset(SCREEN, 0x0, sizeof(SCREEN));
    resolution = RESOLUTION_NATIVE;

    if (!parseROM(reinterpret_cast<const uint8_t *>(rom))) {
        printf("NES file parse error.\n");
        return false;
    }

    loadNVRAM();

    if (InfoNES_Reset() < 0) {
        printf("NES reset error.\n");
        return false;
    }

    return true;
}

int InfoNES_Menu() {
    return loadAndReset() ? 0 : -1;
}

#define MENU_ITEMS_NUMBER 3
#if MENU_ITEMS_NUMBER > 15
error("Too much menu items!")
#endif
const char menu_items[MENU_ITEMS_NUMBER][80] = {
        { "Show FPS %i  " },
        { "Reset to ROM select" },
        { "Return to game" },
};



void *menu_values[MENU_ITEMS_NUMBER] = {
        &show_fps,
        nullptr,
        nullptr,
};

void menu() {
    bool exit = false;
    resolution_t old_resolution = resolution;
    memset(&textmode, 0x00, sizeof(textmode));
    memset(&colors, 0x00, sizeof(colors));
    resolution = RESOLUTION_TEXTMODE;

    int current_item = 0;
    char item[80];

    while (!exit) {
        nespad_read();
        sleep_ms(25);
        nespad_read();

        if ((nespad_state & DPAD_DOWN) != 0) {
            if (current_item < MENU_ITEMS_NUMBER - 1) {
                current_item++;
            } else {
                current_item = 0;
            }
        }

        if ((nespad_state & DPAD_UP) != 0) {
            if (current_item > 0) {
                current_item--;
            } else {
                current_item = MENU_ITEMS_NUMBER - 1;
            }
        }

        if ((nespad_state & DPAD_LEFT) != 0 || (nespad_state & DPAD_RIGHT) != 0) {
            switch (current_item) {
                case 0:  // show fps
                    show_fps = !show_fps;
                    break;
            }
        }

        if ((nespad_state & DPAD_START) != 0 || (nespad_state & DPAD_A) != 0 || (nespad_state & DPAD_B) != 0) {
            switch (current_item) {
                case MENU_ITEMS_NUMBER - 2:
                    watchdog_enable(100, true);
                    while (true);
                    break;
                case MENU_ITEMS_NUMBER - 1:
                    exit = true;
                    break;
            }
        }

        for (int i = 0; i < MENU_ITEMS_NUMBER; i++) {
            // TODO: textmode maxy from define
            uint8_t y = i + ((15 - MENU_ITEMS_NUMBER) >> 1);
            uint8_t x = 30;
            uint8_t color = 0xFF;
            uint8_t bg_color = 0x00;
            if (current_item == i) {
                color = 0x01;
                bg_color = 0xFF;
            }
            if (strstr(menu_items[i], "%s") != nullptr) {
                sprintf(item, menu_items[i], menu_values[i]);
            } else {
                sprintf(item, menu_items[i], *(uint8_t *) menu_values[i]);
            }
            draw_text(item, x, y, color, bg_color);
        }

        sleep_ms(100);
    }

    resolution = old_resolution;
}

int start_time;
int frames, frame_cnt;
int frame_timer_start;
int InfoNES_LoadFrame() {
#if USE_PS2_KBD
    ps2kbd.tick();
#endif

#if USE_NESPAD
    nespad_tick();
#endif
    if ((keyboard_bits.start || gamepad_bits.start) && (keyboard_bits.select || gamepad_bits.select)) {
        menu();
    }

    frames++;

    if (frames == 60) {
        uint64_t end_time;
        uint32_t diff;
        uint8_t fps;
        end_time = time_us_64();
        diff = end_time - start_time;
        fps = ((uint64_t) frames * 1000 * 1000) / diff;
        char fps_text[3];
        sprintf(fps_text, "%i ", fps);
        draw_text(fps_text, 77, 0, 0xFF, 0x00);
        frames = 0;
        start_time = time_us_64();
    }
    return 0;
}

int main() {
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(33);
    set_sys_clock_khz(288000, true);

#if !NDEBUG
    stdio_init_all();
#endif

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);


    for (int i = 0; i < 6; i++) {
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

    printf("Start program\n");

    sleep_ms(50);
    vmode = Video(DEV_VGA, RES_HVGA);
    sleep_ms(50);

#if USE_PS2_KBD
    printf("PS2 KBD ");
    ps2kbd.init_gpio();
#endif

#if USE_NESPAD
    nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);
#endif

    // util::dumpMemory((void *)NES_FILE_ADDR, 1024);
    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_loop);
    sem_release(&vga_start_semaphore);

    loadState();
    InfoNES_Main();
}
