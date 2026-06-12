#include "dc_settings.h"
#include "dc_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static dc_settings_t active_settings;

static void ensure_config_dir(void)
{
   mkdir("/sd/ngp", 0755);
}

void dc_settings_set_defaults(dc_settings_t *settings)
{
   if (!settings)
      return;

   settings->volume        = DC_SETTINGS_VOLUME_DEFAULT;
   settings->scale         = DC_SETTINGS_SCALE_DEFAULT;
   settings->video_output    = DC_SETTINGS_VIDEO_DEFAULT;
   settings->video_renderer  = DC_SETTINGS_RENDERER_DEFAULT;
   settings->audio_enabled   = true;
   settings->vsync           = true;
   settings->auto_load_state = false;
   settings->vmu_lcd         = true;
   settings->vmu_save_sync   = false;
   strncpy(settings->save_dir, "/sd/ngp", sizeof(settings->save_dir) - 1);
   strncpy(settings->system_dir, "/sd", sizeof(settings->system_dir) - 1);
   settings->save_dir[sizeof(settings->save_dir) - 1]   = '\0';
   settings->system_dir[sizeof(settings->system_dir) - 1] = '\0';
   settings->last_rom[0] = '\0';
}

void dc_settings_load(dc_settings_t *settings)
{
   FILE *file;
   char line[128];
   char key[32];
   char value[96];

   dc_settings_set_defaults(settings);
   ensure_config_dir();

   file = fopen(DC_SETTINGS_PATH, "r");
   if (!file)
      return;

   while (fgets(line, sizeof(line), file))
   {
      if (sscanf(line, " %31[^=]=%95s", key, value) < 2)
         continue;

      if (!strcmp(key, "volume"))
         settings->volume = (uint8_t)strtoul(value, NULL, 10);
      else if (!strcmp(key, "scale"))
      {
         unsigned scale = (unsigned)strtoul(value, NULL, 10);

         if (scale >= DC_SETTINGS_SCALE_MIN && scale <= DC_SETTINGS_SCALE_MAX)
            settings->scale = (uint8_t)scale;
      }
      else if (!strcmp(key, "video"))
      {
         unsigned video = (unsigned)strtoul(value, NULL, 10);

         if (video < DC_VIDEO_OUTPUT_COUNT)
            settings->video_output = (uint8_t)video;
      }
      else if (!strcmp(key, "renderer"))
      {
         unsigned renderer = (unsigned)strtoul(value, NULL, 10);

         if (renderer < DC_VIDEO_RENDERER_COUNT)
            settings->video_renderer = (uint8_t)renderer;
      }
      else if (!strcmp(key, "audio"))
         settings->audio_enabled = (strtoul(value, NULL, 10) != 0);
      else if (!strcmp(key, "vsync"))
         settings->vsync = (strtoul(value, NULL, 10) != 0);
      else if (!strcmp(key, "auto_load_state"))
         settings->auto_load_state = (strtoul(value, NULL, 10) != 0);
      else if (!strcmp(key, "vmu_lcd"))
         settings->vmu_lcd = (strtoul(value, NULL, 10) != 0);
      else if (!strcmp(key, "vmu_save"))
         settings->vmu_save_sync = (strtoul(value, NULL, 10) != 0);
      else if (!strcmp(key, "save_dir"))
         strncpy(settings->save_dir, value, sizeof(settings->save_dir) - 1);
      else if (!strcmp(key, "system_dir"))
         strncpy(settings->system_dir, value, sizeof(settings->system_dir) - 1);
      else if (!strcmp(key, "last_rom"))
         strncpy(settings->last_rom, value, sizeof(settings->last_rom) - 1);
   }

   fclose(file);

   settings->save_dir[sizeof(settings->save_dir) - 1]     = '\0';
   settings->system_dir[sizeof(settings->system_dir) - 1] = '\0';
   settings->last_rom[sizeof(settings->last_rom) - 1]     = '\0';
}

void dc_settings_save(const dc_settings_t *settings)
{
   FILE *file;

   if (!settings)
      return;

   ensure_config_dir();

   file = fopen(DC_SETTINGS_PATH, "w");
   if (!file)
      return;

   fprintf(file, "volume=%u\n", settings->volume);
   fprintf(file, "scale=%u\n", settings->scale);
   fprintf(file, "video=%u\n", settings->video_output);
   fprintf(file, "renderer=%u\n", settings->video_renderer);
   fprintf(file, "audio=%u\n", settings->audio_enabled ? 1 : 0);
   fprintf(file, "vsync=%u\n", settings->vsync ? 1 : 0);
   fprintf(file, "auto_load_state=%u\n", settings->auto_load_state ? 1 : 0);
   fprintf(file, "vmu_lcd=%u\n", settings->vmu_lcd ? 1 : 0);
   fprintf(file, "vmu_save=%u\n", settings->vmu_save_sync ? 1 : 0);
   fprintf(file, "save_dir=%s\n", settings->save_dir);
   fprintf(file, "system_dir=%s\n", settings->system_dir);
   fprintf(file, "last_rom=%s\n", settings->last_rom);
   fclose(file);
}

dc_settings_t *dc_settings_get(void)
{
   return &active_settings;
}

bool dc_settings_last_rom_valid(const dc_settings_t *settings)
{
   FILE *file;

   if (!settings || !settings->last_rom[0])
      return false;

   file = fopen(settings->last_rom, "rb");
   if (!file)
      return false;

   fclose(file);
   return true;
}

void dc_settings_set_last_rom(dc_settings_t *settings, const char *path)
{
   if (!settings || !path || !path[0])
      return;

   strncpy(settings->last_rom, path, sizeof(settings->last_rom) - 1);
   settings->last_rom[sizeof(settings->last_rom) - 1] = '\0';
   dc_settings_save(settings);
}
