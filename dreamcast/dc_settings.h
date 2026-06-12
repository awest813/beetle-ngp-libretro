#ifndef DC_SETTINGS_H
#define DC_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#define DC_SETTINGS_PATH   "/sd/ngp/beetlengp.cfg"
#define DC_SETTINGS_VOLUME_DEFAULT 200
#define DC_SETTINGS_SCALE_DEFAULT  3
#define DC_SETTINGS_SCALE_MIN      2
#define DC_SETTINGS_SCALE_MAX      4
#define DC_SETTINGS_VIDEO_DEFAULT  0

typedef struct dc_settings
{
   uint8_t volume;
   uint8_t scale;
   uint8_t video_output;
   bool audio_enabled;
   bool vsync;
   bool auto_load_state;
   bool vmu_lcd;
   bool vmu_save_sync;
   char save_dir[64];
   char system_dir[64];
} dc_settings_t;

void dc_settings_load(dc_settings_t *settings);
void dc_settings_save(const dc_settings_t *settings);
void dc_settings_set_defaults(dc_settings_t *settings);

dc_settings_t *dc_settings_get(void);

#endif
