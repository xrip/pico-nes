add_library(usb INTERFACE)

target_sources(usb INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/usb.c
  ${CMAKE_CURRENT_LIST_DIR}/usb.h
)

target_link_libraries(usb INTERFACE usb_device)

target_include_directories(usb INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)
