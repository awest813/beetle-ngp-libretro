#include "dc_saves.h"
#include "dc_settings.h"

#include <libretro.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void make_basename(const char *rom_path, char *out, size_t out_len)
{
   const char *base;
   char *dot;

   if (!rom_path || !out || !out_len)
      return;

   base = strrchr(rom_path, '/');
   base = base ? base + 1 : rom_path;

   strncpy(out, base, out_len - 1);
   out[out_len - 1] = '\0';

   dot = strrchr(out, '.');
   if (dot)
      *dot = '\0';
}

static bool write_file_atomic(const char *path, const void *data, size_t size)
{
   char tmp[280];
   FILE *file;

   snprintf(tmp, sizeof(tmp), "%s.tmp", path);
   file = fopen(tmp, "wb");
   if (!file)
      return false;

   if (fwrite(data, 1, size, file) != size)
   {
      fclose(file);
      remove(tmp);
      return false;
   }

   fclose(file);
   remove(path);

   if (rename(tmp, path) != 0)
   {
      remove(tmp);
      return false;
   }

   return true;
}

bool dc_saves_ensure_dir(void)
{
   const dc_settings_t *cfg = dc_settings_get();

   return mkdir(cfg->save_dir, 0755) == 0 || errno == EEXIST;
}

void dc_saves_basename(const char *rom_path, char *out, size_t out_len)
{
   make_basename(rom_path, out, out_len);
}

void dc_saves_state_path(const char *rom_path, char *out, size_t out_len)
{
   char name[128];
   const dc_settings_t *cfg = dc_settings_get();

   make_basename(rom_path, name, sizeof(name));
   snprintf(out, out_len, "%s/%s.state", cfg->save_dir, name);
}

void dc_saves_flash_path(const char *rom_path, char *out, size_t out_len)
{
   char name[128];
   const dc_settings_t *cfg = dc_settings_get();

   make_basename(rom_path, name, sizeof(name));
   snprintf(out, out_len, "%s/%s.flash", cfg->save_dir, name);
}

bool dc_saves_state_exists(const char *rom_path)
{
   char path[256];

   if (!rom_path)
      return false;

   dc_saves_state_path(rom_path, path, sizeof(path));
   return access(path, R_OK) == 0;
}

bool dc_saves_flash_exists(const char *rom_path)
{
   char path[256];

   if (!rom_path)
      return false;

   dc_saves_flash_path(rom_path, path, sizeof(path));
   return access(path, R_OK) == 0;
}

bool dc_saves_save_state(const char *rom_path)
{
   char path[256];
   size_t size;
   void *buffer;

   if (!rom_path)
      return false;

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

   if (!dc_saves_ensure_dir())
   {
      free(buffer);
      return false;
   }

   dc_saves_state_path(rom_path, path, sizeof(path));

   if (!write_file_atomic(path, buffer, size))
   {
      free(buffer);
      return false;
   }

   free(buffer);
   return true;
}

bool dc_saves_load_state(const char *rom_path)
{
   char path[256];
   size_t size;
   void *buffer;
   FILE *file;
   long file_size;

   if (!rom_path)
      return false;

   dc_saves_state_path(rom_path, path, sizeof(path));
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
   return true;
}
