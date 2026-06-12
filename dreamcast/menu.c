#include "menu.h"

#include "dc_input.h"
#include "dc_saves.h"
#include "dc_settings.h"
#include "dc_ui.h"
#include "dc_video.h"
#include "dc_vmu.h"

#include <kos.h>

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAX_ROM_ENTRIES 64
#define ROM_NAME_LEN    48
#define SETTINGS_COUNT  10

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

static void menu_frame_begin(const char *title, const char *subtitle,
      const char *footer)
{
   dc_ui_begin_frame();
   dc_ui_draw_header(title, subtitle);
   if (footer)
      dc_ui_draw_footer(footer);
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

static void draw_main_menu(dc_ui_list_t *list)
{
   static const char *items[] = {
      "Load Game",
      "Settings",
      "Exit",
   };
   dc_ui_layout_t layout;
   int i;
   int y;
   int count = (int)(sizeof(items) / sizeof(items[0]));
   int visible = dc_ui_list_visible_rows(list);
   int row_w;

   dc_ui_get_layout(&layout);
   row_w = (int)layout.width - layout.margin_x * 2 - 12;

   menu_frame_begin("Beetle NeoGeo Pocket", "D-Pad: move   A: select   B: cancel",
         NULL);

   y = list->content_y;
   for (i = list->scroll; i < count && i < list->scroll + visible; i++)
   {
      dc_ui_draw_menu_row(y, row_w, i == list->selected, items[i], NULL);
      y += list->row_h;
   }

   dc_ui_draw_scrollbar((int)layout.width - layout.margin_x - 8,
         list->content_y, list->content_h, count, visible, list->scroll);
   dc_ui_draw_footer("Dreamcast launcher");
}

menu_action_t menu_main(char **rom_path_out)
{
   dc_menu_input_t input;
   dc_ui_layout_t layout;
   dc_ui_list_t list;
   menu_action_t action = MENU_ACTION_NONE;
   char *picked = NULL;
   int count = 3;

   if (rom_path_out)
      *rom_path_out = NULL;

   dc_video_menu_begin();
   dc_menu_input_reset(&input);
   dc_ui_get_layout(&layout);
   dc_ui_list_init(&list, &layout);

   for (;;)
   {
      uint32_t pressed;

      draw_main_menu(&list);
      dc_ui_present(true);

      if (!dc_menu_input_poll(&input, 0, &pressed))
      {
         thd_sleep(16);
         continue;
      }

      if (pressed & CONT_DPAD_UP)
         dc_ui_list_move(&list, -1, count);
      else if (pressed & CONT_DPAD_DOWN)
         dc_ui_list_move(&list, 1, count);
      else if (pressed & CONT_A)
      {
         if (list.selected == 0)
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
         else if (list.selected == 1)
            menu_settings();
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

static void draw_rom_menu(dc_ui_list_t *list)
{
   dc_ui_layout_t layout;
   char subtitle[64];
   int i;
   int y;
   int visible;
   int row_w;

   dc_ui_get_layout(&layout);
   visible = dc_ui_list_visible_rows(list);
   row_w     = (int)layout.width - layout.margin_x * 2 - 12;

   snprintf(subtitle, sizeof(subtitle), "%d ROM(s) found   D-Pad: move   A: load   B: back",
         rom_count);

   menu_frame_begin("Select ROM", subtitle, NULL);

   if (rom_count == 0)
   {
      dc_ui_draw_text(layout.margin_x, list->content_y, DC_UI_COLOR_TEXT,
            "No ROMs found on /sd or /ide");
      dc_ui_draw_footer("Place .ngp/.ngc files under /sd/ngp");
      return;
   }

   y = list->content_y;
   for (i = list->scroll; i < rom_count && i < list->scroll + visible; i++)
   {
      dc_ui_draw_menu_row(y, row_w, i == list->selected, roms[i].name, NULL);
      y += list->row_h;
   }

   dc_ui_draw_scrollbar((int)layout.width - layout.margin_x - 8,
         list->content_y, list->content_h, rom_count, visible, list->scroll);

   snprintf(subtitle, sizeof(subtitle), "%d / %d", list->selected + 1, rom_count);
   dc_ui_draw_footer(subtitle);
}

char *menu_pick_rom(void)
{
   dc_menu_input_t input;
   dc_ui_layout_t layout;
   dc_ui_list_t list;
   char *result = NULL;

   collect_roms();

   dc_menu_input_reset(&input);
   dc_ui_get_layout(&layout);
   dc_ui_list_init(&list, &layout);

   if (rom_count > 0)
      dc_ui_list_clamp(&list, rom_count);

   for (;;)
   {
      uint32_t pressed;

      draw_rom_menu(&list);
      dc_ui_present(true);

      if (!dc_menu_input_poll(&input, 0, &pressed))
      {
         thd_sleep(16);
         continue;
      }

      if (rom_count == 0)
      {
         if (pressed & (CONT_A | CONT_B | CONT_START))
            break;
      }
      else if (pressed & CONT_DPAD_UP)
         dc_ui_list_move(&list, -1, rom_count);
      else if (pressed & CONT_DPAD_DOWN)
         dc_ui_list_move(&list, 1, rom_count);
      else if (pressed & CONT_A)
      {
         result = strdup(roms[list.selected].path);
         break;
      }
      else if (pressed & CONT_B || pressed & CONT_START)
         break;

      thd_sleep(16);
   }

   return result;
}

static void format_setting_value(int index, const dc_settings_t *settings,
      char *value, size_t value_len)
{
   switch (index)
   {
      case 0:
         snprintf(value, value_len, "%3u", settings->volume);
         break;
      case 1:
         snprintf(value, value_len, "%ux", settings->scale);
         break;
      case 2:
         snprintf(value, value_len, "%s", settings->audio_enabled ? "ON" : "OFF");
         break;
      case 3:
         snprintf(value, value_len, "%s",
               dc_video_output_name((dc_video_output_t)settings->video_output));
         break;
      case 4:
         snprintf(value, value_len, "%s",
               dc_video_renderer_name((dc_video_renderer_t)settings->video_renderer));
         break;
      case 5:
         snprintf(value, value_len, "%s", settings->vsync ? "ON" : "OFF");
         break;
      case 6:
         snprintf(value, value_len, "%s",
               settings->auto_load_state ? "ON" : "OFF");
         break;
      case 7:
         snprintf(value, value_len, "%s", settings->vmu_lcd ? "ON" : "OFF");
         break;
      case 8:
         snprintf(value, value_len, "%s", settings->vmu_save_sync ? "ON" : "OFF");
         break;
      case 9:
         snprintf(value, value_len, "%s", settings->save_dir);
         break;
      default:
         value[0] = '\0';
         break;
   }
}

static void draw_settings_menu(dc_ui_list_t *list, const dc_settings_t *settings,
      const char *rom_path)
{
   static const char *labels[SETTINGS_COUNT] = {
      "Volume",
      "Scale",
      "Audio",
      "Video",
      "Renderer",
      "VSync",
      "Auto load state",
      "VMU LCD",
      "VMU save sync",
      "Save directory",
   };
   dc_ui_layout_t layout;
   dc_ui_list_t draw_list;
   char value[64];
   char line[96];
   int i;
   int y;
   int visible;
   int row_w;
   int status_y;
   int status_h = 52;

   dc_ui_get_layout(&layout);
   draw_list = *list;
   draw_list.content_h -= status_h;
   if (draw_list.content_h < draw_list.row_h * 2)
      draw_list.content_h = draw_list.row_h * 2;
   dc_ui_list_clamp(&draw_list, SETTINGS_COUNT);
   visible = dc_ui_list_visible_rows(&draw_list);
   row_w   = (int)layout.width - layout.margin_x * 2 - 12;

   menu_frame_begin("Settings", "Left/Right: change   A: toggle   B: save & back",
         "Start+Y/X state   L/R battery (in-game)");

   y = draw_list.content_y;
   for (i = draw_list.scroll; i < SETTINGS_COUNT && i < draw_list.scroll + visible; i++)
   {
      format_setting_value(i, settings, value, sizeof(value));
      dc_ui_draw_menu_row(y, row_w, i == draw_list.selected, labels[i], value);
      y += draw_list.row_h;
   }

   dc_ui_draw_scrollbar((int)layout.width - layout.margin_x - 8,
         draw_list.content_y, draw_list.content_h, SETTINGS_COUNT, visible,
         draw_list.scroll);

   status_y = draw_list.content_y + draw_list.content_h + 4;
   dc_ui_draw_panel(layout.margin_x, status_y,
         (int)layout.width - layout.margin_x * 2, status_h - 4);

   snprintf(line, sizeof(line), "VMU: %u   %ux%u   %s",
         dc_vmu_device_count(), dc_video_width(), dc_video_height(),
         dc_video_cable_name(dc_video_get_cable()));
   dc_ui_draw_text(layout.margin_x + 8, status_y + 8, DC_UI_COLOR_TEXT_DIM, line);

   snprintf(line, sizeof(line), "Config: %s", DC_SETTINGS_PATH);
   dc_ui_draw_text(layout.margin_x + 8, status_y + 24, DC_UI_COLOR_TEXT_DIM, line);

   if (rom_path)
   {
      snprintf(line, sizeof(line), "State: %s   Battery: %s",
            dc_saves_state_exists(rom_path) ? "yes" : "no",
            dc_saves_flash_exists(rom_path) ? "yes" : "no");
      dc_ui_draw_text(layout.margin_x + 8, status_y + 40, DC_UI_COLOR_TEXT_DIM, line);
   }
}

void menu_settings_for_rom(const char *rom_path)
{
   dc_settings_t *settings = dc_settings_get();
   dc_menu_input_t input;
   dc_ui_layout_t layout;
   dc_ui_list_t list;
   bool dirty = false;

   dc_video_menu_begin();
   dc_menu_input_reset(&input);
   dc_ui_get_layout(&layout);
   dc_ui_list_init(&list, &layout);
   list.content_h -= 52;
   if (list.content_h < list.row_h * 2)
      list.content_h = list.row_h * 2;
   dc_ui_list_clamp(&list, SETTINGS_COUNT);

   for (;;)
   {
      uint32_t pressed;

      draw_settings_menu(&list, settings, rom_path);
      dc_ui_present(true);

      if (!dc_menu_input_poll(&input, 0, &pressed))
      {
         thd_sleep(16);
         continue;
      }

      if (pressed & CONT_DPAD_UP)
         dc_ui_list_move(&list, -1, SETTINGS_COUNT);
      else if (pressed & CONT_DPAD_DOWN)
         dc_ui_list_move(&list, 1, SETTINGS_COUNT);
      else if (pressed & CONT_DPAD_LEFT || pressed & CONT_DPAD_RIGHT)
      {
         int delta = (pressed & CONT_DPAD_RIGHT) ? 1 : -1;

         if (list.selected == 0)
         {
            int vol = (int)settings->volume + delta * 10;

            if (vol < 0)
               vol = 0;
            if (vol > 255)
               vol = 255;
            settings->volume = (uint8_t)vol;
            dirty = true;
         }
         else if (list.selected == 1)
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
         else if (list.selected == 3)
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
         else if (list.selected == 4)
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
         else if (list.selected == 9)
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
         if (list.selected == 2)
            settings->audio_enabled = !settings->audio_enabled;
         else if (list.selected == 5)
            settings->vsync = !settings->vsync;
         else if (list.selected == 6)
            settings->auto_load_state = !settings->auto_load_state;
         else if (list.selected == 7)
            settings->vmu_lcd = !settings->vmu_lcd;
         else if (list.selected == 8)
            settings->vmu_save_sync = !settings->vmu_save_sync;

         if (list.selected == 2 || list.selected == 5 || list.selected == 6
               || list.selected == 7 || list.selected == 8)
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

static void draw_pause_menu(dc_ui_list_t *list, const char *rom_path)
{
   static const char *items[] = {
      "Resume",
      "Save State",
      "Load State",
      "Settings",
      "Quit to Menu",
   };
   dc_ui_layout_t layout;
   char subtitle[80];
   const char *basename;
   int i;
   int y;
   int count = (int)(sizeof(items) / sizeof(items[0]));
   int visible;
   int row_w;

   dc_ui_get_layout(&layout);
   visible = dc_ui_list_visible_rows(list);
   row_w   = (int)layout.width - layout.margin_x * 2 - 12;

   basename = rom_path ? strrchr(rom_path, '/') : NULL;
   basename = basename ? basename + 1 : "Game";

   snprintf(subtitle, sizeof(subtitle), "%s", basename);

   menu_frame_begin("Paused", subtitle,
         "Start+Y/X quick save/load   L/R battery");

   y = list->content_y;
   for (i = list->scroll; i < count && i < list->scroll + visible; i++)
   {
      dc_ui_draw_menu_row(y, row_w, i == list->selected, items[i], NULL);
      y += list->row_h;
   }

   dc_ui_draw_scrollbar((int)layout.width - layout.margin_x - 8,
         list->content_y, list->content_h, count, visible, list->scroll);
}

menu_pause_action_t menu_pause(const char *rom_path)
{
   dc_menu_input_t input;
   dc_ui_layout_t layout;
   dc_ui_list_t list;
   menu_pause_action_t action = MENU_PAUSE_RESUME;
   int count = 5;

   dc_video_menu_begin();
   dc_menu_input_reset(&input);
   dc_ui_get_layout(&layout);
   dc_ui_list_init(&list, &layout);

   for (;;)
   {
      uint32_t pressed;

      draw_pause_menu(&list, rom_path);
      dc_ui_present(true);

      if (!dc_menu_input_poll(&input, 0, &pressed))
      {
         thd_sleep(16);
         continue;
      }

      if (pressed & CONT_DPAD_UP)
         dc_ui_list_move(&list, -1, count);
      else if (pressed & CONT_DPAD_DOWN)
         dc_ui_list_move(&list, 1, count);
      else if (pressed & CONT_A || pressed & CONT_START)
      {
         switch (list.selected)
         {
            case 0:
               action = MENU_PAUSE_RESUME;
               break;
            case 1:
               action = MENU_PAUSE_SAVE;
               break;
            case 2:
               action = MENU_PAUSE_LOAD;
               break;
            case 3:
               action = MENU_PAUSE_SETTINGS;
               break;
            default:
               action = MENU_PAUSE_QUIT;
               break;
         }
         break;
      }
      else if (pressed & CONT_B)
      {
         action = MENU_PAUSE_RESUME;
         break;
      }

      thd_sleep(16);
   }

   dc_video_menu_end();
   return action;
}
