cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(pico_sdk_import.cmake)

project(dendy C CXX ASM)
pico_sdk_init()

add_subdirectory(infones)
add_subdirectory(drivers/vga)
add_subdirectory(drivers/audio)
add_subdirectory(drivers/ps2kbd)
add_subdirectory(drivers/nespad)
add_subdirectory(drivers/fatfs)
add_subdirectory(drivers/sdcard)


add_executable(dendy
        main.cpp
)

pico_set_program_name(dendy "Dendy Emulator for MURMULATOR devboard")
pico_set_program_version(dendy "v0.1")


target_include_directories(dendy PRIVATE infones)

pico_enable_stdio_uart(dendy 0)
pico_enable_stdio_usb(dendy 1)

target_link_libraries(dendy PRIVATE
        infones

        vga
        audio
        ps2kbd
        nespad
        sdcard
        fatfs

        pico_stdlib
        pico_multicore
        hardware_dma
        hardware_pio
        hardware_i2c
        hardware_interp
        hardware_timer
        hardware_clocks
        hardware_pwm
        hardware_flash
)

target_compile_definitions(dendy PRIVATE
        SPEAKER_ENABLED
        # VGA
        VGA_GPIO_FIRST=6
        VGA_GPIO_OUTNUM=6
        VGA_GPIO_SYNC=12
        VGA_VSYNC=13

        # SDCARD
        SDCARD_PIN_SPI0_CS=5
        SDCARD_PIN_SPI0_SCK=2
        SDCARD_PIN_SPI0_MOSI=3
        SDCARD_PIN_SPI0_MISO=4

        # PS2KBD
        USE_PS2_KBD
        PS2KBD_GPIO_FIRST=0

        # NES Gamepad
        USE_NESPAD
        NES_GPIO_CLK=14
        NES_GPIO_DATA=16
        NES_GPIO_LAT=15

        #AUDIO
        AUDIO_PWM_PIN=26

        # Other
        FF_USE_FIND
)
pico_add_extra_outputs(dendy)



