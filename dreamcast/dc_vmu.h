#ifndef DC_VMU_H
#define DC_VMU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool dc_vmu_init(void);
void dc_vmu_shutdown(void);

void dc_vmu_set_enabled(bool lcd, bool save_sync);
void dc_vmu_set_game(const char *rom_path, bool battery_save);

void dc_vmu_present_idle(const char *message);
void dc_vmu_feed_frame(const uint16_t *frame, unsigned width, unsigned height,
      size_t pitch);
void dc_vmu_on_frame(void);

bool dc_vmu_sync_flash_to_vmu(const char *rom_path);
bool dc_vmu_load_flash_from_vmu(const char *rom_path);

unsigned dc_vmu_device_count(void);

#endif
