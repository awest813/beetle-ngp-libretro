#include "menu.h"

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
#define SCREEN_PITCH    640

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

static cont_state_t *poll_controller(void)
{
   maple_device_t *device = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

   if (!device)
      return NULL;

   return (cont_state_t *)maple_dev_status(device);
}

static void draw_menu(int selected)
{
   int y = 24;
   int i;

   memset(vram_s, 0, SCREEN_PITCH * 480 * sizeof(uint16_t));
   bfont_draw_str(vram_s + y * SCREEN_PITCH + 20, SCREEN_PITCH, 1,
         "Beetle NGP - Select ROM");
   y += 24;
   bfont_draw_str(vram_s + y * SCREEN_PITCH + 20, SCREEN_PITCH, 1,
         "D-Pad: move   A: load   B: cancel");
   y += 32;

   if (rom_count == 0)
   {
      bfont_draw_str(vram_s + y * SCREEN_PITCH + 20, SCREEN_PITCH, 1,
            "No ROMs found on /sd or /ide");
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
      bfont_draw_str(vram_s + y * SCREEN_PITCH + 20, SCREEN_PITCH,
            (i == selected) ? 1 : 0, line);
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

   vid_set_mode(DM_640x480, PM_RGB555);

   for (;;)
   {
      uint32_t pressed;

      draw_menu(selected);

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
