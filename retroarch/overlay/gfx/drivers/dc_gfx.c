/* RetroArch Dreamcast video driver (KallistiOS / RGB555 VRAM) */

#include "../../dreamcast/dc_video.h"
#include "../../dreamcast/dc_pvr.h"
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
#include "../../gfx/video_driver.h"

#define DC_SCALE_MAX 4

typedef struct dc_video
{
   dc_video_blitter_t *blitter;
   uint16_t *menu_buffer;
   unsigned menu_width;
   unsigned menu_height;
   unsigned scale;
   unsigned rotation;
   bool vsync;
   bool rgb32;
   bool keep_aspect;
   bool menu_visible;
   bool menu_fullscreen;
} dc_video_t;

static void dc_convert_rgb32_row(const uint32_t *src, uint16_t *dst, unsigned width)
{
   unsigned x;

   for (x = 0; x < width; x++)
   {
      uint32_t px = src[x];

      dst[x] = (uint16_t)(((px >> 9) & 0x7C00)
                        | ((px >> 6) & 0x03E0)
                        | ((px >> 3) & 0x001F));
   }
}

static bool dc_menu_buffer_resize(dc_video_t *dc, unsigned width, unsigned height)
{
   size_t bytes;

   if (!dc || !width || !height)
      return false;

   if (dc->menu_buffer && dc->menu_width == width && dc->menu_height == height)
      return true;

   free(dc->menu_buffer);
   bytes = (size_t)width * height * sizeof(uint16_t);
   dc->menu_buffer = (uint16_t *)memalign(32, bytes);

   if (!dc->menu_buffer)
   {
      dc->menu_width  = 0;
      dc->menu_height = 0;
      return false;
   }

   dc->menu_width  = width;
   dc->menu_height = height;
   return true;
}

static void dc_store_menu_frame(dc_video_t *dc, const void *frame, bool rgb32,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned y;

   if (!dc || !frame || !width || !height)
      return;

   if (!dc_menu_buffer_resize(dc, width, height))
      return;

   if (rgb32)
   {
      for (y = 0; y < height; y++)
      {
         const uint32_t *row = (const uint32_t *)((const uint8_t *)frame + y * pitch);

         dc_convert_rgb32_row(row, dc->menu_buffer + y * width, width);
      }
   }
   else
   {
      unsigned copy_w = width * sizeof(uint16_t);

      if (pitch == copy_w)
         memcpy(dc->menu_buffer, frame, (size_t)copy_w * height);
      else
      {
         for (y = 0; y < height; y++)
         {
            const uint8_t *row = (const uint8_t *)frame + y * pitch;

            memcpy(dc->menu_buffer + y * width, row, copy_w);
         }
      }
   }

   if (dc_video_get_renderer() == DC_VIDEO_RENDERER_PVR)
      dc_pvr_menu_set_frame(dc->menu_buffer, width, height);
}

static void dc_composite_menu(dc_video_t *dc)
{
   unsigned x, y;
   unsigned offset_x, offset_y;
   unsigned draw_w, draw_h;

   if (!dc || !dc->menu_visible || !dc->menu_buffer || !dc->blitter)
      return;

   if (!dc->menu_width || !dc->menu_height)
      return;

   draw_w = dc->menu_width;
   draw_h = dc->menu_height;

   if (draw_w > dc->blitter->buf_w)
      draw_w = dc->blitter->buf_w;
   if (draw_h > dc->blitter->buf_h)
      draw_h = dc->blitter->buf_h;

   offset_x = (dc->blitter->buf_w - draw_w) / 2;
   offset_y = (dc->blitter->buf_h - draw_h) / 2;

   for (y = 0; y < draw_h; y++)
   {
      uint16_t *dst = dc->blitter->buffer + (offset_y + y) * dc->blitter->buf_w + offset_x;
      const uint16_t *src = dc->menu_buffer + y * dc->menu_width;

      for (x = 0; x < draw_w; x++)
      {
         uint16_t px = src[x];

         if (px)
            dst[x] = px;
      }
   }
}

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

   dc_video_set_renderer((dc_video_renderer_t)cfg->video_renderer);
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
#ifdef HAVE_MENU
   bool menu_is_alive = false;

   if (frame_info)
      menu_is_alive = (frame_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   (void)frame_count;
   (void)msg;

   if (!dc)
      return false;

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, frame_info);
#endif

   if (dc_video_get_renderer() == DC_VIDEO_RENDERER_PVR)
   {
      if (frame)
         dc_video_present_rgb555(frame, width, height, pitch, dc->scale, dc->vsync);
      else
         dc_video_present_rgb555(NULL, width, height, pitch, dc->scale, dc->vsync);
   }
   else
   {
      if (frame)
         dc_blit_frame(dc, frame, width, height, pitch);

      if (dc->menu_visible)
         dc_composite_menu(dc);

      if (frame || dc->menu_visible)
         dc_video_blitter_present(dc->blitter, dc->vsync);
   }

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

   dc_pvr_menu_set_visible(false);
   free(dc->menu_buffer);
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

static void dc_gfx_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   dc_video_t *dc = (dc_video_t *)data;

   (void)alpha;

   if (!dc)
      return;

   dc_store_menu_frame(dc, frame, rgb32, width, height,
         width * (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t)));
}

static void dc_gfx_set_texture_enable(void *data, bool enable, bool fullscreen)
{
   dc_video_t *dc = (dc_video_t *)data;

   if (!dc)
      return;

   dc->menu_visible    = enable;
   dc->menu_fullscreen = fullscreen;

   if (dc_video_get_renderer() == DC_VIDEO_RENDERER_PVR)
      dc_pvr_menu_set_visible(enable);
}

static const video_poke_interface_t dc_poke_interface = {
   NULL, /* get_flags */
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   dc_gfx_set_texture_frame,
   dc_gfx_set_texture_enable,
   NULL, /* set_osd_msg */
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void dc_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;

   *iface = &dc_poke_interface;
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
   dc_gfx_get_poke_interface,
   NULL,
   NULL,
   NULL,
#ifdef HAVE_GFX_WIDGETS
   NULL
#endif
};
