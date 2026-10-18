enum PALETTES {
    RGB333,
    RBG222,
};

enum INPUT {
    KEYBOARD,
    GAMEPAD1,
    GAMEPAD2,
    COMBINED,
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
    bool swap_ab;
    bool inversion;
    uint8_t tv_system; // TV-SOFT: 0 - PAL, 1 - NTSC
    bool tv_color;     // TV-SOFT: colour on/off
    uint8_t nes_timing; // 0 - auto (ROM header), 1 - NTSC, 2 - PAL, 3 - Dendy
} SETTINGS;
