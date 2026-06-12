/* KallistiOS libretro host for Beetle NeoGeo Pocket
 *
 * Build from the dreamcast/ directory after sourcing the KOS environ:
 *   make
 *
 * Usage:
 *   beetlengp.elf /sd/path/to/game.ngp
 */

#include "dc_audio.h"
#include "dc_input.h"
#include "dc_notify.h"
#include "dc_saves.h"
#include "dc_vmu.h"
#include "dc_settings.h"
#include "dc_video.h"
#include "menu.h"

#include <kos.h>

#include <libretro.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FB_WIDTH  160
#define FB_HEIGHT 152

static dc_video_blitter_t *blitter;
static unsigned video_scale;
static bool vsync_enabled = true;

static dc_input_t player_input;
static dc_audio_stream_t *audio_stream;
static const char *loaded_rom_path;
static uint32_t previous_buttons;

typedef enum hotkey_action
{
   HOTKEY_NONE = 0,
   HOTKEY_QUIT,
   HOTKEY_SETTINGS
} hotkey_action_t;

static void retro_log_printf(enum retro_log_level level, const char *fmt, ...)
{
   const char *tag = "beetlengp";
   va_list args;

   (void)level;
   printf("%s: ", tag);
   va_start(args, fmt);
   vprintf(fmt, args);
   va_end(args);
}

static void apply_vmu_settings(void)
{
   const dc_settings_t *cfg = dc_settings_get();

   dc_vmu_set_enabled(cfg->vmu_lcd, cfg->vmu_save_sync);
}

static void apply_audio_settings(void)
{
   const dc_settings_t *cfg = dc_settings_get();

   if (!audio_stream)
      return;

   dc_audio_set_volume(audio_stream, cfg->volume);
   dc_audio_set_enabled(audio_stream, cfg->audio_enabled);
}

static void apply_video_settings(void)
{
   const dc_settings_t *cfg = dc_settings_get();
   dc_video_output_t output = (dc_video_output_t)cfg->video_output;

   video_scale = cfg->scale;
   if (video_scale < DC_SETTINGS_SCALE_MIN)
      video_scale = DC_SETTINGS_SCALE_MIN;
   if (video_scale > DC_SETTINGS_SCALE_MAX)
      video_scale = DC_SETTINGS_SCALE_MAX;

   vsync_enabled = cfg->vsync;
   dc_video_set_renderer((dc_video_renderer_t)cfg->video_renderer);
   dc_video_reinit_for_scale(output, video_scale);

   if (blitter)
      dc_video_blitter_sync(blitter, video_scale);
}

static void video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
   if (!blitter)
      return;

   if (width != FB_WIDTH || height != FB_HEIGHT)
      return;

   if (dc_video_get_renderer() == DC_VIDEO_RENDERER_PVR)
   {
      dc_video_present_rgb555(data, width, height, pitch, video_scale, vsync_enabled);
   }
   else
   {
      if (!data)
         return;

      dc_video_blitter_rgb555(blitter, data, width, height, pitch);
      dc_video_blitter_present(blitter, vsync_enabled);
      dc_notify_draw();
   }

   if (data)
      dc_vmu_feed_frame((const uint16_t *)data, width, height, pitch);
}

static void audio_sample(int16_t left, int16_t right)
{
   int16_t frame[2];

   if (!audio_stream)
      return;

   frame[0] = left;
   frame[1] = right;
   dc_audio_write(audio_stream, frame, 1, false);
}

static size_t audio_sample_batch(const int16_t *data, size_t frames)
{
   if (!audio_stream)
      return 0;

   return dc_audio_write(audio_stream, data, frames, false);
}

static void input_poll(void)
{
   dc_input_poll(&player_input);
}

static int16_t input_state(unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   (void)index;

   if (port != player_input.port)
      return 0;

   return dc_input_state(&player_input, device, id);
}

static bool environment(unsigned cmd, void *data)
{
   const dc_settings_t *cfg = dc_settings_get();

   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_CAN_DUPE:
         *(bool *)data = true;
         return true;
      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
         *(const char **)data = cfg->system_dir;
         return true;
      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         *(const char **)data = cfg->save_dir;
         return true;
      case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
         return true;
      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
      {
         struct retro_log_callback *log = (struct retro_log_callback *)data;

         log->log = retro_log_printf;
         return true;
      }
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
         if (*(const enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_0RGB1555)
            return true;
         return false;
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
         return false;
      default:
         return false;
   }
}

static uint8_t *load_rom_file(const char *path, size_t *size_out)
{
   FILE *file;
   long size;
   uint8_t *data;

   file = fopen(path, "rb");
   if (!file)
      return NULL;

   if (fseek(file, 0, SEEK_END) != 0)
   {
      fclose(file);
      return NULL;
   }

   size = ftell(file);
   if (size <= 0)
   {
      fclose(file);
      return NULL;
   }

   rewind(file);

   data = (uint8_t *)malloc((size_t)size);
   if (!data)
   {
      fclose(file);
      return NULL;
   }

   if (fread(data, 1, (size_t)size, file) != (size_t)size)
   {
      free(data);
      fclose(file);
      return NULL;
   }

   fclose(file);
   *size_out = (size_t)size;
   return data;
}

static const char *pick_rom_path(int argc, char **argv, char **menu_owned_out)
{
   menu_action_t action;
   char *picked = NULL;

   if (menu_owned_out)
      *menu_owned_out = NULL;

   if (argc > 1 && argv[1] && argv[1][0])
      return argv[1];

   action = menu_main(&picked);
   if (action == MENU_ACTION_LOAD && picked)
   {
      if (menu_owned_out)
         *menu_owned_out = picked;
      else
         free(picked);
      return picked;
   }

   if (picked)
      free(picked);

   return NULL;
}

static hotkey_action_t handle_hotkeys(uint32_t buttons)
{
   uint32_t pressed;

   pressed = buttons & ~previous_buttons;
   previous_buttons = buttons;

   if ((buttons & CONT_START) && (pressed & CONT_Y))
   {
      if (dc_saves_save_state(loaded_rom_path))
         dc_notify_show("State saved", 90);
      else
         dc_notify_show("Save failed", 90);
   }

   if ((buttons & CONT_START) && (pressed & CONT_X))
   {
      if (dc_saves_load_state(loaded_rom_path))
         dc_notify_show("State loaded", 90);
      else
         dc_notify_show("Load failed", 90);
   }

   if ((buttons & CONT_START) && (pressed & CONT_A))
      return HOTKEY_SETTINGS;

   if ((buttons & CONT_START) && (pressed & CONT_B))
      return HOTKEY_QUIT;

   return HOTKEY_NONE;
}

int main(int argc, char **argv)
{
   const char *rom_path;
   char *menu_path = NULL;
   struct retro_game_info game;
   struct retro_system_av_info av_info;
   struct retro_system_info sys_info;
   uint8_t *rom_data = NULL;
   size_t rom_size = 0;
   hotkey_action_t action;

   dc_settings_load(dc_settings_get());
   apply_video_settings();
   dc_saves_ensure_dir();
   dc_vmu_init();
   apply_vmu_settings();

   blitter = dc_video_blitter_create(video_scale);
   if (!blitter)
   {
      printf("beetlengp: out of memory for framebuffer\n");
      return 1;
   }

   player_input.port = 0;

   printf("beetlengp: video %ux%u cable=%s output=%s scale=%ux\n",
         dc_video_width(), dc_video_height(),
         dc_video_cable_name(dc_video_get_cable()),
         dc_video_output_name((dc_video_output_t)dc_settings_get()->video_output),
         video_scale);

   retro_set_environment(environment);
   retro_set_video_refresh(video_refresh);
   retro_set_audio_sample(audio_sample);
   retro_set_audio_sample_batch(audio_sample_batch);
   retro_set_input_poll(input_poll);
   retro_set_input_state(input_state);

   retro_init();

   retro_get_system_info(&sys_info);
   printf("beetlengp: loaded core %s %s\n",
         sys_info.library_name, sys_info.library_version);

   rom_path = pick_rom_path(argc, argv, &menu_path);
   if (!rom_path)
   {
      printf("beetlengp: no ROM selected\n");
      retro_deinit();
      dc_vmu_shutdown();
      dc_video_blitter_destroy(blitter);
      dc_video_shutdown();
      return 0;
   }

   loaded_rom_path = rom_path;

   rom_data = load_rom_file(rom_path, &rom_size);
   if (!rom_data)
   {
      printf("beetlengp: failed to read ROM '%s'\n", rom_path);
      free(menu_path);
      retro_deinit();
      dc_vmu_shutdown();
      dc_video_blitter_destroy(blitter);
      dc_video_shutdown();
      return 1;
   }

   dc_vmu_load_flash_from_vmu(rom_path);

   memset(&game, 0, sizeof(game));
   game.path = rom_path;
   game.data = rom_data;
   game.size = rom_size;

   if (!retro_load_game(&game))
   {
      printf("beetlengp: core rejected ROM '%s'\n", rom_path);
      free(rom_data);
      free(menu_path);
      retro_deinit();
      dc_vmu_shutdown();
      dc_video_blitter_destroy(blitter);
      dc_video_shutdown();
      return 1;
   }

   retro_get_system_av_info(&av_info);
   printf("beetlengp: running %s at %.2f fps / %.0f Hz\n",
         rom_path,
         av_info.timing.fps,
         av_info.timing.sample_rate);

   dc_vmu_set_game(rom_path, dc_saves_flash_exists(rom_path));
   if (dc_saves_flash_exists(rom_path))
      printf("beetlengp: found battery save\n");
   printf("beetlengp: VMU devices=%u\n", dc_vmu_device_count());

   audio_stream = dc_audio_create((unsigned)av_info.timing.sample_rate);
   if (audio_stream)
      dc_audio_start(audio_stream, (unsigned)av_info.timing.sample_rate);

   apply_audio_settings();

   if (dc_settings_get()->auto_load_state && dc_saves_state_exists(rom_path))
   {
      if (dc_saves_load_state(rom_path))
         dc_notify_show("Auto-loaded state", 120);
   }

   previous_buttons = 0;

   for (;;)
   {
      uint32_t buttons;

      dc_input_poll(&player_input);
      buttons = dc_input_maple_buttons(&player_input);

      action = handle_hotkeys(buttons);
      if (action == HOTKEY_QUIT)
         break;

      if (action == HOTKEY_SETTINGS)
      {
         dc_audio_pause(audio_stream);
         menu_settings();
         apply_video_settings();
         apply_audio_settings();
         apply_vmu_settings();
         dc_saves_ensure_dir();
         dc_audio_resume(audio_stream);
         previous_buttons = 0;
      }

      if (audio_stream)
         dc_audio_poll(audio_stream);

      retro_run();
      dc_notify_tick();
      dc_vmu_on_frame();
   }

   retro_unload_game();
   dc_vmu_sync_flash_to_vmu(loaded_rom_path);
   if (audio_stream)
   {
      dc_audio_destroy(audio_stream);
      audio_stream = NULL;
   }

   retro_deinit();
   free(rom_data);
   free(menu_path);
   dc_vmu_shutdown();
   dc_video_blitter_destroy(blitter);
   dc_video_shutdown();

   return 0;
}
