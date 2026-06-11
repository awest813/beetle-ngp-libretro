/* Shared Dreamcast video backend (KallistiOS / RGB555 VRAM) */

#include "dc_video.h"

#include <stdlib.h>
#include <string.h>

static dc_video_output_t active_output = DC_VIDEO_OUTPUT_AUTO;
static unsigned active_scale = 3;
static bool video_ready;

static int8_t effective_cable(dc_video_output_t pref, int8_t cable)
{
   if (pref == DC_VIDEO_OUTPUT_VGA)
      return CT_VGA;

   if (pref == DC_VIDEO_OUTPUT_TV && cable == CT_VGA)
      return CT_COMPOSITE;

   return cable;
}

static int pick_display_mode(dc_video_output_t pref, int8_t cable, unsigned scale)
{
   int8_t link = effective_cable(pref, cable);

   switch (link)
   {
      case CT_VGA:
         if (scale >= 4)
            return DM_800x608_VGA;
         return DM_640x480_VGA;

      case CT_RGB:
         return DM_640x480;

      case CT_COMPOSITE:
      case CT_NONE:
      default:
         if (scale <= 2)
            return DM_320x240_NTSC;
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

bool dc_video_init_for_scale(dc_video_output_t output, unsigned scale)
{
   int mode;

   if (scale < 1)
      scale = 1;

   mode = pick_display_mode(output, dc_video_get_cable(), scale);
   vid_set_mode(mode, PM_RGB555);
   active_output = output;
   active_scale  = scale;
   video_ready   = true;
   return true;
}

bool dc_video_init(dc_video_output_t output)
{
   return dc_video_init_for_scale(output, 3);
}

void dc_video_shutdown(void)
{
   video_ready = false;
}

bool dc_video_reinit_for_scale(dc_video_output_t output, unsigned scale)
{
   return dc_video_init_for_scale(output, scale);
}

bool dc_video_reinit(dc_video_output_t output)
{
   return dc_video_reinit_for_scale(output, active_scale);
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
   return pick_display_mode(active_output, dc_video_get_cable(), active_scale);
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

static unsigned clamp_scale(unsigned scale, unsigned src_w, unsigned src_h,
      unsigned buf_w, unsigned buf_h)
{
   unsigned draw_w = src_w * scale;
   unsigned draw_h = src_h * scale;

   if (draw_w > buf_w)
   {
      scale = buf_w / src_w;
      if (scale < 1)
         scale = 1;
      draw_w = src_w * scale;
      draw_h = src_h * scale;
   }

   if (draw_h > buf_h)
   {
      scale = buf_h / src_h;
      if (scale < 1)
         scale = 1;
   }

   return scale;
}

static void expand_row(uint16_t *dst, const uint16_t *src, unsigned src_w,
      unsigned scale)
{
   unsigned x;
   unsigned out = 0;

   for (x = 0; x < src_w; x++)
   {
      uint16_t pixel = src[x];
      unsigned sx;

      for (sx = 0; sx < scale; sx++)
         dst[out++] = pixel;
   }
}

dc_video_blitter_t *dc_video_blitter_create(unsigned scale)
{
   dc_video_blitter_t *blitter;
   unsigned scratch_w;

   blitter = (dc_video_blitter_t *)calloc(1, sizeof(*blitter));
   if (!blitter)
      return NULL;

   blitter->buf_w = dc_video_width();
   blitter->buf_h = dc_video_height();
   blitter->scale = scale ? scale : 1;

   blitter->buffer = (uint16_t *)calloc(
         (size_t)blitter->buf_w * blitter->buf_h, sizeof(uint16_t));
   scratch_w = blitter->buf_w;
   blitter->row_scratch = (uint16_t *)malloc(scratch_w * sizeof(uint16_t));

   if (!blitter->buffer || !blitter->row_scratch)
   {
      free(blitter->row_scratch);
      free(blitter->buffer);
      free(blitter);
      return NULL;
   }

   return blitter;
}

void dc_video_blitter_destroy(dc_video_blitter_t *blitter)
{
   if (!blitter)
      return;

   free(blitter->row_scratch);
   free(blitter->buffer);
   free(blitter);
}

bool dc_video_blitter_sync(dc_video_blitter_t *blitter, unsigned scale)
{
   unsigned w;
   unsigned h;
   uint16_t *buffer;
   uint16_t *scratch;

   if (!blitter)
      return false;

   if (scale < 1)
      scale = 1;

   w = dc_video_width();
   h = dc_video_height();

   if (w != blitter->buf_w || h != blitter->buf_h)
   {
      buffer = (uint16_t *)realloc(blitter->buffer,
            (size_t)w * h * sizeof(uint16_t));
      if (!buffer)
         return false;

      scratch = (uint16_t *)realloc(blitter->row_scratch, w * sizeof(uint16_t));
      if (!scratch)
         return false;

      blitter->buffer      = buffer;
      blitter->row_scratch = scratch;
      blitter->buf_w       = w;
      blitter->buf_h       = h;
   }

   blitter->scale = scale;
   return true;
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
   const uint8_t *src8 = (const uint8_t *)src;
   unsigned y, sy;
   unsigned offset_x, offset_y;
   unsigned draw_w, draw_h;
   unsigned scale;

   if (!blitter || !blitter->buffer || !blitter->row_scratch || !src
         || !src_w || !src_h)
      return;

   scale = clamp_scale(blitter->scale, src_w, src_h,
         blitter->buf_w, blitter->buf_h);
   blitter->scale = scale;

   dc_video_blitter_clear(blitter);

   draw_w = src_w * scale;
   draw_h = src_h * scale;
   offset_x = (blitter->buf_w - draw_w) / 2;
   offset_y = (blitter->buf_h - draw_h) / 2;

   for (y = 0; y < src_h; y++)
   {
      const uint16_t *row = (const uint16_t *)(src8 + y * src_pitch);
      uint16_t *dst_base;

      expand_row(blitter->row_scratch, row, src_w, scale);
      dst_base = blitter->buffer + (offset_y + y * scale) * blitter->buf_w
            + offset_x;

      for (sy = 0; sy < scale; sy++)
         memcpy(dst_base + sy * blitter->buf_w,
               blitter->row_scratch, draw_w * sizeof(uint16_t));
   }
}

void dc_video_blitter_present(dc_video_blitter_t *blitter, bool vsync)
{
   if (!blitter)
      return;

   dc_video_present(blitter->buffer, vsync);
}
