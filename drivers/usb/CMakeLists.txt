set(FAMILY rp2040)
set(BOARD pico_sdk)
set(TINYUSB_FAMILY_PROJECT_NAME_PREFIX "tinyusb_dev_")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_library(usb INTERFACE)

target_sources(usb INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/usb.c
  ${CMAKE_CURRENT_SOURCE_DIR}/msc_disk.c
  ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.c
)

target_include_directories(usb INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(usb INTERFACE pico_stdlib)

target_include_directories(usb INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)
