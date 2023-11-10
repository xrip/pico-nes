#ifndef PICO_USB_DRIVE
#define PICO_USB_DRIVE

void init_pico_usb_drive();
void pico_usb_drive_heartbeat();
void logMsg(char * msg); // from vga.h
void flash_range_erase2(uint32_t addr, size_t sz); // from main.cpp
void flash_range_program2(uint32_t addr, const u_int8_t * buff, size_t sz);
size_t get_rom4prog_size();
uint32_t get_rom4prog();
char* get_shared_ram();
size_t get_shared_ram_size();
char* get_rom_filename();
bool tud_msc_test_ejected(); // msc_disk.c

#endif
