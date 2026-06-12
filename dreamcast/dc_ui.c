#include "dc_ui.h"
#include "dc_pvr.h"
#include "dc_video.h"

#include <kos.h>

#include <stdlib.h>
#include <string.h>

static uint16_t *ui_buffer;
static unsigned ui_buf_w;
static unsigned ui_buf_h;

void dc_ui_init(void)
{
   ui_buffer = NULL;
   ui_buf_w  = 0;
   ui_buf_h  = 0;
}

void dc_ui_shutdown(void)
{
   free(ui_buffer);
   ui_buffer = NULL;
   ui_buf_w  = 0;
   ui_buf_h  = 0;
}

void dc_ui_resize(void)
{
   unsigned w = dc_video_width();
   unsigned h = dc_video_height();

   if (ui_buffer && w == ui_buf_w && h == ui_buf_h)
      return;

   free(ui_buffer);
   ui_buffer = (uint16_t *)memalign(32, (size_t)w * h * sizeof(uint16_t));
   ui_buf_w  = w;
   ui_buf_h  = h;
}

void dc_ui_begin_frame(void)
{
   dc_ui_resize();

   if (ui_buffer && ui_buf_w && ui_buf_h)
      memset(ui_buffer, 0, (size_t)ui_buf_w * ui_buf_h * sizeof(uint16_t));
}

void dc_ui_draw_text(int x, int y, int color, const char *text)
{
   if (!ui_buffer || !text || x < 0 || y < 0)
      return;

   bfont_draw_str(ui_buffer + y * ui_buf_w + x, ui_buf_w, color, text);
}

void dc_ui_present(bool vsync)
{
   if (!ui_buffer || !ui_buf_w || !ui_buf_h)
      return;

   if (dc_video_get_renderer() == DC_VIDEO_RENDERER_PVR && dc_pvr_is_ready())
      dc_pvr_present_ui(ui_buffer, ui_buf_w, ui_buf_h, vsync);
   else
      dc_video_present(ui_buffer, vsync);
}
