#include "menu.h"

#include "dc_saves.h"
#include "dc_settings.h"
#include "dc_ui.h"
#include "dc_video.h"
#include "dc_vmu.h"

#include <kos.h>
#include <dc/maple/controller.h>

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAX_ROM_ENTRIES 64
#define ROM_NAME_LEN    48
typedef struct
{
   char path[256];
   char name[ROM_NAME_LEN];
} rom_entry_t;

static const char *scan_dirs[] = {
   "/sd/ngp",
   "/sd/roms/ngp",
   "/sd",
   "/ide/ngp",
   "/ide/roms/ngp",
   "/ide",
   "/pc",
};

static rom_entry_t roms[MAX_ROM_ENTRIES];
static int rom_count;

static cont_state_t *poll_controller(void)
{
   maple_device_t *device = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

   if (!device)
      return NULL;

   return (cont_state_t *)maple_dev_status(device);
}

static void clear_screen(void)
{
   dc_ui_begin_frame();
}

static void draw_text(int x, int y, int color, const char *text)
{
   dc_ui_draw_text(x, y, color, text);
}

static bool has_rom_extension(const char *name)
{
   const char *dot = strrchr(name, '.');

   if (!dot)
      return false;

   return !strcasecmp(dot, ".ngp")
       || !strcasecmp(dot, ".ngc")
       || !strcasecmp(dot, ".ngpc")
       || !strcasecmp(dot, ".npc");
}

static void add_rom(const char *dir, const char *name)
{
   rom_entry_t *entry;

   if (rom_count >= MAX_ROM_ENTRIES)
      return;

   entry = &roms[rom_count++];
   snprintf(entry->path, sizeof(entry->path), "%s/%s", dir, name);
   strncpy(entry->name, name, sizeof(entry->name) - 1);
   entry->name[sizeof(entry->name) - 1] = '\0';
}

static void scan_directory(const char *dir)
{
   DIR *handle;
   struct dirent *item;

   handle = opendir(dir);
   if (!handle)
      return;

   while ((item = readdir(handle)) != NULL)
   {
      if (item->d_name[0] == '.')
         continue;

      if (!has_rom_extension(item->d_name))
         continue;

      add_rom(dir, item->d_name);
   }

   closedir(handle);
}

static void collect_roms(void)
{
   size_t i;

   rom_count = 0;

   for (i = 0; i < sizeof(scan_dirs) / sizeof(scan_dirs[0]); i++)
      scan_directory(scan_dirs[i]);
}

static void draw_rom_menu(int selected)
{
   int y = 24;
   int i;

   clear_screen();
   draw_text(20, y, 1, "Beetle NGP - Select ROM");
   y += 24;
   draw_text(20, y, 1, "D-Pad: move   A: load   B: back");
   y += 32;

   if (rom_count == 0)
   {
      draw_text(20, y, 1, "No ROMs found on /sd or /ide");
      return;
   }

   for (i = 0; i < rom_count; i++)
   {
      char line[ROM_NAME_LEN + 4];

      if (i < selected - 8)
         continue;
      if (i > selected + 12)
         break;

      snprintf(line, sizeof(line), "%s%s",
            (i == selected) ? "> " : "  ", roms[i].name);
      draw_text(20, y, (i == selected) ? 1 : 0, line);
      y += 20;
   }
}

char *menu_pick_rom(void)
{
   cont_state_t *state;
   uint32_t previous = 0;
   int selected = 0;
   char *result = NULL;

   collect_roms();
   if (rom_count == 0)
      return NULL;

   for (;;)
   {
      uint32_t pressed;

      draw_rom_menu(selected);
      dc_ui_present(true);

      state = poll_controller();
      if (!state)
      {
         thd_sleep(16);
         continue;
      }

      pressed = state->buttons & ~previous;
      previous = state->buttons;

      if (pressed & CONT_DPAD_UP)
      {
         if (selected > 0)
            selected--;
      }
      else if (pressed & CONT_DPAD_DOWN)
      {
         if (selected < rom_count - 1)
            selected++;
      }
      else if (pressed & CONT_A)
      {
         result = strdup(roms[selected].path);
         break;
      }
      else if (pressed & CONT_B || pressed & CONT_START)
         break;

      thd_sleep(16);
   }

   return result;
}

static void draw_main_menu(int selected)
{
   static const char *items[] = {
      "Load Game",
      "Settings",
      "Exit",
   };
   int y = 48;
   size_t i;

   clear_screen();
   draw_text(20, 24, 1, "Beetle NeoGeo Pocket");
   draw_text(20, 48, 1, "D-Pad: move   A: select   B: cancel");

   y = 96;
   for (i = 0; i < sizeof(items) / sizeof(items[0]); i++)
   {
      char line[48];

      snprintf(line, sizeof(line), "%s%s",
            (int)i == selected ? "> " : "  ", items[i]);
      draw_text(40, y, ((int)i == selected) ? 1 : 0, line);
      y += 28;
   }
}

menu_action_t menu_main(char **rom_path_out)
{
   cont_state_t *state;
   uint32_t previous = 0;
   int selected = 0;
   menu_action_t action = MENU_ACTION_NONE;
   char *picked = NULL;

   if (rom_path_out)
      *rom_path_out = NULL;

   dc_video_menu_begin();

   for (;;)
   {
      uint32_t pressed;

      draw_main_menu(selected);
      dc_ui_present(true);

      state = poll_controller();
      if (!state)
      {
         thd_sleep(16);
         continue;
      }

      pressed = state->buttons & ~previous;
      previous = state->buttons;

      if (pressed & CONT_DPAD_UP)
      {
         if (selected > 0)
            selected--;
      }
      else if (pressed & CONT_DPAD_DOWN)
      {
         if (selected < 2)
            selected++;
      }
      else if (pressed & CONT_A)
      {
         if (selected == 0)
         {
            picked = menu_pick_rom();
            if (picked)
            {
               action = MENU_ACTION_LOAD;
               if (rom_path_out)
                  *rom_path_out = picked;
               else
                  free(picked);
            }
            break;
         }
         else if (selected == 1)
         {
            menu_settings();
         }
         else
         {
            action = MENU_ACTION_QUIT;
            break;
         }
      }
      else if (pressed & CONT_B || pressed & CONT_START)
      {
         action = MENU_ACTION_QUIT;
         break;
      }

      thd_sleep(16);
   }

   dc_video_menu_end();
   return action;
}

static void draw_settings_menu(int selected, const dc_settings_t *settings,
      const char *rom_path)
{
   char line[80];
   int y = 24;

   clear_screen();
   draw_text(20, y, 1, "Settings");
   y += 24;
   draw_text(20, y, 1, "Left/Right: change   A: toggle   B: save");
   y += 32;

   snprintf(line, sizeof(line), "%sVolume: %3u",
         (selected == 0) ? "> " : "  ", settings->volume);
   draw_text(20, y, (selected == 0) ? 1 : 0, line);
   y += 24;

   snprintf(line, sizeof(line), "%sScale: %ux",
         (selected == 1) ? "> " : "  ", settings->scale);
   draw_text(20, y, (selected == 1) ? 1 : 0, line);
   y += 24;

   snprintf(line, sizeof(line), "%sAudio: %s",
         (selected == 2) ? "> " : "  ",
         settings->audio_enabled ? "ON" : "OFF");
   draw_text(20, y, (selected == 2) ? 1 : 0, line);
   y += 24;

   snprintf(line, sizeof(line), "%sVideo: %s",
         (selected == 3) ? "> " : "  ",
         dc_video_output_name((dc_video_output_t)settings->video_output));
   draw_text(20, y, (selected == 3) ? 1 : 0, line);
   y += 24;

   snprintf(line, sizeof(line), "%sRenderer: %s",
         (selected == 4) ? "> " : "  ",
         dc_video_renderer_name((dc_video_renderer_t)settings->video_renderer));
   draw_text(20, y, (selected == 4) ? 1 : 0, line);
   y += 24;

   snprintf(line, sizeof(line), "%sVSync: %s",
         (selected == 5) ? "> " : "  ",
         settings->vsync ? "ON" : "OFF");
   draw_text(20, y, (selected == 5) ? 1 : 0, line);
   y += 24;

   snprintf(line, sizeof(line), "%sAuto load: %s",
         (selected == 6) ? "> " : "  ",
         settings->auto_load_state ? "ON" : "OFF");
   draw_text(20, y, (selected == 6) ? 1 : 0, line);
   y += 24;

   snprintf(line, sizeof(line), "%sVMU LCD: %s",
         (selected == 7) ? "> " : "  ",
         settings->vmu_lcd ? "ON" : "OFF");
   draw_text(20, y, (selected == 7) ? 1 : 0, line);
   y += 24;

   snprintf(line, sizeof(line), "%sVMU save: %s",
         (selected == 8) ? "> " : "  ",
         settings->vmu_save_sync ? "ON" : "OFF");
   draw_text(20, y, (selected == 8) ? 1 : 0, line);
   y += 24;

   snprintf(line, sizeof(line), "%sSave dir: %s",
         (selected == 9) ? "> " : "  ", settings->save_dir);
   draw_text(20, y, (selected == 9) ? 1 : 0, line);
   y += 24;

   snprintf(line, sizeof(line), "  VMU: %u   Cable: %s   %ux%u",
         dc_vmu_device_count(),
         dc_video_cable_name(dc_video_get_cable()),
         dc_video_width(), dc_video_height());
   draw_text(20, y, 0, line);
   y += 20;
   snprintf(line, sizeof(line), "  Config: %s", DC_SETTINGS_PATH);
   draw_text(20, y, 0, line);
   y += 20;

   if (rom_path)
   {
      snprintf(line, sizeof(line), "  State: %s   Battery: %s",
            dc_saves_state_exists(rom_path) ? "yes" : "no",
            dc_saves_flash_exists(rom_path) ? "yes" : "no");
      draw_text(20, y, 0, line);
      y += 20;
   }

   draw_text(20, y, 0, "  Start+Y/X state  L/R battery");
}

void menu_settings_for_rom(const char *rom_path)
{
   dc_settings_t *settings = dc_settings_get();
   cont_state_t *state;
   uint32_t previous = 0;
   int selected = 0;
   bool dirty = false;

   dc_video_menu_begin();

   for (;;)
   {
      uint32_t pressed;

      draw_settings_menu(selected, settings, rom_path);
      dc_ui_present(true);

      state = poll_controller();
      if (!state)
      {
         thd_sleep(16);
         continue;
      }

      pressed = state->buttons & ~previous;
      previous = state->buttons;

      if (pressed & CONT_DPAD_UP)
      {
         if (selected > 0)
            selected--;
      }
      else if (pressed & CONT_DPAD_DOWN)
      {
         if (selected < 9)
            selected++;
      }
      else if (pressed & CONT_DPAD_LEFT || pressed & CONT_DPAD_RIGHT)
      {
         int delta = (pressed & CONT_DPAD_RIGHT) ? 1 : -1;

         if (selected == 0)
         {
            int vol = (int)settings->volume + delta * 10;

            if (vol < 0)
               vol = 0;
            if (vol > 255)
               vol = 255;
            settings->volume = (uint8_t)vol;
            dirty = true;
         }
         else if (selected == 1)
         {
            int scale = (int)settings->scale + delta;

            if (scale < DC_SETTINGS_SCALE_MIN)
               scale = DC_SETTINGS_SCALE_MIN;
            if (scale > DC_SETTINGS_SCALE_MAX)
               scale = DC_SETTINGS_SCALE_MAX;
            settings->scale = (uint8_t)scale;
            dc_video_reinit_for_scale((dc_video_output_t)settings->video_output,
                  settings->scale);
            dirty = true;
         }
         else if (selected == 3)
         {
            int next = (int)settings->video_output + delta;

            if (next < 0)
               next = DC_VIDEO_OUTPUT_COUNT - 1;
            if (next >= DC_VIDEO_OUTPUT_COUNT)
               next = 0;

            settings->video_output = (uint8_t)next;
            dc_video_reinit_for_scale((dc_video_output_t)settings->video_output,
                  settings->scale);
            dirty = true;
         }
         else if (selected == 4)
         {
            int next = (int)settings->video_renderer + delta;

            if (next < 0)
               next = DC_VIDEO_RENDERER_COUNT - 1;
            if (next >= DC_VIDEO_RENDERER_COUNT)
               next = 0;

            settings->video_renderer = (uint8_t)next;
            dc_video_set_renderer((dc_video_renderer_t)settings->video_renderer);
            dirty = true;
         }
         else if (selected == 9)
         {
            if (delta > 0)
            {
               strncpy(settings->save_dir, "/sd/ngp", sizeof(settings->save_dir) - 1);
               strncpy(settings->system_dir, "/sd", sizeof(settings->system_dir) - 1);
            }
            else
            {
               strncpy(settings->save_dir, "/ide/ngp", sizeof(settings->save_dir) - 1);
               strncpy(settings->system_dir, "/ide", sizeof(settings->system_dir) - 1);
            }
            settings->save_dir[sizeof(settings->save_dir) - 1]     = '\0';
            settings->system_dir[sizeof(settings->system_dir) - 1] = '\0';
            dirty = true;
         }
      }
      else if (pressed & CONT_A)
      {
         if (selected == 2)
            settings->audio_enabled = !settings->audio_enabled;
         else if (selected == 5)
            settings->vsync = !settings->vsync;
         else if (selected == 6)
            settings->auto_load_state = !settings->auto_load_state;
         else if (selected == 7)
            settings->vmu_lcd = !settings->vmu_lcd;
         else if (selected == 8)
            settings->vmu_save_sync = !settings->vmu_save_sync;

         if (selected == 2 || selected == 5 || selected == 6
               || selected == 7 || selected == 8)
            dirty = true;
      }
      else if (pressed & CONT_B || pressed & CONT_START)
         break;

      thd_sleep(16);
   }

   if (dirty)
      dc_settings_save(settings);

   dc_video_menu_end();
}

void menu_settings(void)
{
   menu_settings_for_rom(NULL);
}
