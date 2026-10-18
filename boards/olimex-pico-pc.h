#pragma once
#if PICO_RP2350
#include "boards/pico2.h"

#else
#include "boards/pico.h"
#endif

#define PICO_PC 1
#if PICO_RP2350
#define PSRAM_CS1_GPIO_RP2350A 8
#define PSRAM_CS1_GPIO_RP2350B 47
#endif

#define CPU_FREQ 378

// SDCARD
#define SDCARD_PIN_SPI0_SCK 6
#define SDCARD_PIN_SPI0_MOSI 7
#define SDCARD_PIN_SPI0_MISO 4
#define SDCARD_PIN_SPI0_CS 22

// PS2KBD
#define PS2KBD_GPIO_FIRST 0

// NES Gamepad
#define NES_GPIO_CLK 8
#define NES_GPIO_LAT 9
#define NES_GPIO_DATA 20
#define NES_GPIO_DATA2 21

// VGA 8 pins starts from pin:
#define VGA_BASE_PIN 12

// HDMI 8 pins starts from pin:
#define HDMI_BASE_PIN 12

// TFT
#define TFT_CS_PIN 12
#define TFT_RST_PIN 14
#define TFT_LED_PIN 15
#define TFT_DC_PIN 16
#define TFT_DATA_PIN 18
#define TFT_CLK_PIN 19

#define SMS_SINGLE_FILE 1

// Sound
#if defined(AUDIO_PWM)
#define AUDIO_PWM_PIN 26
/// TODO: remove it
#define AUDIO_DATA_PIN 26
#define AUDIO_CLOCK_PIN 27
#define AUDIO_LCK_PIN 28
#else
// I2S Sound
#define AUDIO_DATA_PIN 26
#define AUDIO_CLOCK_PIN 27
#define AUDIO_LCK_PIN 28
#endif
