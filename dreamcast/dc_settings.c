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
   settings->video_output  = DC_SETTINGS_VIDEO_DEFAULT;
   settings->audio_enabled = true;
   strncpy(settings->save_dir, "/sd/ngp", sizeof(settings->save_dir) - 1);
   strncpy(settings->system_dir, "/sd", sizeof(settings->system_dir) - 1);
   settings->save_dir[sizeof(settings->save_dir) - 1]   = '\0';
   settings->system_dir[sizeof(settings->system_dir) - 1] = '\0';
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
      else if (!strcmp(key, "audio"))
         settings->audio_enabled = (strtoul(value, NULL, 10) != 0);
      else if (!strcmp(key, "save_dir"))
         strncpy(settings->save_dir, value, sizeof(settings->save_dir) - 1);
      else if (!strcmp(key, "system_dir"))
         strncpy(settings->system_dir, value, sizeof(settings->system_dir) - 1);
   }

   fclose(file);

   settings->save_dir[sizeof(settings->save_dir) - 1]     = '\0';
   settings->system_dir[sizeof(settings->system_dir) - 1] = '\0';
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
   fprintf(file, "audio=%u\n", settings->audio_enabled ? 1 : 0);
   fprintf(file, "save_dir=%s\n", settings->save_dir);
   fprintf(file, "system_dir=%s\n", settings->system_dir);
   fclose(file);
}

dc_settings_t *dc_settings_get(void)
{
   return &active_settings;
}
