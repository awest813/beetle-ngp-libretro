/* KallistiOS libretro host for Beetle NeoGeo Pocket
 *
 * Build from the dreamcast/ directory after sourcing the KOS environ:
 *   make
 *
 * Usage:
 *   beetlengp.elf /sd/path/to/game.ngp
 */

#include "dc_audio.h"
#include "dc_settings.h"
#include "dc_video.h"
#include "menu.h"

#include <kos.h>
#include <dc/maple/controller.h>

#include <libretro.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FB_WIDTH  160
#define FB_HEIGHT 152

static dc_video_blitter_t *blitter;
static unsigned video_scale;

static maple_device_t *controller;
static dc_audio_stream_t *audio_stream;
static const char *loaded_rom_path;
static uint32_t previous_buttons;

typedef enum hotkey_action
{
   HOTKEY_NONE = 0,
   HOTKEY_QUIT,
   HOTKEY_SETTINGS
} hotkey_action_t;

static void ensure_save_dir(void)
{
   mkdir(dc_settings_get()->save_dir, 0755);
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

   dc_video_reinit(output);

   if (blitter)
      dc_video_blitter_set_scale(blitter, video_scale);
}

static void video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
   if (!data || !blitter)
      return;

   if (width != FB_WIDTH || height != FB_HEIGHT)
      return;

   dc_video_blitter_rgb555(blitter, data, width, height, pitch);
   dc_video_blitter_present(blitter, true);
}

static size_t audio_sample_batch(const int16_t *data, size_t frames)
{
   if (!audio_stream)
      return 0;

   return dc_audio_write(audio_stream, data, frames, false);
}

static void input_poll(void)
{
}

static int16_t input_state(unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   cont_state_t *state;

   (void)index;

   if (port != 0 || device != RETRO_DEVICE_JOYPAD)
      return 0;

   if (!controller)
      return 0;

   state = (cont_state_t *)maple_dev_status(controller);
   if (!state)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_JOYPAD_UP:
         return (state->buttons & CONT_DPAD_UP) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_DOWN:
         return (state->buttons & CONT_DPAD_DOWN) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_LEFT:
         return (state->buttons & CONT_DPAD_LEFT) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_RIGHT:
         return (state->buttons & CONT_DPAD_RIGHT) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_B:
         return (state->buttons & CONT_B) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_A:
         return (state->buttons & CONT_A) ? 1 : 0;
      case RETRO_DEVICE_ID_JOYPAD_START:
         return (state->buttons & CONT_START) ? 1 : 0;
      default:
         return 0;
   }
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

static void make_state_path(char *out, size_t out_len, const char *rom_path)
{
   const char *base;
   char name[128];
   char *dot;
   const dc_settings_t *cfg = dc_settings_get();

   base = strrchr(rom_path, '/');
   base = base ? base + 1 : rom_path;

   strncpy(name, base, sizeof(name) - 1);
   name[sizeof(name) - 1] = '\0';

   dot = strrchr(name, '.');
   if (dot)
      *dot = '\0';

   snprintf(out, out_len, "%s/%s.state", cfg->save_dir, name);
}

static bool save_state_file(const char *rom_path)
{
   char path[256];
   size_t size;
   void *buffer;
   FILE *file;

   size = retro_serialize_size();
   if (!size)
      return false;

   buffer = malloc(size);
   if (!buffer)
      return false;

   if (!retro_serialize(buffer, size))
   {
      free(buffer);
      return false;
   }

   make_state_path(path, sizeof(path), rom_path);
   file = fopen(path, "wb");
   if (!file)
   {
      free(buffer);
      return false;
   }

   fwrite(buffer, 1, size, file);
   fclose(file);
   free(buffer);

   printf("beetlengp: saved state to %s\n", path);
   return true;
}

static bool load_state_file(const char *rom_path)
{
   char path[256];
   size_t size;
   void *buffer;
   FILE *file;
   long file_size;

   make_state_path(path, sizeof(path), rom_path);
   file = fopen(path, "rb");
   if (!file)
      return false;

   size = retro_serialize_size();
   if (!size)
   {
      fclose(file);
      return false;
   }

   fseek(file, 0, SEEK_END);
   file_size = ftell(file);
   rewind(file);

   if ((size_t)file_size != size)
   {
      fclose(file);
      return false;
   }

   buffer = malloc(size);
   if (!buffer)
   {
      fclose(file);
      return false;
   }

   if (fread(buffer, 1, size, file) != size)
   {
      free(buffer);
      fclose(file);
      return false;
   }

   fclose(file);

   if (!retro_unserialize(buffer, size))
   {
      free(buffer);
      return false;
   }

   free(buffer);
   printf("beetlengp: loaded state from %s\n", path);
   return true;
}

static hotkey_action_t handle_hotkeys(cont_state_t *state)
{
   uint32_t pressed;

   if (!state)
      return HOTKEY_NONE;

   pressed = state->buttons & ~previous_buttons;
   previous_buttons = state->buttons;

   if ((state->buttons & CONT_START) && (pressed & CONT_Y))
      save_state_file(loaded_rom_path);

   if ((state->buttons & CONT_START) && (pressed & CONT_X))
      load_state_file(loaded_rom_path);

   if ((state->buttons & CONT_START) && (pressed & CONT_A))
      return HOTKEY_SETTINGS;

   if ((state->buttons & CONT_START) && (pressed & CONT_B))
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
   ensure_save_dir();

   blitter = dc_video_blitter_create(video_scale);
   if (!blitter)
   {
      printf("beetlengp: out of memory for framebuffer\n");
      return 1;
   }

   printf("beetlengp: video %ux%u cable=%s output=%s\n",
         dc_video_width(), dc_video_height(),
         dc_video_cable_name(dc_video_get_cable()),
         dc_video_output_name((dc_video_output_t)dc_settings_get()->video_output));

   controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

   retro_set_environment(environment);
   retro_set_video_refresh(video_refresh);
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
      dc_video_blitter_destroy(blitter);
      dc_video_shutdown();
      return 1;
   }

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
      dc_video_blitter_destroy(blitter);
      dc_video_shutdown();
      return 1;
   }

   retro_get_system_av_info(&av_info);
   printf("beetlengp: running %s at %.2f fps / %.0f Hz\n",
         rom_path,
         av_info.timing.fps,
         av_info.timing.sample_rate);

   audio_stream = dc_audio_create((unsigned)av_info.timing.sample_rate);
   if (audio_stream)
      dc_audio_start(audio_stream, (unsigned)av_info.timing.sample_rate);

   apply_audio_settings();
   previous_buttons = 0;

   for (;;)
   {
      cont_state_t *state = NULL;

      if (controller)
         state = (cont_state_t *)maple_dev_status(controller);

      action = handle_hotkeys(state);
      if (action == HOTKEY_QUIT)
         break;

      if (action == HOTKEY_SETTINGS)
      {
         menu_settings();
         apply_video_settings();
         apply_audio_settings();
         ensure_save_dir();
         previous_buttons = 0;
      }

      if (audio_stream)
         dc_audio_poll(audio_stream);

      retro_run();
   }

   if (audio_stream)
   {
      dc_audio_destroy(audio_stream);
      audio_stream = NULL;
   }

   retro_unload_game();
   retro_deinit();
   free(rom_data);
   free(menu_path);
   dc_video_blitter_destroy(blitter);
   dc_video_shutdown();

   return 0;
}
