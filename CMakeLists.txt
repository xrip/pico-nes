cmake_minimum_required(VERSION 3.13)

set(FAMILY rp2040)
set(BOARD pico_sdk)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
include(${PICO_SDK_PATH}/lib/tinyusb/hw/bsp/family_support.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

include(pico_sdk_import.cmake)

project(dendy C CXX ASM)

# Checks this example is valid for the family and initializes the project
family_initialize_project(dendy ${CMAKE_CURRENT_LIST_DIR})

#pico_sdk_init()
SET(GCC_RELEASENODEBUG_FLAGS "-O2 -DNDEBUG")
SET(CMAKE_ASM_FLAGS_RELEASENODEBUG
        "${GCC_RELEASENODEBUG_FLAGS}"
        CACHE STRING "Flags used by the ASM compiler during REL_O2 builds."
        FORCE)

SET(CMAKE_CXX_FLAGS_RELEASENODEBUG
        "${GCC_RELEASENODEBUG_FLAGS}"
        CACHE STRING "Flags used by the C++ compiler during REL_O2 builds."
        FORCE)
SET(CMAKE_C_FLAGS_RELEASENODEBUG
        "${GCC_RELEASENODEBUG_FLAGS}"
        CACHE STRING "Flags used by the C compiler during REL_O2 builds."
        FORCE)
SET(CMAKE_EXE_LINKER_FLAGS_RELEASENODEBUG
        ""
        CACHE STRING "Flags used for linking binaries during REL_O2 builds."
        FORCE)
SET(CMAKE_SHARED_LINKER_FLAGS_RELEASENODEBUG
        ""
        CACHE STRING "Flags used by the shared libraries linker during REL_O2 builds."
        FORCE)
MARK_AS_ADVANCED(
        CMAKE_CXX_FLAGS_RELEASENODEBUG
        CMAKE_ASM_FLAGS_RELEASENODEBUG
        CMAKE_C_FLAGS_RELEASENODEBUG
        CMAKE_EXE_LINKER_FLAGS_RELEASENODEBUG
        CMAKE_SHARED_LINKER_FLAGS_RELEASENODEBUG)

add_subdirectory(infones)
add_subdirectory(drivers/vga-nextgen)
add_subdirectory(drivers/audio)
add_subdirectory(drivers/ps2kbd)
add_subdirectory(drivers/nespad)
add_subdirectory(drivers/fatfs)
add_subdirectory(drivers/sdcard)

add_executable(dendy
        main.cpp
)

target_sources(dendy PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/usb.c
  ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/msc_disk.c
  ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb/usb_descriptors.c
)

#pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
#target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
#pico_set_boot_stage2(${PROJECT_NAME} slower_boot2)

pico_set_program_name(dendy "NES Emulator by xrip/DnCraptor")
pico_set_program_version(dendy "development version")


target_include_directories(dendy PRIVATE
 infones
 ${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb
)

family_configure_device_example(dendy noos)

target_link_libraries(dendy PRIVATE
        infones

        vga-nextgen
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
target_compile_options(dendy PUBLIC
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
        # usb
        PICO_DEFAULT_LED_PIN=25
)
pico_add_extra_outputs(dendy)
