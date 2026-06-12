#include "dc_notify.h"
#include "dc_video.h"

#include <kos.h>

#include <stdio.h>
#include <string.h>

#define DC_NOTIFY_BAR_COLOR 0xA108u

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
   unsigned y;
   unsigned bar_y0 = 4;
   unsigned bar_y1 = height > 8 ? height - 4 : height;
   int x;
   int text_y;

   if (!dc_notify_active() || !buf || !width || !height || !pitch)
      return;

   memset(buf, 0, (size_t)pitch * height * sizeof(uint16_t));

   for (y = bar_y0; y < bar_y1; y++)
   {
      uint16_t *row = buf + y * pitch;

      for (x = 0; x < (int)width; x++)
         row[x] = DC_NOTIFY_BAR_COLOR;
   }

   x      = 8;
   text_y = 8;
   bfont_draw_str(buf + text_y * pitch + x, pitch, 1, notify_text);
}

void dc_notify_draw(void)
{
   unsigned pitch;
   int x;
   int y;

   if (!dc_notify_active())
      return;

   pitch = dc_video_width();
   x     = 16;
   y     = (int)dc_video_height() - 32;

   if (y < 0)
      y = 8;

   bfont_draw_str(vram_s + y * pitch + x, pitch, 1, notify_text);
}
