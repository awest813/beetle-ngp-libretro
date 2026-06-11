/* RetroArch Dreamcast video driver (KallistiOS / RGB555 VRAM) */

#include <kos.h>

#include <string.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_inline.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../configuration.h"
#include "../../driver.h"

#define DC_SCREEN_W 640
#define DC_SCREEN_H 480
#define DC_SCALE_MAX 4

typedef struct dc_video
{
   uint16_t *scaled;
   unsigned out_w;
   unsigned out_h;
   unsigned scale;
   unsigned rotation;
   bool vsync;
   bool rgb32;
   bool keep_aspect;
} dc_video_t;

static void dc_clear_screen(uint16_t *fb, unsigned w, unsigned h)
{
   memset(fb, 0, w * h * sizeof(uint16_t));
}

static void dc_blit_frame(dc_video_t *dc, const void *data,
      unsigned width, unsigned height, size_t pitch)
{
   const uint8_t *src8 = (const uint8_t *)data;
   unsigned x, y, sx, sy;
   unsigned offset_x, offset_y;
   unsigned draw_w, draw_h;

   if (!data || !dc->scaled || !width || !height)
      return;

   dc_clear_screen(dc->scaled, dc->out_w, dc->out_h);

   draw_w = width * dc->scale;
   draw_h = height * dc->scale;

   if (dc->keep_aspect)
   {
      if (draw_w > dc->out_w)
      {
         dc->scale = dc->out_w / width;
         if (dc->scale < 1)
            dc->scale = 1;
         draw_w = width * dc->scale;
         draw_h = height * dc->scale;
      }

      if (draw_h > dc->out_h)
      {
         dc->scale = dc->out_h / height;
         if (dc->scale < 1)
            dc->scale = 1;
         draw_w = width * dc->scale;
         draw_h = height * dc->scale;
      }
   }

   offset_x = (dc->out_w - draw_w) / 2;
   offset_y = (dc->out_h - draw_h) / 2;

   if (dc->rgb32)
   {
      for (y = 0; y < height; y++)
      {
         const uint32_t *row = (const uint32_t *)(src8 + y * pitch);

         for (x = 0; x < width; x++)
         {
            uint32_t px = row[x];
            uint16_t rgb555 = ((px >> 9) & 0x7C00)
                            | ((px >> 6) & 0x03E0)
                            | ((px >> 3) & 0x001F);

            for (sy = 0; sy < dc->scale; sy++)
            {
               for (sx = 0; sx < dc->scale; sx++)
               {
                  dc->scaled[(offset_y + y * dc->scale + sy) * dc->out_w
                     + (offset_x + x * dc->scale + sx)] = rgb555;
               }
            }
         }
      }
   }
   else
   {
      for (y = 0; y < height; y++)
      {
         const uint16_t *row = (const uint16_t *)(src8 + y * pitch);

         for (x = 0; x < width; x++)
         {
            uint16_t pixel = row[x];

            for (sy = 0; sy < dc->scale; sy++)
            {
               for (sx = 0; sx < dc->scale; sx++)
               {
                  dc->scaled[(offset_y + y * dc->scale + sy) * dc->out_w
                     + (offset_x + x * dc->scale + sx)] = pixel;
               }
            }
         }
      }
   }
}

static void *dc_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   dc_video_t *dc = (dc_video_t *)calloc(1, sizeof(*dc));

   (void)input;
   (void)input_data;

   if (!dc)
      return NULL;

   vid_set_mode(DM_640x480, PM_RGB555);

   dc->out_w       = DC_SCREEN_W;
   dc->out_h       = DC_SCREEN_H;
   dc->scale       = 3;
   dc->vsync       = video->vsync;
   dc->rgb32       = video->rgb32;
   dc->keep_aspect = true;

   dc->scaled = (uint16_t *)calloc(dc->out_w * dc->out_h, sizeof(uint16_t));
   if (!dc->scaled)
   {
      free(dc);
      return NULL;
   }

   dc_clear_screen(dc->scaled, dc->out_w, dc->out_h);
   memcpy(vram_s, dc->scaled, dc->out_w * dc->out_h * sizeof(uint16_t));

   return dc;
}

static bool dc_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *frame_info)
{
   dc_video_t *dc = (dc_video_t *)data;

   (void)frame_count;
   (void)msg;
   (void)frame_info;

   if (!dc)
      return false;

   if (frame)
      dc_blit_frame(dc, frame, width, height, pitch);

   if (dc->vsync)
      vid_waitvbl();

   memcpy(vram_s, dc->scaled, dc->out_w * dc->out_h * sizeof(uint16_t));
   return true;
}

static void dc_gfx_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   dc_video_t *dc = (dc_video_t *)data;

   (void)adaptive_vsync_enabled;
   (void)swap_interval;

   if (dc)
      dc->vsync = !state;
}

static bool dc_gfx_alive(void *data) { (void)data; return true; }
static bool dc_gfx_focus(void *data) { (void)data; return true; }
static bool dc_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}
static bool dc_gfx_has_windowed(void *data) { (void)data; return false; }
static bool dc_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;
   return false;
}

static void dc_gfx_free(void *data)
{
   dc_video_t *dc = (dc_video_t *)data;

   if (!dc)
      return;

   free(dc->scaled);
   free(dc);
}

static void dc_gfx_set_viewport(void *data, unsigned width, unsigned height,
      bool force_full, bool allow_rotate)
{
   dc_video_t *dc = (dc_video_t *)data;
   unsigned scale;

   (void)force_full;
   (void)allow_rotate;

   if (!dc || !width || !height)
      return;

   scale = DC_SCREEN_W / width;
   if (DC_SCREEN_H / height < scale)
      scale = DC_SCREEN_H / height;

   if (scale < 1)
      scale = 1;
   if (scale > DC_SCALE_MAX)
      scale = DC_SCALE_MAX;

   dc->scale = scale;
}

static void dc_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   dc_video_t *dc = (dc_video_t *)data;

   if (!dc || !vp)
      return;

   vp->x      = 0;
   vp->y      = 0;
   vp->width  = dc->out_w;
   vp->height = dc->out_h;
   vp->full_width  = dc->out_w;
   vp->full_height = dc->out_h;
}

video_driver_t video_dc = {
   dc_gfx_init,
   dc_gfx_frame,
   dc_gfx_set_nonblock_state,
   dc_gfx_alive,
   dc_gfx_focus,
   dc_gfx_suppress_screensaver,
   dc_gfx_has_windowed,
   dc_gfx_set_shader,
   dc_gfx_free,
   "dc",
   dc_gfx_set_viewport,
   NULL,
   dc_gfx_viewport_info,
   NULL,
   NULL,
#ifdef HAVE_OVERLAY
   NULL,
#endif
   NULL,
   NULL,
   NULL,
   NULL
};
