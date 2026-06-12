#ifndef DC_SAVES_H
#define DC_SAVES_H

#include <stdbool.h>
#include <stddef.h>

bool dc_saves_ensure_dir(void);

void dc_saves_basename(const char *rom_path, char *out, size_t out_len);
void dc_saves_state_path(const char *rom_path, char *out, size_t out_len);
void dc_saves_flash_path(const char *rom_path, char *out, size_t out_len);

bool dc_saves_state_exists(const char *rom_path);
bool dc_saves_flash_exists(const char *rom_path);

bool dc_saves_save_state(const char *rom_path);
bool dc_saves_load_state(const char *rom_path);

/* Flush in-game battery data to <save_dir>/<rom>.flash (core API). */
bool dc_saves_sync_battery(void);
/* Reload battery data from <save_dir>/<rom>.flash into the core. */
bool dc_saves_reload_battery(void);

#endif
