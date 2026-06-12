/* Dreamcast VMU LCD status and optional flash-save mirroring */

#include "dc_vmu.h"
#include "dc_saves.h"

#include <kos.h>
#include <dc/maple.h>
#include <dc/maple/vmu.h>
#include <dc/vmu_fb.h>
#include <dc/vmu_pkg.h>
#include <dc/fs_vmu.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DC_VMU_MAX_DEVICES 8
#define DC_VMU_FLASH_EXT    "FLA"
#define DC_VMU_PRESENT_RATE 60

typedef struct dc_vmu_slot
{
   maple_device_t *dev;
} dc_vmu_slot_t;

static struct
{
   vmufb_t fb;
   dc_vmu_slot_t slots[DC_VMU_MAX_DEVICES];
   unsigned slot_count;
   bool lcd_enabled;
   bool save_sync_enabled;
   char game_name[20];
   bool battery_save;
   unsigned frame_counter;
   bool preview_valid;
   uint8_t preview[VMU_SCREEN_WIDTH * VMU_SCREEN_HEIGHT / 8];
} vmu;

static void vmu_make_filename(const char *basename, char *out, size_t out_len)
{
   size_t i;
   char stem[9];

   memset(stem, 0, sizeof(stem));
   strncpy(stem, basename, sizeof(stem) - 1);
   for (i = 0; stem[i]; i++)
      stem[i] = (char)toupper((unsigned char)stem[i]);

   snprintf(out, out_len, "%s.%s", stem, DC_VMU_FLASH_EXT);
}

static void vmu_make_path(const dc_vmu_slot_t *slot, const char *filename,
      char *out, size_t out_len)
{
   maple_device_t *dev = slot->dev;

   snprintf(out, out_len, "/vmu/%c%d/%s",
         'a' + (int)dev->port, (int)dev->unit + 1, filename);
}

static void vmu_scan_devices(void)
{
   unsigned i;

   vmu.slot_count = 0;

   for (i = 0; vmu.slot_count < DC_VMU_MAX_DEVICES; i++)
   {
      maple_device_t *dev = maple_enum_dev(i, MAPLE_FUNC_MEMCARD);

      if (!dev)
         break;

      vmu.slots[vmu.slot_count++].dev = dev;
   }
}

static void vmu_draw_status(const char *line1, const char *line2)
{
   const vmufb_font_t *font = vmu_get_font();

   vmufb_clear(&vmu.fb);

   if (font && line1 && line1[0])
      vmufb_print_string(&vmu.fb, font, line1);

   if (font && line2 && line2[0])
      vmufb_print_string_into(&vmu.fb, font, 0, 10, VMU_SCREEN_WIDTH,
            VMU_SCREEN_HEIGHT - 10, 0, line2);
}

static void vmu_present_all(void)
{
   unsigned i;

   if (!vmu.lcd_enabled || vmu.slot_count == 0)
      return;

   for (i = 0; i < vmu.slot_count; i++)
      vmufb_present(&vmu.fb, vmu.slots[i].dev);
}

static void vmu_build_preview(const uint16_t *frame, unsigned width, unsigned height,
      size_t pitch)
{
   unsigned y, x;

   if (!frame || !width || !height)
      return;

   memset(vmu.preview, 0, sizeof(vmu.preview));

   for (y = 0; y < VMU_SCREEN_HEIGHT; y++)
   {
      unsigned sy = y * height / VMU_SCREEN_HEIGHT;
      const uint16_t *row = (const uint16_t *)((const uint8_t *)frame + sy * pitch);

      for (x = 0; x < VMU_SCREEN_WIDTH; x++)
      {
         unsigned sx = x * width / VMU_SCREEN_WIDTH;
         uint16_t px = row[sx];
         unsigned lum = ((px >> 10) & 0x1f) + ((px >> 5) & 0x1f) + (px & 0x1f);
         size_t bit = (size_t)y * VMU_SCREEN_WIDTH + x;

         if (lum >= 24)
            vmu.preview[bit / 8] |= (uint8_t)(1u << (bit % 8));
      }
   }

   vmu.preview_valid = true;
}

static void vmu_draw_game_screen(void)
{
   const vmufb_font_t *font = vmu_get_font();
   char line2[20];

   vmufb_clear(&vmu.fb);

   if (vmu.preview_valid)
      vmufb_paint_area(&vmu.fb, 0, 0, VMU_SCREEN_WIDTH, VMU_SCREEN_HEIGHT,
            vmu.preview);

   if (font && vmu.game_name[0])
      vmufb_print_string_into(&vmu.fb, font, 0, 24, VMU_SCREEN_WIDTH, 8, 0,
            vmu.game_name);

   if (vmu.battery_save)
      snprintf(line2, sizeof(line2), "SAV");
   else
      line2[0] = '\0';

   if (font && line2[0])
      vmufb_print_string_into(&vmu.fb, font, 34, 24, 14, 8, 0, line2);
}

static void vmu_setup_default_pkg(vmu_pkg_t *pkg, const char *basename)
{
   memset(pkg, 0, sizeof(*pkg));
   snprintf(pkg->desc_short, sizeof(pkg->desc_short), "%.16s", basename);
   snprintf(pkg->desc_long, sizeof(pkg->desc_long), "Beetle NGP Flash");
   pkg->app_id        = 'NGP';
   pkg->icon_cnt      = 1;
   pkg->icon_anim_speed = 0;
   pkg->eyecatch_type = VMUPKG_EC_NONE;
}

static size_t vmu_pad_size(size_t size)
{
   size_t blocks = (size + 511) / 512;

   return blocks * 512;
}

bool dc_vmu_init(void)
{
   memset(&vmu, 0, sizeof(vmu));
   vmu.lcd_enabled      = true;
   vmu.save_sync_enabled = false;
   vmu_scan_devices();
   vmu_draw_status("Beetle NGP", vmu.slot_count ? "Ready" : "No VMU");
   vmu_present_all();
   return true;
}

void dc_vmu_shutdown(void)
{
   vmu_draw_status("Beetle NGP", "Bye");
   vmu_present_all();
   memset(&vmu, 0, sizeof(vmu));
}

void dc_vmu_set_enabled(bool lcd, bool save_sync)
{
   vmu.lcd_enabled       = lcd;
   vmu.save_sync_enabled = save_sync;
}

void dc_vmu_set_game(const char *rom_path, bool battery_save)
{
   char basename[128];

   dc_saves_basename(rom_path, basename, sizeof(basename));
   strncpy(vmu.game_name, basename, sizeof(vmu.game_name) - 1);
   vmu.game_name[sizeof(vmu.game_name) - 1] = '\0';
   vmu.battery_save = battery_save;
   vmu.frame_counter = 0;
   vmu.preview_valid = false;
}

void dc_vmu_present_idle(const char *message)
{
   if (!vmu.lcd_enabled)
      return;

   vmu_draw_status("Beetle NGP", message ? message : "");
   vmu_present_all();
}

void dc_vmu_feed_frame(const uint16_t *frame, unsigned width, unsigned height,
      size_t pitch)
{
   if (!vmu.lcd_enabled)
      return;

   vmu_build_preview(frame, width, height, pitch);
}

void dc_vmu_on_frame(void)
{
   if (!vmu.lcd_enabled)
      return;

   vmu.frame_counter++;
   if ((vmu.frame_counter % DC_VMU_PRESENT_RATE) != 0)
      return;

   if (vmu.preview_valid)
      vmu_draw_game_screen();
   else
      vmu_draw_status(vmu.game_name[0] ? vmu.game_name : "Beetle NGP",
            vmu.battery_save ? "SAV" : "");

   vmu_present_all();
}

unsigned dc_vmu_device_count(void)
{
   return vmu.slot_count;
}

bool dc_vmu_sync_flash_to_vmu(const char *rom_path)
{
   char flash_path[256];
   char filename[16];
   char basename[128];
   FILE *in;
   uint8_t *padded;
   size_t size, padded_size;
   vmu_pkg_t pkg;
   unsigned i;
   bool any = false;

   if (!vmu.save_sync_enabled || !rom_path || vmu.slot_count == 0)
      return false;

   dc_saves_flash_path(rom_path, flash_path, sizeof(flash_path));
   in = fopen(flash_path, "rb");
   if (!in)
      return false;

   fseek(in, 0, SEEK_END);
   size = (size_t)ftell(in);
   rewind(in);

   if (size == 0)
   {
      fclose(in);
      return false;
   }

   padded_size = vmu_pad_size(size);
   padded = (uint8_t *)calloc(1, padded_size);
   if (!padded)
   {
      fclose(in);
      return false;
   }

   if (fread(padded, 1, size, in) != size)
   {
      free(padded);
      fclose(in);
      return false;
   }

   fclose(in);

   dc_saves_basename(rom_path, basename, sizeof(basename));
   vmu_make_filename(basename, filename, sizeof(filename));
   vmu_setup_default_pkg(&pkg, basename);
   fs_vmu_set_default_header(&pkg);

   for (i = 0; i < vmu.slot_count; i++)
   {
      char path[64];
      FILE *out;

      vmu_make_path(&vmu.slots[i], filename, path, sizeof(path));
      out = fopen(path, "wb");
      if (!out)
         continue;

      if (fwrite(padded, 1, padded_size, out) == padded_size)
         any = true;

      fclose(out);
   }

   free(padded);
   return any;
}

bool dc_vmu_load_flash_from_vmu(const char *rom_path)
{
   char flash_path[256];
   char filename[16];
   char path[64];
   FILE *in, *out;
   uint8_t *data;
   long size;
   unsigned i;

   if (!vmu.save_sync_enabled || !rom_path || vmu.slot_count == 0)
      return false;

   if (dc_saves_flash_exists(rom_path))
      return false;

   char basename[128];

   dc_saves_flash_path(rom_path, flash_path, sizeof(flash_path));
   dc_saves_basename(rom_path, basename, sizeof(basename));
   vmu_make_filename(basename, filename, sizeof(filename));

   for (i = 0; i < vmu.slot_count; i++)
   {
      vmu_make_path(&vmu.slots[i], filename, path, sizeof(path));
      in = fopen(path, "rb");
      if (!in)
         continue;

      fseek(in, 0, SEEK_END);
      size = ftell(in);
      rewind(in);

      if (size <= 0)
      {
         fclose(in);
         continue;
      }

      data = (uint8_t *)malloc((size_t)size);
      if (!data)
      {
         fclose(in);
         return false;
      }

      if (fread(data, 1, (size_t)size, in) != (size_t)size)
      {
         free(data);
         fclose(in);
         continue;
      }

      fclose(in);

      dc_saves_ensure_dir();
      out = fopen(flash_path, "wb");
      if (!out)
      {
         free(data);
         return false;
      }

      fwrite(data, 1, (size_t)size, out);
      fclose(out);
      free(data);
      return true;
   }

   return false;
}
