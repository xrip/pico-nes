set(FAMILY rp2040)
set(BOARD pico_sdk)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/hw/bsp/family_support.cmake)

# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name2(INTERFACE ${CMAKE_CURRENT_LIST_DIR})

add_library(usb INTERFACE)

target_sources(usb INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/usb.c
  ${CMAKE_CURRENT_SOURCE_DIR}/msc_disk.c
  ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.c
  ${CMAKE_CURRENT_SOURCE_DIR}/hw/bsp/rp2040/family.c
)

target_include_directories(usb INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(usb INTERFACE pico_stdio_uart)


target_include_directories(usb INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/hw
  ${CMAKE_CURRENT_LIST_DIR}
)

#target_compile_definitions(usb INTERFACE LIB_PICO_STDIO_UART=1)
