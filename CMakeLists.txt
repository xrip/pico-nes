cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(dendy C CXX ASM)
SET(BUILD_NAME "${PROJECT_NAME}")

option(I2S "Enable I2S sound" OFF)
option(I2S_CS4334 "Enable I2S CS4334 sound" OFF)
option(VGA "Enable VGA" OFF)
option(TFT "Enable TFT display" OFF)
option(ILI9341 "Enable TFT ILI9341 display" OFF)
option(HDMI "Enable HDMI display" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")


#set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(FAMILY rp2040)
set(BOARD pico_sdk)
#[[include(${PICO_SDK_PATH}/lib/tinyusb/hw/bsp/family_support.cmake)

# Checks this example is valid for the family and initializes the project
family_initialize_project(dendy ${CMAKE_CURRENT_LIST_DIR})]]

add_subdirectory(infones)
add_subdirectory(drivers/vga-nextgen)
add_subdirectory(drivers/audio)
add_subdirectory(drivers/ps2kbd)
add_subdirectory(drivers/nespad)
add_subdirectory(drivers/fatfs)
add_subdirectory(drivers/sdcard)
add_subdirectory(drivers/st7789)
add_subdirectory(drivers/hdmi)
add_subdirectory(drivers/psram)

add_executable(${PROJECT_NAME} main.cpp )

pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
pico_set_boot_stage2(${PROJECT_NAME} slower_boot2)

set(FAMILY rp2040)
set(BOARD pico_sdk)
family_configure_device_example(${PROJECT_NAME} noos)

target_sources(dendy PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/usb.c
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/msc_disk.c
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/usb_descriptors.c
)


pico_set_program_name(dendy "NES Emulator by xrip & DnCraptor")
pico_set_program_version(dendy "test")


target_include_directories(${PROJECT_NAME} PRIVATE
        infones
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb
)

IF(TFT)
    target_link_libraries(${PROJECT_NAME} PRIVATE st7789)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TFT)
    SET(BUILD_NAME "${BUILD_NAME}-TFT")
    IF(ILI9341)
        SET(BUILD_NAME "${BUILD_NAME}-ILI9341")
        target_compile_definitions(${PROJECT_NAME} PRIVATE ILI9341)
    ELSE()
        SET(BUILD_NAME "${BUILD_NAME}-ST7789")
    ENDIF()
ELSEIF(HDMI)
    target_link_libraries(${PROJECT_NAME} PRIVATE hdmi)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HDMI)
    SET(BUILD_NAME "${BUILD_NAME}-HDMI")
ELSE()
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGA)
    target_link_libraries(${PROJECT_NAME} PRIVATE vga-nextgen)
ENDIF()

IF(NOT I2S)
    target_compile_definitions(${PROJECT_NAME} PRIVATE AUDIO_PWM_PIN=26)
ELSEIF(I2S_CS4334)
    target_compile_definitions(${PROJECT_NAME} PRIVATE I2S_CS4334)
    SET(BUILD_NAME "${BUILD_NAME}-I2S-CS4334")
ELSE()
    SET(BUILD_NAME "${BUILD_NAME}-I2S-PCM5102")
ENDIF()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")

# Enable compiler and linker garbage collect unused code/data sections
#add_link_options(-Wl,--gc-sections)

target_link_libraries(${PROJECT_NAME} PRIVATE
        infones

        psram
        audio
        ps2kbd
        nespad
        sdcard
        fatfs

        pico_runtime
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



if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PUBLIC
            -mcpu=cortex-m0plus
            -Wno-error=suggest-attribute=format
            -Wno-error=cast-qual
            -Wno-error=unused-parameter
            -Wno-error=conversion
            -Wno-error=format=
            -Wno-error=sign-compare
            -Wno-error=missing-field-initializers
            -Wno-error=switch
            -Wno-error=implicit-fallthrough=
            -Wno-error=stringop-truncation
            -Wno-error=restrict
            -w
    )
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE
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
        #AUDIO_PWM_PIN=26
        #I2S_CS4334

        # Other
        FF_USE_FIND
        # usb
        PICO_DEFAULT_LED_PIN=25
        # internal games
        #BUILD_IN_GAMES

        #TFT
        TFT_CS_PIN=6
        TFT_RST_PIN=8
        TFT_DC_PIN=10
        TFT_DATA_PIN=12
        TFT_CLK_PIN=13
        TFT_LED_PIN=9


        PSRAM
        # PSRAM_MUTEX=1
        PSRAM_SPINLOCK=1
        PSRAM_ASYNC=1

        PSRAM_PIN_CS=18
        PSRAM_PIN_SCK=19
        PSRAM_PIN_MOSI=20
        PSRAM_PIN_MISO=21
)
target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage)
pico_add_extra_outputs(${PROJECT_NAME})
