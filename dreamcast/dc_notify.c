#include "dc_notify.h"
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

void dc_notify_draw(void)
{
   unsigned pitch;
   int x;
   int y;

   if (!notify_frames || !notify_text[0])
      return;

   pitch = dc_video_width();
   x     = 16;
   y     = dc_video_height() - 32;

   if (y < 0)
      y = 8;

   bfont_draw_str(vram_s + y * pitch + x, pitch, 1, notify_text);
}
