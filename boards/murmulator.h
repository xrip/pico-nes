#pragma once
#if PICO_RP2350
#include "boards/pico2.h"

#else
#include "boards/pico.h"
#endif

// SDCARD
#define SDCARD_PIN_SPI0_CS 5
#define SDCARD_PIN_SPI0_SCK 2
#define SDCARD_PIN_SPI0_MOSI 3
#define SDCARD_PIN_SPI0_MISO 4

// PS2KBD
#define PS2KBD_GPIO_FIRST 0

// NES Gamepad
#define NES_GPIO_CLK 14
#define NES_GPIO_DATA 16
#define NES_GPIO_LAT 15

// VGA 8 pins starts from pin:
#define VGA_BASE_PIN 6

// HDMI 8 pins starts from pin:
#define HDMI_BASE_PIN 6

// TFT
#define TFT_CS_PIN 6
#define TFT_RST_PIN 8
#define TFT_LED_PIN 9
#define TFT_DC_PIN 10
#define TFT_DATA_PIN 12
#define TFT_CLK_PIN 13

#define SMS_SINGLE_FILE 1

// Sound
#if defined(AUDIO_PWM)
#define AUDIO_PWM_PIN 26
/// TODO: remove it
#define AUDIO_DATA_PIN 26
#define AUDIO_CLOCK_PIN 27
#else
// I2S Sound
#define AUDIO_DATA_PIN 26
#define AUDIO_CLOCK_PIN 27
#endif