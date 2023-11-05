set(FAMILY rp2040)
set(BOARD pico_sdk)
set(TINYUSB_FAMILY_PROJECT_NAME_PREFIX "tinyusb_dev_")

add_library(usb INTERFACE)

target_sources(usb INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/usb.c
  ${CMAKE_CURRENT_SOURCE_DIR}/msc_disk.c
)

target_include_directories(usb INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(usb INTERFACE tinyusb_board)

target_include_directories(usb INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)
