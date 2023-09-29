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
const uint8_t *rom = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);
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

struct joypad_bits_t {
    bool a: true;
    bool b: true;
    bool select: true;
    bool start: true;
    bool right: true;
    bool left: true;
    bool up: true;
    bool down: true;
};
static joypad_bits_t gamepad_bits = { false, false, false, false, false, false, false, false };
static joypad_bits_t gamepad_bits_joy1 = { false, false, false, false, false, false, false, false };
static joypad_bits_t gamepad_bits_joy2 = { false, false, false, false, false, false, false, false };

#if USE_NESPAD

void nespad_tick() {
    nespad_read();

    gamepad_bits_joy1.a = (nespad_state & 0x01) != 0;
    gamepad_bits_joy1.b = (nespad_state & 0x02) != 0;
    gamepad_bits_joy1.select = (nespad_state & 0x04) != 0;
    gamepad_bits_joy1.start = (nespad_state & 0x08) != 0;
    gamepad_bits_joy1.up = (nespad_state & 0x10) != 0;
    gamepad_bits_joy1.down = (nespad_state & 0x20) != 0;
    gamepad_bits_joy1.left = (nespad_state & 0x40) != 0;
    gamepad_bits_joy1.right = (nespad_state & 0x80) != 0;
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
    gamepad_bits.start = isInReport(report, 0x28);
    gamepad_bits.select = isInReport(report, 0x2A);
    gamepad_bits.a = isInReport(report, 0x1D);
    gamepad_bits.b = isInReport(report, 0x1B);
    gamepad_bits.up = isInReport(report, 0x52);
    gamepad_bits.down = isInReport(report, 0x51);
    gamepad_bits.left = isInReport(report, 0x50);
    gamepad_bits.right = isInReport(report, 0x4F);
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

    int gamepad_state = ((gamepad_bits.left||gamepad_bits_joy1.left) ? LEFT : 0) |
                        ((gamepad_bits.right||gamepad_bits_joy1.right) ? RIGHT : 0) |
                        ((gamepad_bits.up||gamepad_bits_joy1.up) ? UP : 0) |
                        ((gamepad_bits.down||gamepad_bits_joy1.down) ? DOWN : 0) |
                        ((gamepad_bits.start||gamepad_bits_joy1.start) ? START : 0) |
                        ((gamepad_bits.select||gamepad_bits_joy1.select) ? SELECT : 0) |
                        ((gamepad_bits.a||gamepad_bits_joy1.a) ? A : 0) |
                        ((gamepad_bits.b||gamepad_bits_joy1.b) ? B : 0) |
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
    i2s_config.dma_trans_count = i2s_config.sample_freq / 60;
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


int InfoNES_LoadFrame() {
#if USE_PS2_KBD
    ps2kbd.tick();
#endif

#if USE_NESPAD
    nespad_tick();
#endif
    return 0;
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
        }
    }
}


/**
 * Load a .gb rom file in flash from the SD card
 */
void load_cart_rom_file(char *filename) {
    extern unsigned char VRAM[];
    FIL fil;
    FRESULT fr;

    size_t bufsize = sizeof(SCREEN);
    BYTE *buffer = (BYTE *) SCREEN;
    auto ofs = FLASH_TARGET_OFFSET;
    printf("Writing %s rom to flash %x\r\n", filename, ofs);
    fr = f_open(&fil, filename, FA_READ);

    UINT bytesRead;
    if (fr == FR_OK) {
        for (;;) {
            fr = f_read(&fil, buffer, bufsize, &bytesRead);
            if (fr == FR_OK) {
                if (bytesRead == 0) {
                    break;
                }

                printf("Flashing %d bytes to flash address %x\r\n", bytesRead, ofs);

                printf("Erasing...");
                // Disable interupts, erase, flash and enable interrupts
                uint32_t ints = save_and_disable_interrupts();
                multicore_lockout_start_blocking();

                flash_range_erase(ofs, bufsize);
                printf("  -> Flashing...\r\n");
                flash_range_program(ofs, buffer, bufsize);
                multicore_lockout_end_blocking();
                restore_interrupts(ints);
                ofs += bufsize;
            } else {
                printf("Error reading rom: %d\n", fr);
                break;
            }
        }


        f_close(&fil);
    }
}

/**
 * Function used by the rom file selector to display one page of .gb rom files
 */
uint16_t rom_file_selector_display_page(char filename[28][256], uint16_t num_page) {
    // Dirty screen cleanup
    memset(&textmode, 0x00, sizeof(textmode));
    memset(&colors, 0x00, sizeof(colors));
    char footer[80];
    sprintf(footer, "=================== PAGE #%i -> NEXT PAGE / <- PREV. PAGE ====================", num_page);
    draw_text(footer, 0, 14, 3, 11);

    DIR dj;
    FILINFO fno;
    FRESULT fr;

    fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        printf("E f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return 0;
    }

    /* clear the filenames array */
    for (uint8_t ifile = 0; ifile < 28; ifile++) {
        strcpy(filename[ifile], "");
    }

    /* search *.gb files */
    uint16_t num_file = 0;
    fr = f_findfirst(&dj, &fno, "NES\\", "*.nes");

    /* skip the first N pages */
    if (num_page > 0) {
        while (num_file < num_page * 14 && fr == FR_OK && fno.fname[0]) {
            num_file++;
            fr = f_findnext(&dj, &fno);
        }
    }

    /* store the filenames of this page */
    num_file = 0;
    while (num_file < 14 && fr == FR_OK && fno.fname[0]) {
        strcpy(filename[num_file], fno.fname);
        num_file++;
        fr = f_findnext(&dj, &fno);
    }
    f_closedir(&dj);

    /* display *.gb rom files on screen */
    // mk_ili9225_fill(0x0000);
    for (uint8_t ifile = 0; ifile < num_file; ifile++) {
        draw_text(filename[ifile], 0, ifile, 0xFF, 0x00);
    }
    return num_file;
}

/**
 * The ROM selector displays pages of up to 22 rom files
 * allowing the user to select which rom file to start
 * Copy your *.gb rom files to the root directory of the SD card
 */
void rom_file_selector() {
    uint16_t num_page = 0;
    char filenames[30][256];

    printf("Selecting ROM\r\n");

    /* display the first page with up to 22 rom files */
    uint16_t numfiles = rom_file_selector_display_page(filenames, num_page);

    /* select the first rom */
    uint8_t selected = 0;
    draw_text(filenames[selected], 0, selected, 0xFF, 0xF8);

    while (true) {

#if USE_PS2_KBD
        ps2kbd.tick();
#endif

#if USE_NESPAD
        nespad_tick();
#endif
        sleep_ms(100);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
        if (gamepad_bits.start||gamepad_bits_joy1.start||gamepad_bits_joy1.a||gamepad_bits_joy1.b) {
            /* copy the rom from the SD card to flash and start the game */
            char pathname[255];
            sprintf(pathname, "NES\\%s", filenames[selected]);
            load_cart_rom_file(pathname);
            break;
        }
        if (gamepad_bits.down||gamepad_bits_joy1.down) {
            /* select the next rom */
            draw_text(filenames[selected], 0, selected, 0xFF, 0x00);
            selected++;
            if (selected >= numfiles)
                selected = 0;
            draw_text(filenames[selected], 0, selected, 0xFF, 0xF8);
            sleep_ms(150);
        }
        if (gamepad_bits.up||gamepad_bits_joy1.up) {
            /* select the previous rom */
            draw_text(filenames[selected], 0, selected, 0xFF, 0x00);
            if (selected == 0) {
                selected = numfiles - 1;
            } else {
                selected--;
            }
            draw_text(filenames[selected], 0, selected, 0xFF, 0xF8);
            sleep_ms(150);
        }
        if (gamepad_bits.right||gamepad_bits_joy1.right) {
            /* select the next page */
            num_page++;
            numfiles = rom_file_selector_display_page(filenames, num_page);
            if (numfiles == 0) {
                /* no files in this page, go to the previous page */
                num_page--;
                numfiles = rom_file_selector_display_page(filenames, num_page);
            }
            /* select the first file */
            selected = 0;
            draw_text(filenames[selected], 0, selected, 0xFF, 0xF8);
            sleep_ms(150);
        }
        if ((gamepad_bits.left||gamepad_bits_joy1.left) && num_page > 0) {
            /* select the previous page */
            num_page--;
            numfiles = rom_file_selector_display_page(filenames, num_page);
            /* select the first file */
            selected = 0;
            draw_text(filenames[selected], 0, selected, 0xFF, 0xF8);
            sleep_ms(150);
        }
        tight_loop_contents();
    }
}


bool loadAndReset() {
    rom_file_selector();
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

int main() {
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    set_sys_clock_khz(288000, true);

    stdio_init_all();

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

    // When system is rebooted after flashing SRAM, load the saved state and volume from flash and proceed.
    loadState();

    while (true) {
        //printf("Starting '%s'.\n", romSelector_.GetCurrentGameName());
        InfoNES_Main();
        tight_loop_contents();
    }
}
