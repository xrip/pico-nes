#ifndef PICO_USB_DRIVE
#define PICO_USB_DRIVE

void init_pico_usb_drive();
void pico_usb_drive_heartbeat();
void logMsg(char * msg); // from vga.h
void flash_range_erase2(uint32_t addr, size_t sz); // from main.cpp
void flash_range_program2(uint32_t addr, const unsigned char* buff, size_t sz);
size_t get_rom4prog_size();
uint32_t get_rom4prog();
char* get_shared_ram();
size_t get_shared_ram_size();
char* get_rom_filename();
_Bool tud_msc_test_ejected(); // msc_disk.c
enum {
  DISK_BLOCK_SIZE = 512
};
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size);

#endif
