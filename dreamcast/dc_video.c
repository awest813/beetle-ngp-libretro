/* Shared Dreamcast video backend (KallistiOS / RGB555 VRAM) */

#include "dc_video.h"

#include <stdlib.h>
#include <string.h>

static dc_video_output_t active_output = DC_VIDEO_OUTPUT_AUTO;
static bool video_ready;

static int pick_display_mode(dc_video_output_t pref, int8_t cable)
{
   if (pref == DC_VIDEO_OUTPUT_VGA)
      return DM_640x480_VGA;

   if (pref == DC_VIDEO_OUTPUT_TV)
   {
      if (cable == CT_RGB)
         return DM_640x480;
      return DM_640x480_NTSC_IL;
   }

   switch (cable)
   {
      case CT_VGA:
         return DM_640x480_VGA;
      case CT_RGB:
         return DM_640x480;
      case CT_COMPOSITE:
      case CT_NONE:
      default:
         return DM_640x480_NTSC_IL;
   }
}

int8_t dc_video_get_cable(void)
{
   return vid_check_cable();
}

const char *dc_video_cable_name(int8_t cable)
{
   switch (cable)
   {
      case CT_VGA:
         return "VGA";
      case CT_RGB:
         return "RGB";
      case CT_COMPOSITE:
         return "Composite";
      case CT_NONE:
         return "None";
      default:
         return "Unknown";
   }
}

const char *dc_video_output_name(dc_video_output_t output)
{
   switch (output)
   {
      case DC_VIDEO_OUTPUT_AUTO:
         return "Auto";
      case DC_VIDEO_OUTPUT_VGA:
         return "VGA";
      case DC_VIDEO_OUTPUT_TV:
         return "TV";
      default:
         return "Auto";
   }
}

bool dc_video_init(dc_video_output_t output)
{
   int mode = pick_display_mode(output, dc_video_get_cable());

   vid_set_mode(mode, PM_RGB555);
   active_output = output;
   video_ready   = true;
   return true;
}

void dc_video_shutdown(void)
{
   video_ready = false;
}

bool dc_video_reinit(dc_video_output_t output)
{
   return dc_video_init(output);
}

unsigned dc_video_width(void)
{
   if (video_ready && vid_mode)
      return vid_mode->width;
   return 640;
}

unsigned dc_video_height(void)
{
   if (video_ready && vid_mode)
      return vid_mode->height;
   return 480;
}

dc_video_output_t dc_video_active_output(void)
{
   return active_output;
}

int dc_video_active_mode(void)
{
   return pick_display_mode(active_output, dc_video_get_cable());
}

void dc_video_clear_screen(void)
{
   unsigned w = dc_video_width();
   unsigned h = dc_video_height();

   memset(vram_s, 0, w * h * sizeof(uint16_t));
}

void dc_video_present(const uint16_t *pixels, bool vsync)
{
   unsigned w = dc_video_width();
   unsigned h = dc_video_height();
   size_t bytes = (size_t)w * h * sizeof(uint16_t);

   if (!pixels)
      return;

   if (vsync)
      vid_waitvbl();

   memcpy(vram_s, pixels, bytes);
}

dc_video_blitter_t *dc_video_blitter_create(unsigned scale)
{
   dc_video_blitter_t *blitter;

   blitter = (dc_video_blitter_t *)calloc(1, sizeof(*blitter));
   if (!blitter)
      return NULL;

   blitter->buf_w = dc_video_width();
   blitter->buf_h = dc_video_height();
   blitter->scale = scale ? scale : 1;

   blitter->buffer = (uint16_t *)calloc(
         (size_t)blitter->buf_w * blitter->buf_h, sizeof(uint16_t));
   if (!blitter->buffer)
   {
      free(blitter);
      return NULL;
   }

   return blitter;
}

void dc_video_blitter_destroy(dc_video_blitter_t *blitter)
{
   if (!blitter)
      return;

   free(blitter->buffer);
   free(blitter);
}

void dc_video_blitter_set_scale(dc_video_blitter_t *blitter, unsigned scale)
{
   if (!blitter)
      return;

   if (scale < 1)
      scale = 1;

   blitter->scale = scale;
}

void dc_video_blitter_clear(dc_video_blitter_t *blitter)
{
   if (!blitter || !blitter->buffer)
      return;

   memset(blitter->buffer, 0,
         (size_t)blitter->buf_w * blitter->buf_h * sizeof(uint16_t));
}

void dc_video_blitter_rgb555(dc_video_blitter_t *blitter,
      const void *src, unsigned src_w, unsigned src_h, size_t src_pitch)
{
   const uint16_t *src_row;
   unsigned x, y, sx, sy;
   unsigned offset_x, offset_y;
   unsigned draw_w, draw_h;
   unsigned scale;

   if (!blitter || !blitter->buffer || !src || !src_w || !src_h)
      return;

   scale = blitter->scale;
   dc_video_blitter_clear(blitter);

   draw_w = src_w * scale;
   draw_h = src_h * scale;

   if (draw_w > blitter->buf_w)
   {
      scale = blitter->buf_w / src_w;
      if (scale < 1)
         scale = 1;
      draw_w = src_w * scale;
      draw_h = src_h * scale;
   }

   if (draw_h > blitter->buf_h)
   {
      scale = blitter->buf_h / src_h;
      if (scale < 1)
         scale = 1;
      draw_w = src_w * scale;
      draw_h = src_h * scale;
   }

   blitter->scale = scale;
   offset_x = (blitter->buf_w - draw_w) / 2;
   offset_y = (blitter->buf_h - draw_h) / 2;

   for (y = 0; y < src_h; y++)
   {
      src_row = (const uint16_t *)((const uint8_t *)src + y * src_pitch);

      for (x = 0; x < src_w; x++)
      {
         uint16_t pixel = src_row[x];

         for (sy = 0; sy < scale; sy++)
         {
            for (sx = 0; sx < scale; sx++)
            {
               blitter->buffer[(offset_y + y * scale + sy) * blitter->buf_w
                  + (offset_x + x * scale + sx)] = pixel;
            }
         }
      }
   }
}

void dc_video_blitter_present(dc_video_blitter_t *blitter, bool vsync)
{
   if (!blitter)
      return;

   dc_video_present(blitter->buffer, vsync);
}
