/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

// This header may be included by other board headers as "boards/waveshare_rp2350_pizero.h"

#ifndef _BOARDS_WAVESHARE_RP2350_PIZERO
#define _BOARDS_WAVESHARE_RP2350_PIZERO

pico_board_cmake_set(PICO_PLATFORM, rp2350)

// For board detection
#define RASPBERRYPI_RP2350_PIZERO

// --- RP2350 VARIANT ---
#define PICO_RP2350A 0

// --- UART ---
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 0
#endif
#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 0
#endif
#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 1
#endif

// --- I2C ---
#ifndef PICO_DEFAULT_I2C
#define PICO_DEFAULT_I2C 0
#endif
#ifndef PICO_DEFAULT_I2C_SDA_PIN
#define PICO_DEFAULT_I2C_SDA_PIN 10
#endif
#ifndef PICO_DEFAULT_I2C_SCL_PIN
#define PICO_DEFAULT_I2C_SCL_PIN 11
#endif

// --- SPI ---
#ifndef PICO_DEFAULT_SPI
#define PICO_DEFAULT_SPI 1
#endif
#ifndef PICO_DEFAULT_SPI_SCK_PIN
#define PICO_DEFAULT_SPI_SCK_PIN 30
#endif
#ifndef PICO_DEFAULT_SPI_TX_PIN
#define PICO_DEFAULT_SPI_TX_PIN 31
#endif
#ifndef PICO_DEFAULT_SPI_RX_PIN
#define PICO_DEFAULT_SPI_RX_PIN 40
#endif
#ifndef PICO_DEFAULT_SPI_CSN_PIN
#define PICO_DEFAULT_SPI_CSN_PIN 43
#endif

// --- FLASH ---

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (4 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
#endif

pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, 1)
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

// Do not include boards/pico2.h here: it redefines PICO_RP2350A to 1, the SDK then
// limits GPIOs to 0..29 and drops the PIO GPIO base, and HDMI on GPIO32..39 never starts.

#define CPU_FREQ 252
#define ZERO2 1

// SDCARD
#define SDCARD_SPI_BUS spi1
#define SDCARD_PIN_SPI0_SCK 30
#define SDCARD_PIN_SPI0_MOSI 31
#define SDCARD_PIN_SPI0_MISO 40
#define SDCARD_PIN_SPI0_CS 43

// PS2KBD
#define PS2KBD_GPIO_FIRST 2 // CLK=GP2, DATA=GP3 - as in MOS2 and murm386 for z0p2

// NES Gamepad
// as in MOS2 for z0p2; joystick 2 data is DATA+1 = GP8 (GP10..12 are taken by the I2S DAC)
#define NES_GPIO_CLK 4
#define NES_GPIO_LAT 5
#define NES_GPIO_DATA 7
#define NES_GPIO_DATA2 8

// HDMI 8 pins starts from pin:
#define HDMI_BASE_PIN 32

#define SMS_SINGLE_FILE 1

// Sound
#if defined(AUDIO_PWM)
#define AUDIO_PWM_PIN 10
/// TODO: remove it
#define AUDIO_DATA_PIN 10
#define AUDIO_CLOCK_PIN 11
#define AUDIO_LCK_PIN 12
#else
// I2S Sound
#define AUDIO_DATA_PIN 10
#define AUDIO_CLOCK_PIN 11
#define AUDIO_LCK_PIN 12
#endif


#define PICO_DEFAULT_LED_PIN 25

#endif
