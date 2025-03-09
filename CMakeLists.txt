# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards )
#set(PICO_PLATFORM "rp2350" CACHE STRING "Chip type")
set(PICO_PLATFORM "rp2040" CACHE STRING "Chip type")
set(PICO_BOARD "murmulator" CACHE STRING "Board type")
#set(PICO_BOARD "murmulator2" CACHE STRING "Board type")
include(pico_sdk_import.cmake)

project(nes C CXX ASM)

option(I2S "Enable I2S sound" OFF)
option(I2S_CS4334 "Enable I2S CS4334 sound" OFF)
option(VGA "Enable VGA" OFF)
option(TFT "Enable TFT display" OFF)
option(ILI9341 "Enable TFT ILI9341 display" OFF)
option(HDMI "Enable HDMI display" OFF)
option(TV "Enable TV composite output" OFF)
option(SOFTTV "Enable TV soft composite output" OFF)
if( ${PICO_PLATFORM} MATCHES "rp2350" )
option(m1p2launcher "Enable m1p2-launcher support" OFF)
endif()

#set(m1p2launcher ON)
#set(I2S ON)
#set(TFT ON)
#set(ILI9341 ON)
#set(HDMI ON)
#set(TV ON)
#set(SOFTTV ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
#add_compile_options(-flto)
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -funroll-loops -ffast-math -feliminate-unused-debug-types -ffunction-sections -fdata-sections -Ofast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops -fno-rtti -ffast-math -feliminate-unused-debug-types -ffunction-sections -fdata-sections -Ofast")

#set(FLASH_SIZE 4096)
#SET(BUILD_NAME "${PROJECT_NAME}-${FLASH_SIZE}K")

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
pico_sdk_init()

add_subdirectory(infones)
add_subdirectory(drivers/audio)
add_subdirectory(drivers/ps2kbd)
add_subdirectory(drivers/nespad)
add_subdirectory(drivers/fatfs)
add_subdirectory(drivers/sdcard)
add_subdirectory(drivers/vga-nextgen)
add_subdirectory(drivers/st7789)
add_subdirectory(drivers/hdmi)
add_subdirectory(drivers/tv)
add_subdirectory(drivers/tv-software)
add_subdirectory(drivers/graphics)

# INCLUDE FILES THAT SHOULD BE COMPILED:
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c"
#        "drivers/usb/*.c"
)

message(STATUS "Add source files:")
foreach (SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach ()
message(STATUS "")

add_executable(${PROJECT_NAME} ${SRC})


if( ${PICO_PLATFORM} MATCHES "rp2040" )
	pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
	target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
	pico_set_boot_stage2(${PROJECT_NAME} slower_boot2)
	if ( ${PICO_BOARD} MATCHES "murmulator2")
		SET(BUILD_NAME "m2p1-${PROJECT_NAME}")
	else()
		SET(BUILD_NAME "m1p1-${PROJECT_NAME}")
	endif()
else()
    if (m1p2launcher)
		pico_set_linker_script(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/memmap.ld")
	endif()
	if ( ${PICO_BOARD} MATCHES "murmulator2")
		SET(BUILD_NAME "m2p2-${PROJECT_NAME}")
	else()
		SET(BUILD_NAME "m1p2-${PROJECT_NAME}")
	endif()
endif()

if ( ${PICO_BOARD} MATCHES "murmulator2")
	pico_set_program_name(nes "Dendy/NES emulator by xrip for MURMULATOR 2.0 board")
else()
	pico_set_program_name(nes "Dendy/NES emulator by xrip for MURMULATOR 1.x board")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    infones
    src
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
ELSEIF(TV)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TV)
    target_link_libraries(${PROJECT_NAME} PRIVATE tv)
    SET(BUILD_NAME "${BUILD_NAME}-TV")
ELSEIF(SOFTTV)
	target_compile_definitions(${PROJECT_NAME} PRIVATE SOFTTV)
	target_link_libraries(${PROJECT_NAME} PRIVATE tv-software)
	SET(BUILD_NAME "${BUILD_NAME}-TV-SOFT")
ELSE()
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGA)
    target_link_libraries(${PROJECT_NAME} PRIVATE vga-nextgen)
    SET(BUILD_NAME "${BUILD_NAME}-VGA")
ENDIF()

IF(NOT I2S)
    target_compile_definitions(${PROJECT_NAME} PRIVATE AUDIO_PWM)
    SET(BUILD_NAME "${BUILD_NAME}-PWM")
ELSEIF(I2S_CS4334)
    target_compile_definitions(${PROJECT_NAME} PRIVATE I2S_CS4334)
    SET(BUILD_NAME "${BUILD_NAME}-I2S-CS4334")
ELSE()
    SET(BUILD_NAME "${BUILD_NAME}-I2S-TDA1387")
ENDIF()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")

# pull in common dependencies
target_link_libraries(nes PRIVATE
    infones
    audio
    graphics
    
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
    stdc++
    
    tinyusb_host tinyusb_board
)

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(nes pico_cyw43_arch_none)
endif()

pico_enable_stdio_uart(${PROJECT_NAME} 0)
##pico_enable_stdio_usb(${PROJECT_NAME} 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})

# add url via pico_set_program_url
target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage --data-sections --function-sections)
target_compile_definitions(${PROJECT_NAME} PRIVATE FLASH_SIZE=${FLASH_SIZE})

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
target_compile_options(${PROJECT_NAME} PUBLIC
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
        PICO_PROGRAM_NAME="nes"
        PICO_PROGRAM_VERSION_STRING="304"
        USE_PS2_KBD
        USE_NESPAD
)

