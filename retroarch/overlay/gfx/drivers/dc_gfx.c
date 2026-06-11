/* RetroArch Dreamcast video driver (KallistiOS / RGB555 VRAM) */

#include "../../dreamcast/dc_video.h"
#include "../../dreamcast/dc_settings.h"

#include <kos.h>

#include <stdlib.h>
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

#define DC_SCALE_MAX 4

typedef struct dc_video
{
   dc_video_blitter_t *blitter;
   unsigned scale;
   unsigned rotation;
   bool vsync;
   bool rgb32;
   bool keep_aspect;
} dc_video_t;

static void dc_blit_frame(dc_video_t *dc, const void *data,
      unsigned width, unsigned height, size_t pitch)
{
   const uint8_t *src8 = (const uint8_t *)data;
   unsigned x, y, sx, sy;
   unsigned offset_x, offset_y;
   unsigned draw_w, draw_h;
   unsigned scale;

   if (!data || !dc->blitter || !width || !height)
      return;

   scale = dc->scale;
   dc_video_blitter_clear(dc->blitter);

   draw_w = width * scale;
   draw_h = height * scale;

   if (dc->keep_aspect)
   {
      if (draw_w > dc_video_width())
      {
         scale = dc_video_width() / width;
         if (scale < 1)
            scale = 1;
         draw_w = width * scale;
         draw_h = height * scale;
      }

      if (draw_h > dc_video_height())
      {
         scale = dc_video_height() / height;
         if (scale < 1)
            scale = 1;
         draw_w = width * scale;
         draw_h = height * scale;
      }
   }

   dc->scale = scale;
   offset_x = (dc_video_width() - draw_w) / 2;
   offset_y = (dc_video_height() - draw_h) / 2;

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

            for (sy = 0; sy < scale; sy++)
            {
               for (sx = 0; sx < scale; sx++)
               {
                  dc->blitter->buffer[(offset_y + y * scale + sy) * dc->blitter->buf_w
                     + (offset_x + x * scale + sx)] = rgb555;
               }
            }
         }
      }
   }
   else
   {
      dc_video_blitter_set_scale(dc->blitter, scale);
      dc_video_blitter_rgb555(dc->blitter, data, width, height, pitch);
      dc->scale = dc->blitter->scale;
   }
}

static void *dc_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   dc_video_t *dc = (dc_video_t *)calloc(1, sizeof(*dc));
   const dc_settings_t *cfg;

   (void)input;
   (void)input_data;

   if (!dc)
      return NULL;

   dc_settings_load(dc_settings_get());
   cfg = dc_settings_get();

   dc_video_init_for_scale((dc_video_output_t)cfg->video_output, cfg->scale);

   dc->scale       = cfg->scale ? cfg->scale : 3;
   dc->vsync       = video->vsync;
   dc->rgb32       = video->rgb32;
   dc->keep_aspect = true;

   dc->blitter = dc_video_blitter_create(dc->scale);
   if (!dc->blitter)
   {
      free(dc);
      return NULL;
   }

   dc_video_blitter_clear(dc->blitter);
   dc_video_blitter_present(dc->blitter, false);

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

   dc_video_blitter_present(dc->blitter, dc->vsync);
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

   dc_video_blitter_destroy(dc->blitter);
   dc_video_shutdown();
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

   scale = dc_video_width() / width;
   if (dc_video_height() / height < scale)
      scale = dc_video_height() / height;

   if (scale < 1)
      scale = 1;
   if (scale > DC_SCALE_MAX)
      scale = DC_SCALE_MAX;

   dc->scale = scale;
   dc_video_blitter_set_scale(dc->blitter, scale);
}

static void dc_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   dc_video_t *dc = (dc_video_t *)data;

   if (!dc || !vp)
      return;

   vp->x      = 0;
   vp->y      = 0;
   vp->width  = dc_video_width();
   vp->height = dc_video_height();
   vp->full_width  = dc_video_width();
   vp->full_height = dc_video_height();
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
