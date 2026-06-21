#include "dc_notify.h"
#include "dc_ui.h"
#include "dc_video.h"

#include <kos.h>

#include <stdio.h>
#include <string.h>

static char notify_text[80];
static unsigned notify_frames;

void dc_notify_show(const char *text, unsigned frames)
{
   if (!text)
      return;

   strncpy(notify_text, text, sizeof(notify_text) - 1);
   notify_text[sizeof(notify_text) - 1] = '\0';
   notify_frames = frames ? frames : 90;
}

void dc_notify_tick(void)
{
   if (notify_frames > 0)
      notify_frames--;
}

bool dc_notify_active(void)
{
   return notify_frames > 0 && notify_text[0] != '\0';
}

void dc_notify_render_argb1555(uint16_t *buf, unsigned width, unsigned height,
      unsigned pitch)
{
   unsigned bar_h = 24;
   unsigned y;
   int x;
   int text_y;

   if (!dc_notify_active() || !buf || !width || !height || !pitch)
      return;

   if (bar_h > height)
      bar_h = height;

   for (y = 0; y < bar_h; y++)
   {
      uint16_t *row = buf + y * pitch;
      int xx;

      for (xx = 0; xx < (int)width; xx++)
         row[xx] = DC_UI_COLOR_ACCENT;
   }

   x      = 8;
   text_y = 4;
   bfont_draw_str(buf + text_y * pitch + x, pitch, DC_UI_COLOR_TEXT, notify_text);
}

void dc_notify_draw(void)
{
   unsigned width;
   unsigned bar_h = 24;
   int x;
   int y;

   if (!dc_notify_active())
      return;

   width = dc_video_width();
   x     = 8;
   y     = (int)dc_video_height() - (int)bar_h - 8;

   if (y < 0)
      y = 8;

   for (unsigned row = 0; row < bar_h; row++)
   {
      uint16_t *dst = vram_s + (y + (int)row) * width;
      int xx;

      for (xx = 0; xx < (int)width; xx++)
         dst[xx] = DC_UI_COLOR_ACCENT;
   }

   bfont_draw_str(vram_s + y * width + x, width, DC_UI_COLOR_TEXT, notify_text);
}
