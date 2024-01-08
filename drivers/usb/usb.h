#ifndef PICO_USB_DRIVE
#define PICO_USB_DRIVE

#include "ff.h"
#include "diskio.h"

void in_flash_drive();
// from vga.h
// from main.cpp
void flash_range_erase2(uint32_t addr, size_t sz);
void flash_range_program2(uint32_t addr, const unsigned char* buff, size_t sz);
size_t get_rom4prog_size();
uint32_t get_rom4prog();
char* get_shared_ram();
size_t get_shared_ram_size();
char* get_rom_filename();
FATFS* getFlashInDriveFATFSptr();
FATFS* getSDCardFATFSptr();
// msc_disk.c
_Bool tud_msc_test_ejected();
enum {
  DISK_BLOCK_SIZE = 512,
  FAT_OFFSET = 0x1000
};
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size);
void restore_clean_fat();

#endif
