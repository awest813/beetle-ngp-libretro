/* KallistiOS libretro host for Beetle NeoGeo Pocket
 *
 * Build from the dreamcast/ directory after sourcing the KOS environ:
 *   make
 *
 * Usage:
 *   beetlengp.elf /sd/path/to/game.ngp
 */

#include <kos.h>
#include <dc/maple/controller.h>
#include <dc/sound/stream.h>

#include <libretro.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FB_WIDTH  160
#define FB_HEIGHT 152
#define SCALE     3
#define AUDIO_RING_SAMPLES 16384

static const char *system_dir = "/sd";
static const char *save_dir   = "/sd/ngp";

static enum retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_RGB565;

static uint16_t *scaled_frame;
static unsigned scaled_w;
static unsigned scaled_h;

static int16_t audio_ring[AUDIO_RING_SAMPLES * 2];
static volatile uint32_t audio_read;
static volatile uint32_t audio_write;

static maple_device_t *controller;

static void video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
   const uint16_t *src;
   uint16_t *dst;
   unsigned x, y, sx, sy;
   unsigned offset_x, offset_y;

   if (!data || !scaled_frame)
      return;

   src = (const uint16_t *)data;

   if (width != FB_WIDTH || height != FB_HEIGHT)
      return;

   memset(scaled_frame, 0, scaled_w * scaled_h * sizeof(uint16_t));

   offset_x = (scaled_w - (width * SCALE)) / 2;
   offset_y = (scaled_h - (height * SCALE)) / 2;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint16_t pixel = src[y * (pitch / sizeof(uint16_t)) + x];

         for (sy = 0; sy < SCALE; sy++)
         {
            for (sx = 0; sx < SCALE; sx++)
            {
               dst = scaled_frame
                  + (offset_y + y * SCALE + sy) * scaled_w
                  + (offset_x + x * SCALE + sx);
               *dst = pixel;
            }
         }
      }
   }

   vid_waitvbl();
   memcpy(vram_l, scaled_frame, scaled_w * scaled_h * sizeof(uint16_t));
}

static size_t audio_sample_batch(const int16_t *data, size_t frames)
{
   size_t i;

   for (i = 0; i < frames; i++)
   {
      uint32_t next = (audio_write + 2) % (AUDIO_RING_SAMPLES * 2);

      if (next == audio_read)
         break;

      audio_ring[audio_write++] = data[i * 2];
      audio_ring[audio_write++] = data[i * 2 + 1];
      audio_write %= (AUDIO_RING_SAMPLES * 2);
   }

   return i;
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
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_CAN_DUPE:
         *(bool *)data = true;
         return true;
      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
         *(const char **)data = system_dir;
         return true;
      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         *(const char **)data = save_dir;
         return true;
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
         if (*(const enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_RGB565
               || *(const enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_0RGB1555)
         {
            pixel_format = *(const enum retro_pixel_format *)data;
            return true;
         }
         return false;
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
         return false;
      default:
         return false;
   }
}

static void *stream_callback(snd_stream_hnd_t hnd, int smp_req, int *smp_recv)
{
   int16_t *out = (int16_t *)snd_stream_get_data(hnd);
   int produced = 0;
   int i;

   (void)hnd;

   for (i = 0; i < smp_req; i++)
   {
      if (audio_read == audio_write)
         break;

      out[i * 2]     = audio_ring[audio_read++];
      out[i * 2 + 1] = audio_ring[audio_read++];
      audio_read %= (AUDIO_RING_SAMPLES * 2);
      produced++;
   }

   *smp_recv = produced;
   return out;
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

static const char *pick_rom_path(int argc, char **argv)
{
   static const char *candidates[] = {
      NULL,
      "/sd/rom.ngp",
      "/sd/game.ngp",
      "/ide/rom.ngp",
      "/ide/game.ngp",
      "/pc/rom.ngp",
   };

   size_t i;

   if (argc > 1 && argv[1] && argv[1][0])
      return argv[1];

   for (i = 1; i < sizeof(candidates) / sizeof(candidates[0]); i++)
   {
      FILE *probe = fopen(candidates[i], "rb");
      if (probe)
      {
         fclose(probe);
         return candidates[i];
      }
   }

   return NULL;
}

int main(int argc, char **argv)
{
   const char *rom_path;
   struct retro_game_info game;
   struct retro_system_av_info av_info;
   struct retro_system_info sys_info;
   snd_stream_hnd_t stream;
   uint8_t *rom_data = NULL;
   size_t rom_size = 0;

   vid_set_mode(DM_640x480, PM_RGB565);
   scaled_w = 640;
   scaled_h = 480;
   scaled_frame = (uint16_t *)calloc(scaled_w * scaled_h, sizeof(uint16_t));
   if (!scaled_frame)
   {
      printf("beetlengp: out of memory for framebuffer\n");
      return 1;
   }

   controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

   retro_set_environment(environment);
   retro_set_video_refresh(video_refresh);
   retro_set_audio_sample_batch(audio_sample_batch);
   retro_set_input_poll(input_poll);
   retro_set_input_state(input_state);

   retro_init();

   retro_get_system_info(&sys_info);
   printf("beetlengp: loaded core %s %s\n", sys_info.library_name, sys_info.library_version);

   rom_path = pick_rom_path(argc, argv);
   if (!rom_path)
   {
      printf("beetlengp: no ROM found. Usage: beetlengp.elf /sd/game.ngp\n");
      retro_deinit();
      free(scaled_frame);
      return 1;
   }

   rom_data = load_rom_file(rom_path, &rom_size);
   if (!rom_data)
   {
      printf("beetlengp: failed to read ROM '%s'\n", rom_path);
      retro_deinit();
      free(scaled_frame);
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
      retro_deinit();
      free(scaled_frame);
      return 1;
   }

   retro_get_system_av_info(&av_info);
   printf("beetlengp: running %s at %.2f fps / %u Hz\n",
         rom_path,
         av_info.timing.fps,
         av_info.timing.sample_rate);

   stream = snd_stream_alloc((uint32_t)av_info.timing.sample_rate, 2, stream_callback);
   if (stream)
      snd_stream_start(stream, 64, 0xffff);

   while (1)
   {
      cont_state_t *state;

      if (controller)
      {
         state = (cont_state_t *)maple_dev_status(controller);
         if (state && (state->buttons & CONT_START) && (state->buttons & CONT_A))
            break;
      }

      retro_run();
   }

   if (stream)
   {
      snd_stream_stop(stream);
      snd_stream_destroy(stream);
   }

   retro_unload_game();
   retro_deinit();
   free(rom_data);
   free(scaled_frame);

   return 0;
}
