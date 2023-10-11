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

#define FLASH_TARGET_OFFSET (1024 * 1024)
const char *rom_filename = (const char *) (XIP_BASE + FLASH_TARGET_OFFSET);
const uint8_t *rom = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET) + 4096;

struct semaphore vga_start_semaphore;
uint8_t SCREEN[NES_DISP_HEIGHT][NES_DISP_WIDTH];
uint16_t linebuffer[256];

// SETTINGS
bool show_fps = false;
bool flash_line = true;
bool flash_frame = true;
uint8_t snd_vol = 8;
uint8_t player_1_input = 1;
uint8_t player_2_input = 0;

static FATFS fs;

i2s_config_t i2s_config;

int start_time;
int frames;

uint8_t PALETTE(uint8_t r, uint8_t g, uint8_t b) {
    static uint8_t pal_index = 0;
    pal_index++;
    setVGA_color_palette(pal_index - 1, (r << 16) | (g << 8) | b);
    return (pal_index - 1);
}

const BYTE __not_in_flash_func(NesPalette)
[64] = {
PALETTE(0x7c, 0x7c, 0x7c),
PALETTE(0x00, 0x00, 0xfc),
PALETTE(0x00, 0x00, 0xbc),
PALETTE(0x44, 0x28, 0xbc),

PALETTE(0x94, 0x00, 0x84),
PALETTE(0xa8, 0x00, 0x20),
PALETTE(0xa8, 0x10, 0x00),
PALETTE(0x88, 0x14, 0x00),

PALETTE(0x50, 0x30, 0x00),
PALETTE(0x00, 0x78, 0x00),
PALETTE(0x00, 0x68, 0x00),
PALETTE(0x00, 0x58, 0x00),

PALETTE(0x00, 0x40, 0x58),
PALETTE(0x00, 0x00, 0x00),
PALETTE(0x00, 0x00, 0x00),
PALETTE(0x00, 0x00, 0x00),

PALETTE(0xbc, 0xbc, 0xbc),
PALETTE(0x00, 0x78, 0xf8),
PALETTE(0x00, 0x58, 0xf8),
PALETTE(0x68, 0x44, 0xfc),

PALETTE(0xd8, 0x00, 0xcc),
PALETTE(0xe4, 0x00, 0x58),
PALETTE(0xf8, 0x38, 0x00),
PALETTE(0xe4, 0x5c, 0x10),

PALETTE(0xac, 0x7c, 0x00),
PALETTE(0x00, 0xb8, 0x00),
PALETTE(0x00, 0xa8, 0x00),
PALETTE(0x00, 0xa8, 0x44),

PALETTE(0x00, 0x88, 0x88),
PALETTE(0x00, 0x00, 0x00),
PALETTE(0x00, 0x00, 0x00),
PALETTE(0x00, 0x00, 0x00),

PALETTE(0xf8, 0xf8, 0xf8),
PALETTE(0x3c, 0xbc, 0xfc),
PALETTE(0x68, 0x88, 0xfc),
PALETTE(0x98, 0x78, 0xf8),

PALETTE(0xf8, 0x78, 0xf8),
PALETTE(0xf8, 0x58, 0x98),
PALETTE(0xf8, 0x78, 0x58),
PALETTE(0xfc, 0xa0, 0x44),

PALETTE(0xf8, 0xb8, 0x00),
PALETTE(0xb8, 0xf8, 0x18),
PALETTE(0x58, 0xd8, 0x54),
PALETTE(0x58, 0xf8, 0x98),

PALETTE(0x00, 0xe8, 0xd8),
PALETTE(0x78, 0x78, 0x78),
PALETTE(0x00, 0x00, 0x00),
PALETTE(0x00, 0x00, 0x00),

PALETTE(0xfc, 0xfc, 0xfc),
PALETTE(0xa4, 0xe4, 0xfc),
PALETTE(0xb8, 0xb8, 0xf8),
PALETTE(0xd8, 0xb8, 0xf8),

PALETTE(0xf8, 0xb8, 0xf8),
PALETTE(0xf8, 0xa4, 0xc0),
PALETTE(0xf0, 0xd0, 0xb0),
PALETTE(0xfc, 0xe0, 0xa8),

PALETTE(0xf8, 0xd8, 0x78),
PALETTE(0xd8, 0xf8, 0x78),
PALETTE(0xb8, 0xf8, 0xb8),
PALETTE(0xb8, 0xf8, 0xd8),

PALETTE(0x00, 0xfc, 0xfc),
PALETTE(0xf8, 0xd8, 0xf8),
PALETTE(0x00, 0x00, 0x00),
PALETTE(0x00, 0x00, 0x00),
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
static input_bits_t keyboard_bits = {false, false, false, false, false, false, false, false};
static input_bits_t gamepad1_bits = {false, false, false, false, false, false, false, false};
static input_bits_t gamepad2_bits = {false, false, false, false, false, false, false, false};

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

void
__not_in_flash_func(process_kbd_report)(hid_keyboard_report_t const *report, hid_keyboard_report_t const *prev_report) {
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

    switch (player_1_input) {
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

    switch (player_2_input) {
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
    static int16_t samples_out[2][buffermax * 2];
    static int i_active_buf = 0;
    static int inx = 0;

    static int max = 0;
    static int min = 30000;
    static uint32_t ii = 0;

    if (((ii++) & 0xff) == 0) printf("max=%d  min=%d\n", max, min);

    for (int i = 0; i < samples; i++) {
        int r, l;

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


        max = MAX(l, max);
        min = MIN(l, min);
//
        l -= 4000;
        r -= 4000;


        samples_out[i_active_buf][inx * 2] = l * snd_vol;
        samples_out[i_active_buf][inx * 2 + 1] = r * snd_vol;
        if (inx++ >= i2s_config.dma_trans_count) {
            inx = 0;
            i2s_dma_write(&i2s_config, reinterpret_cast<const int16_t *>(samples_out[i_active_buf]));
            i_active_buf ^= 1;

        }
    }

}


void __not_in_flash_func(InfoNES_PreDrawLine)
(
int line
){
InfoNES_SetLineBuffer(linebuffer,
NES_DISP_WIDTH);
}

#define X2(a) (a | (a << 8))
void __not_in_flash_func(InfoNES_PostDrawLine)
(
int line
){
for(
int x = 0;
x< NES_DISP_WIDTH; x++) SCREEN[line][x] = linebuffer[x];
}


#define CHECK_BIT(var, pos) (((var)>>(pos)) & 1)


/* Renderer loop on Pico's second core */
void __time_critical_func(render_core)() {
    initVGA();
    auto *buffer = reinterpret_cast<uint8_t *>(&SCREEN[0][0]);
    setVGAbuf(buffer, NES_DISP_WIDTH, NES_DISP_HEIGHT);
    uint8_t *text_buf = buffer + 1000;
    setVGA_text_buf(text_buf, &text_buf[80 * 30]);
    setVGA_bg_color(0);
    setVGAbuf_pos(32, 0);

    setVGA_color_flash_mode(flash_line, flash_frame);

    sem_acquire_blocking(&vga_start_semaphore);
}


void fileselector_load(char *pathname) {
    if (strcmp(rom_filename, pathname) == 0) {
        printf("Launching last rom");
        return;
    }

    FIL file;
    size_t bufsize = sizeof(SCREEN) & 0xfffff000;
    BYTE *buffer = (BYTE *) SCREEN;
    auto offset = FLASH_TARGET_OFFSET;
    UINT bytesRead;

    printf("Writing %s rom to flash %x\r\n", pathname, offset);
    FRESULT result = f_open(&file, pathname, FA_READ);


    if (result == FR_OK) {
        uint32_t interrupts = save_and_disable_interrupts();

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
    }
}

uint16_t fileselector_display_page(char filenames[60][256], uint16_t page_number) {
    clrScr(0);
    char footer[80];
    const int files_in_page = 29;
    sprintf(footer, "=================== PAGE #%i -> NEXT PAGE / <- PREV. PAGE ====================", page_number);
    draw_text(footer, 0, files_in_page, 1, 11);

    DIR directory;
    FILINFO file;
    FRESULT result = f_mount(&fs, "", 1);
    if (FR_OK != result) {
        printf("E f_mount error: %s (%d)\n", FRESULT_str(result), result);
        return 0;
    }

    /* clear the filenames array */
    for (uint8_t ifile = 0; ifile < 28; ifile++) {
        memset(&filenames[ifile][0], 0, 256);
    }

    uint16_t total_files = 0;
    result = f_findfirst(&directory, &file, "NES\\", "*.nes");

    /* skip the first N pages */
    if (page_number > 0) {
        while (total_files < page_number * files_in_page && result == FR_OK && file.fname[0]) {
            total_files++;
            result = f_findnext(&directory, &file);
        }
    }

    /* store the filenames of this page */
    total_files = 0;
    while (total_files < files_in_page && result == FR_OK && file.fname[0]) {
        strcpy(filenames[total_files], file.fname);
        total_files++;
        result = f_findnext(&directory, &file);
    }
    f_closedir(&directory);

    for (uint8_t ifile = 0; ifile < total_files; ifile++) {
        char pathname[255];
        uint8_t color = 0x0b;
        sprintf(pathname, "NES\\%s", filenames[ifile]);

        if (strcmp(pathname, rom_filename) != 0) {
            color = 0x0F;
        }
        draw_text(filenames[ifile], 0, ifile, color, 0);
    }
    return total_files;
}

void fileselector() {
    uint16_t page_number = 0;
    char filenames[60][256];

    printf("Selecting ROM\r\n");

    /* display the first page with up to 22 rom files */
    uint16_t total_files = fileselector_display_page(filenames, page_number);

    /* select the first rom */
    uint8_t current_file = 0;

    uint8_t color = 0x0b;
    draw_text(filenames[current_file], 0, current_file, color, 1);

    while (true) {
        char pathname[255];
        sprintf(pathname, "NES\\%s", filenames[current_file]);

        if (strcmp(pathname, rom_filename) != 0) {
            color = 0x0f;
        } else {
            color = 0x0b;
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
        if (keyboard_bits.select || gamepad1_bits.select) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            break;
        }

        if (keyboard_bits.start || gamepad1_bits.start || gamepad1_bits.a || gamepad1_bits.b) {
            /* copy the rom from the SD card to flash and start the game */
            fileselector_load(pathname);
            break;
        }
        if (keyboard_bits.down || gamepad1_bits.down) {
            /* select the next rom */
            draw_text(filenames[current_file], 0, current_file, color, 0x0);
            current_file++;
            if (current_file >= total_files)
                current_file = 0;
            draw_text(filenames[current_file], 0, current_file, color, 0x1);
            sleep_ms(150);
        }
        if (keyboard_bits.up || gamepad1_bits.up) {
            /* select the previous rom */
            draw_text(filenames[current_file], 0, current_file, color, 0x00);
            if (current_file == 0) {
                current_file = total_files - 1;
            } else {
                current_file--;
            }
            draw_text(filenames[current_file], 0, current_file, color, 0x1);
            sleep_ms(150);
        }
        if (keyboard_bits.right || gamepad1_bits.right) {
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
            draw_text(filenames[current_file], 0, current_file, color, 1);
            sleep_ms(150);
        }
        if ((keyboard_bits.left || gamepad1_bits.left) && page_number > 0) {
            /* select the previous page */
            page_number--;
            total_files = fileselector_display_page(filenames, page_number);
            /* select the first file */
            current_file = 0;
            draw_text(filenames[current_file], 0, current_file, color, 1);
            sleep_ms(150);
        }
        tight_loop_contents();
    }
}


int InfoNES_Menu() {
    setVGAmode(VGA640x480_text_80_30);
    fileselector();
    memset(SCREEN, 63, sizeof(SCREEN));
    setVGAmode(VGA640x480div2);

    if (!parseROM(reinterpret_cast<const uint8_t *>(rom))) {
        printf("NES file parse error.\n");
        return false;
    }
    if (InfoNES_Reset() < 0) {
        printf("NES reset error.\n");
        return false;
    }
    return 0;
}

enum menu_type_e {
    NONE,
    INT,
    TEXT,
    ARRAY,

    SAVE,
    LOAD,
    RESET,
    RETURN,
};

typedef struct __attribute__((__packed__)) {
    const char *text;
    menu_type_e type;
    const void *value;
    uint8_t max_value;
    char value_list[5][10];
} MenuItem;

#define MENU_ITEMS_NUMBER 13
const MenuItem menu_items[MENU_ITEMS_NUMBER] = {
        {"Player 1: %s",        ARRAY, &player_1_input, 2, {"Keyboard ", "Gamepad 1", "Gamepad 2"}},
        {"Player 2: %s",        ARRAY, &player_2_input, 2, {"Keyboard ", "Gamepad 1", "Gamepad 2"}},
        {""},
        {"Volume: %d",          INT,   &snd_vol,        8},
        {""},
        {"Flash line: %s",      ARRAY, &flash_line,     1, {"NO ",       "YES"}},
        {"Flash frame: %s",     ARRAY, &flash_frame,    1, {"NO ",       "YES"}},
        {""},
        {"Save state",          SAVE},
        {"Load state",          LOAD},
        {""},
        {"Reset to ROM select", RESET},
        {"Return to game",      RETURN}
};

void menu() {
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
                            save(rom_filename);
                            exit = true;
                        }
                        break;
                    case LOAD:
                        if (nespad_state & DPAD_START || keyboard_bits.start) {
                            load(rom_filename);
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
    setVGA_color_flash_mode(flash_line, flash_frame);

    setVGAmode(VGA640x480div2);
}

int InfoNES_LoadFrame() {
#if USE_PS2_KBD
    ps2kbd.tick();
#endif

#if USE_NESPAD
    nespad_tick();
#endif
    if ((keyboard_bits.start || gamepad1_bits.start) && (keyboard_bits.select || gamepad1_bits.select)) {
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

#if USE_PS2_KBD
    printf("PS2 KBD ");
    ps2kbd.init_gpio();
#endif

#if USE_NESPAD
    nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);
#endif

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_core);
    sem_release(&vga_start_semaphore);

    InfoNES_Main();
}
