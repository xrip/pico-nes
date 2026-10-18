#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NES_PSRAM_BASE ((uintptr_t)0x11000000u)
#define NES_PSRAM_MAX_SIZE (16u * 1024u * 1024u)

#ifdef __cplusplus
extern "C" {
#endif

bool nes_psram_init(void);
bool nes_psram_available(void);
size_t nes_psram_size(void);
int nes_psram_cs1_gpio(void);
void nes_psram_prepare_reset(void);

/* Ctrl+Alt+Del and all other watchdog resets use the same safe path. */
void nes_safe_reset(void);

#ifdef __cplusplus
}
#endif
