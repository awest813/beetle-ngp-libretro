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
#define SETTINGS_COUNT  11
#define SPLASH_FRAMES   90

typedef struct
{
   char path[256];
   char name[ROM_NAME_LEN];
} rom_entry_t;

typedef enum main_item_id
{
   MAIN_ITEM_CONTINUE = 0,
   MAIN_ITEM_LOAD,
   MAIN_ITEM_SETTINGS,
   MAIN_ITEM_EXIT
} main_item_id_t;

typedef struct main_item
{
   main_item_id_t id;
   const char *label;
   char value[48];
} main_item_t;

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
static unsigned settings_saved_flash;

static void menu_frame_begin(const char *title, const char *subtitle)
{
   dc_ui_begin_frame();
   dc_ui_draw_header(title, subtitle);
}

static const char *basename_only(const char *path)
{
   const char *slash;

   if (!path)
      return "";

   slash = strrchr(path, '/');
   return slash ? slash + 1 : path;
}

static void truncate_middle(char *dst, size_t dst_len, const char *src,
      unsigned max_chars)
{
   size_t len;

   if (!dst || dst_len == 0)
      return;

   if (!src)
   {
      dst[0] = '\0';
      return;
   }

   len = strlen(src);
   if (len <= max_chars || max_chars < 8)
   {
      strncpy(dst, src, dst_len - 1);
      dst[dst_len - 1] = '\0';
      return;
   }

   snprintf(dst, dst_len, "%.*s...%s",
         (int)(max_chars / 2 - 2), src,
         src + len - (max_chars / 2 - 1));
}

static bool menu_confirm(const char *title, const char *message,
      const char *detail, bool default_yes)
{
   dc_menu_input_t input;
   int selected = default_yes ? 0 : 1;
   dc_ui_layout_t layout;
   int row_w;
   int y;

   dc_menu_input_reset(&input);
   dc_ui_get_layout(&layout);
   row_w = (int)layout.width - layout.margin_x * 2 - 12;
   y     = layout.content_y + layout.row_h * 2;

   for (;;)
   {
      uint32_t pressed;

      menu_frame_begin(title, message);
      if (detail && detail[0])
         dc_ui_draw_text(layout.margin_x, layout.content_y, DC_UI_COLOR_TEXT_DIM,
               detail);
      dc_ui_draw_menu_row(y, row_w, selected == 0, "Yes", NULL);
      dc_ui_draw_menu_row(y + layout.row_h, row_w, selected == 1, "No", NULL);
      dc_ui_draw_footer("A: confirm   B: cancel   Left/Right: choose");
      dc_ui_present(true);

      if (!dc_menu_input_poll(&input, 0, &pressed))
      {
         thd_sleep(16);
         continue;
      }

      if (pressed & (CONT_DPAD_LEFT | CONT_DPAD_RIGHT))
         selected ^= 1;
      else if (pressed & CONT_A)
         return selected == 0;
      else if (pressed & CONT_B || pressed & CONT_START)
         return false;

      thd_sleep(16);
   }
}

static int build_main_items(main_item_t *items, int max_items)
{
   const dc_settings_t *cfg = dc_settings_get();
   int count = 0;

   if (max_items < 3)
      return 0;

   if (dc_settings_last_rom_valid(cfg))
   {
      items[count].id = MAIN_ITEM_CONTINUE;
      items[count].label = "Continue";
      truncate_middle(items[count].value, sizeof(items[count].value),
            basename_only(cfg->last_rom), 22);
      count++;
   }

   items[count].id    = MAIN_ITEM_LOAD;
   items[count].label = "Load Game";
   items[count].value[0] = '\0';
   count++;

   items[count].id    = MAIN_ITEM_SETTINGS;
   items[count].label = "Settings";
   items[count].value[0] = '\0';
   count++;

   items[count].id    = MAIN_ITEM_EXIT;
   items[count].label = "Exit";
   items[count].value[0] = '\0';
   count++;

   return count;
}

void menu_splash(void)
{
   dc_menu_input_t input;
   dc_ui_layout_t layout;
   unsigned frame;

   dc_video_menu_begin();
   dc_menu_input_reset(&input);
   dc_ui_get_layout(&layout);

   for (frame = 0; frame < SPLASH_FRAMES; frame++)
   {
      uint32_t pressed;
      char line[64];

      menu_frame_begin("Beetle NeoGeo Pocket", "Dreamcast launcher");
      dc_ui_draw_text(layout.margin_x, layout.content_y + 8, DC_UI_COLOR_TEXT,
            "Neo Geo Pocket Color emulator");
      snprintf(line, sizeof(line), "%ux%u  %s  %s",
            dc_video_width(), dc_video_height(),
            dc_video_cable_name(dc_video_get_cable()),
            dc_video_renderer_name(dc_video_get_renderer()));
      dc_ui_draw_text(layout.margin_x, layout.content_y + 32,
            DC_UI_COLOR_TEXT_DIM, line);
      dc_ui_draw_text(layout.margin_x, layout.content_y + 56,
            DC_UI_COLOR_TEXT_DIM, "Press any button to continue");
      dc_ui_draw_footer("Place ROMs in /sd/ngp");
      dc_ui_present(true);

      if (dc_menu_input_poll(&input, 0, &pressed) && pressed)
         break;

      thd_sleep(16);
   }

   dc_video_menu_end();
}

void menu_loading_screen(const char *rom_path)
{
   dc_ui_layout_t layout;
   char line[80];

   dc_video_menu_begin();
   dc_ui_get_layout(&layout);

   menu_frame_begin("Loading", basename_only(rom_path));
   dc_ui_draw_text(layout.margin_x, layout.content_y + 24, DC_UI_COLOR_TEXT,
         "Please wait...");
   snprintf(line, sizeof(line), "%s", rom_path ? rom_path : "");
   truncate_middle(line, sizeof(line), line, 34);
   dc_ui_draw_text(layout.margin_x, layout.content_y + 48, DC_UI_COLOR_TEXT_DIM,
         line);
   dc_ui_present(true);
   dc_video_menu_end();
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

static int rom_entry_cmp(const void *a, const void *b)
{
   const rom_entry_t *ra = (const rom_entry_t *)a;
   const rom_entry_t *rb = (const rom_entry_t *)b;

   return strcasecmp(ra->name, rb->name);
}

static void collect_roms(void)
{
   size_t i;

   rom_count = 0;

   for (i = 0; i < sizeof(scan_dirs) / sizeof(scan_dirs[0]); i++)
      scan_directory(scan_dirs[i]);

   if (rom_count > 1)
      qsort(roms, (size_t)rom_count, sizeof(roms[0]), rom_entry_cmp);
}

static int find_rom_index(const char *path)
{
   int i;

   if (!path)
      return 0;

   for (i = 0; i < rom_count; i++)
   {
      if (!strcmp(roms[i].path, path))
         return i;
   }

   return 0;
}

static void format_rom_badges(const char *path, char *badges, size_t len)
{
   bool state  = dc_saves_state_exists(path);
   bool battery = dc_saves_flash_exists(path);

   badges[0] = '\0';
   if (state && battery)
      snprintf(badges, len, "S B");
   else if (state)
      snprintf(badges, len, "S");
   else if (battery)
      snprintf(badges, len, "B");
}

static void draw_main_menu(dc_ui_list_t *list, const main_item_t *items,
      int count)
{
   dc_ui_layout_t layout;
   int i;
   int y;
   int visible;
   int row_w;

   dc_ui_get_layout(&layout);
   visible = dc_ui_list_visible_rows(list);
   row_w   = (int)layout.width - layout.margin_x * 2 - 12;

   menu_frame_begin("Beetle NeoGeo Pocket", "D-Pad: move (hold to scroll fast)");

   y = list->content_y;
   for (i = list->scroll; i < count && i < list->scroll + visible; i++)
   {
      dc_ui_draw_menu_row(y, row_w, i == list->selected, items[i].label,
            items[i].value[0] ? items[i].value : NULL);
      y += list->row_h;
   }

   dc_ui_draw_scrollbar((int)layout.width - layout.margin_x - 8,
         list->content_y, list->content_h, count, visible, list->scroll);
   dc_ui_draw_footer("A: select   B: back");
}

menu_action_t menu_main(char **rom_path_out)
{
   dc_menu_input_t input;
   dc_ui_layout_t layout;
   dc_ui_list_t list;
   main_item_t items[4];
   menu_action_t action = MENU_ACTION_NONE;
   char *picked = NULL;
   int count;
   int nav;

   if (rom_path_out)
      *rom_path_out = NULL;

   count = build_main_items(items, (int)(sizeof(items) / sizeof(items[0])));

   dc_video_menu_begin();
   dc_menu_input_reset(&input);
   dc_ui_get_layout(&layout);
   dc_ui_list_init(&list, &layout);
   dc_ui_list_clamp(&list, count);

   for (;;)
   {
      uint32_t pressed;
      main_item_id_t chosen;

      draw_main_menu(&list, items, count);
      dc_ui_present(true);

      if (!dc_menu_input_poll(&input, 0, &pressed))
      {
         thd_sleep(16);
         continue;
      }

      nav = dc_menu_input_list_delta(&input, pressed);
      if (nav != 0)
         dc_ui_list_move(&list, nav, count);
      else if (pressed & CONT_A)
      {
         chosen = items[list.selected].id;

         if (chosen == MAIN_ITEM_CONTINUE)
         {
            picked = strdup(dc_settings_get()->last_rom);
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
         else if (chosen == MAIN_ITEM_LOAD)
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
         else if (chosen == MAIN_ITEM_SETTINGS)
            menu_settings();
         else if (chosen == MAIN_ITEM_EXIT)
         {
            if (menu_confirm("Exit?", "Quit Beetle NGP?", NULL, false))
            {
               action = MENU_ACTION_QUIT;
               break;
            }
         }
      }
      else if (pressed & CONT_B || pressed & CONT_START)
      {
         if (menu_confirm("Exit?", "Quit Beetle NGP?", NULL, false))
         {
            action = MENU_ACTION_QUIT;
            break;
         }
      }

      thd_sleep(16);
   }

   dc_video_menu_end();
   return action;
}

static void draw_rom_menu(dc_ui_list_t *list)
{
   dc_ui_layout_t layout;
   char subtitle[72];
   char badges[8];
   int i;
   int y;
   int visible;
   int row_w;

   dc_ui_get_layout(&layout);
   visible = dc_ui_list_visible_rows(list);
   row_w   = (int)layout.width - layout.margin_x * 2 - 12;

   snprintf(subtitle, sizeof(subtitle), "%d ROM(s)   S=state  B=battery",
         rom_count);

   menu_frame_begin("Select ROM", subtitle);

   if (rom_count == 0)
   {
      dc_ui_draw_text(layout.margin_x, list->content_y, DC_UI_COLOR_TEXT,
            "No ROMs found on /sd or /ide");
      dc_ui_draw_hint(list->content_y + 28,
            "Copy .ngp / .ngc files to /sd/ngp");
      dc_ui_draw_footer("Press A or B to go back");
      return;
   }

   y = list->content_y;
   for (i = list->scroll; i < rom_count && i < list->scroll + visible; i++)
   {
      format_rom_badges(roms[i].path, badges, sizeof(badges));
      dc_ui_draw_menu_row(y, row_w, i == list->selected, roms[i].name,
            badges[0] ? badges : NULL);
      y += list->row_h;
   }

   dc_ui_draw_scrollbar((int)layout.width - layout.margin_x - 8,
         list->content_y, list->content_h, rom_count, visible, list->scroll);

   snprintf(subtitle, sizeof(subtitle), "%d / %d   Hold D-Pad to scroll",
         list->selected + 1, rom_count);
   dc_ui_draw_footer(subtitle);
}

char *menu_pick_rom(void)
{
   dc_menu_input_t input;
   dc_ui_layout_t layout;
   dc_ui_list_t list;
   const dc_settings_t *cfg = dc_settings_get();
   char *result = NULL;
   int nav;

   collect_roms();

   dc_menu_input_reset(&input);
   dc_ui_get_layout(&layout);
   dc_ui_list_init(&list, &layout);

   if (rom_count > 0)
   {
      list.selected = find_rom_index(cfg->last_rom);
      dc_ui_list_clamp(&list, rom_count);
   }

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
      else
      {
         nav = dc_menu_input_list_delta(&input, pressed);
         if (nav != 0)
            dc_ui_list_move(&list, nav, rom_count);
         else if (pressed & CONT_A)
         {
            result = strdup(roms[list.selected].path);
            break;
         }
         else if (pressed & CONT_B || pressed & CONT_START)
            break;
      }

      thd_sleep(16);
   }

   return result;
}

static const char *setting_hints[SETTINGS_COUNT] = {
   "Emulated audio loudness (0-255)",
   "Screen upscale factor (2x-4x)",
   "Enable or mute game audio",
   "TV mode / VGA output selection",
   "Software blit or PVR hardware",
   "Wait for vertical blank",
   "Load .state file when game starts",
   "Show game preview on VMU LCD",
   "Mirror battery saves to VMU",
   "Where .state and .flash files go",
   "Frames to skip (0=none, 1-3)",
};

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
      case 10:
         snprintf(value, value_len, "%u", settings->frame_skip);
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
      "Frame skip",
   };
   dc_ui_layout_t layout;
   dc_ui_list_t draw_list;
   char value[64];
   char line[96];
   char footer[80];
   int i;
   int y;
   int visible;
   int row_w;
   int status_y;
   int status_h = 52;
   int hint_y;

   dc_ui_get_layout(&layout);
   draw_list = *list;
   draw_list.content_h -= status_h + draw_list.row_h + 8;
   if (draw_list.content_h < draw_list.row_h * 2)
      draw_list.content_h = draw_list.row_h * 2;
   dc_ui_list_clamp(&draw_list, SETTINGS_COUNT);
   visible = dc_ui_list_visible_rows(&draw_list);
   row_w   = (int)layout.width - layout.margin_x * 2 - 12;

   menu_frame_begin("Settings", "Left/Right: change   A: toggle   B: save & back");

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

   hint_y = draw_list.content_y + draw_list.content_h + 4;
   dc_ui_draw_hint(hint_y, setting_hints[draw_list.selected]);

   status_y = hint_y + draw_list.row_h + 10;
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

   if (settings_saved_flash > 0)
      snprintf(footer, sizeof(footer), "Settings saved");
   else
      snprintf(footer, sizeof(footer), "Start+Y/X state   L/R battery (in-game)");
   dc_ui_draw_footer(footer);
}

void menu_settings_for_rom(const char *rom_path)
{
   dc_settings_t *settings = dc_settings_get();
   dc_menu_input_t input;
   dc_ui_layout_t layout;
   dc_ui_list_t list;
   bool dirty = false;
   int nav;

   dc_video_menu_begin();
   dc_menu_input_reset(&input);
   dc_ui_get_layout(&layout);
   dc_ui_list_init(&list, &layout);
   list.content_h -= 52 + list.row_h + 8;
   if (list.content_h < list.row_h * 2)
      list.content_h = list.row_h * 2;
   dc_ui_list_clamp(&list, SETTINGS_COUNT);

   for (;;)
   {
      uint32_t pressed;

      if (settings_saved_flash > 0)
         settings_saved_flash--;

      draw_settings_menu(&list, settings, rom_path);
      dc_ui_present(true);

      if (!dc_menu_input_poll(&input, 0, &pressed))
      {
         thd_sleep(16);
         continue;
      }

      nav = dc_menu_input_list_delta(&input, pressed);
      if (nav != 0)
         dc_ui_list_move(&list, nav, SETTINGS_COUNT);
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
         else if (list.selected == 10)
         {
            int fs = (int)settings->frame_skip + delta;

            if (fs < 0)
               fs = DC_SETTINGS_FRAMESKIP_MAX;
            if (fs > DC_SETTINGS_FRAMESKIP_MAX)
               fs = 0;
            settings->frame_skip = (uint8_t)fs;
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
   {
      unsigned flash;

      dc_settings_save(settings);
      for (flash = 0; flash < 45; flash++)
      {
         settings_saved_flash = 45 - flash;
         draw_settings_menu(&list, settings, rom_path);
         dc_ui_present(true);
         thd_sleep(16);
      }
      settings_saved_flash = 0;
   }

   dc_video_menu_end();
}

void menu_settings(void)
{
   menu_settings_for_rom(NULL);
}

static const char *pause_hints[] = {
   "Return to the game",
   "Write current progress to .state",
   "Restore from saved .state (replaces progress)",
   "Audio, video, VMU, and save options",
   "Return to launcher (battery auto-saved)",
};

static void draw_pause_menu(dc_ui_list_t *list, const char *rom_path)
{
   static const char *items[] = {
      "Resume",
      "Save State",
      "Load State",
      "Settings",
      "Quit to Menu",
   };
   static const char *values[] = {
      NULL, NULL, NULL, NULL, NULL,
   };
   dc_ui_layout_t layout;
   char subtitle[80];
   char badges[16];
   char footer[72];
   int i;
   int y;
   int count = (int)(sizeof(items) / sizeof(items[0]));
   int visible;
   int row_w;
   int hint_y;

   dc_ui_get_layout(&layout);
   visible = dc_ui_list_visible_rows(list);
   row_w   = (int)layout.width - layout.margin_x * 2 - 12;

   format_rom_badges(rom_path, badges, sizeof(badges));
   snprintf(subtitle, sizeof(subtitle), "%s%s%s",
         basename_only(rom_path),
         badges[0] ? "   " : "",
         badges);

   menu_frame_begin("Paused", subtitle);

   y = list->content_y;
   for (i = list->scroll; i < count && i < list->scroll + visible; i++)
   {
      dc_ui_draw_menu_row(y, row_w, i == list->selected, items[i],
            values[i]);
      y += list->row_h;
   }

   dc_ui_draw_scrollbar((int)layout.width - layout.margin_x - 8,
         list->content_y, list->content_h, count, visible, list->scroll);

   hint_y = list->content_y + list->content_h - list->row_h - 2;
   dc_ui_draw_hint(hint_y, pause_hints[list->selected]);

   snprintf(footer, sizeof(footer), "B: resume   Start also resumes");
   dc_ui_draw_footer(footer);
}

menu_pause_action_t menu_pause(const char *rom_path)
{
   dc_menu_input_t input;
   dc_ui_layout_t layout;
   dc_ui_list_t list;
   menu_pause_action_t action = MENU_PAUSE_RESUME;
   int count = 5;
   int nav;

   dc_video_menu_begin();
   dc_menu_input_reset(&input);
   dc_ui_get_layout(&layout);
   dc_ui_list_init(&list, &layout);
   list.content_h -= list.row_h + 12;
   if (list.content_h < list.row_h * count)
      list.content_h = list.row_h * count;

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

      nav = dc_menu_input_list_delta(&input, pressed);
      if (nav != 0)
         dc_ui_list_move(&list, nav, count);
      else if (pressed & CONT_A || (pressed & CONT_START && list.selected == 0))
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
               if (dc_saves_state_exists(rom_path))
               {
                  if (!menu_confirm("Load state?", "Replace current progress?",
                        "This cannot be undone", false))
                     continue;
               }
               action = MENU_PAUSE_LOAD;
               break;
            case 3:
               action = MENU_PAUSE_SETTINGS;
               break;
            default:
               if (!menu_confirm("Quit?", "Return to launcher?",
                     "Battery save runs on exit", false))
                  continue;
               action = MENU_PAUSE_QUIT;
               break;
         }
         break;
      }
      else if (pressed & CONT_B || pressed & CONT_START)
      {
         action = MENU_PAUSE_RESUME;
         break;
      }

      thd_sleep(16);
   }

   dc_video_menu_end();
   return action;
}
