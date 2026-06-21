#include "dc_ui.h"
#include "dc_pvr.h"
#include "dc_video.h"

#include <kos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t *ui_buffer;
static unsigned ui_buf_w;
static unsigned ui_buf_h;

static void clamp_rect(int *x, int *y, int *w, int *h)
{
   if (*x < 0)
   {
      *w += *x;
      *x = 0;
   }
   if (*y < 0)
   {
      *h += *y;
      *y = 0;
   }
   if (*x + *w > (int)ui_buf_w)
      *w = (int)ui_buf_w - *x;
   if (*y + *h > (int)ui_buf_h)
      *h = (int)ui_buf_h - *y;
   if (*w < 0)
      *w = 0;
   if (*h < 0)
      *h = 0;
}

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

unsigned dc_ui_width(void)
{
   dc_ui_resize();
   return ui_buf_w;
}

unsigned dc_ui_height(void)
{
   dc_ui_resize();
   return ui_buf_h;
}

void dc_ui_get_layout(dc_ui_layout_t *layout)
{
   unsigned h;

   if (!layout)
      return;

   dc_ui_resize();

   h = ui_buf_h ? ui_buf_h : 240;

   layout->width    = ui_buf_w ? ui_buf_w : 320;
   layout->height   = h;
   layout->margin_x = (layout->width >= 640) ? 32 : 16;
   layout->header_h = (h >= 400) ? 56 : 44;
   layout->footer_h = (h >= 400) ? 44 : 32;
   layout->row_h    = (h >= 400) ? 28 : 22;
   layout->content_y = layout->header_h + 4;
   layout->content_h = (int)h - layout->header_h - layout->footer_h - 8;
   if (layout->content_h < layout->row_h * 2)
      layout->content_h = layout->row_h * 2;
}

void dc_ui_begin_frame(void)
{
   dc_ui_resize();

   if (ui_buffer && ui_buf_w && ui_buf_h)
      memset(ui_buffer, 0, (size_t)ui_buf_w * ui_buf_h * sizeof(uint16_t));

   dc_ui_fill_rect(0, 0, (int)ui_buf_w, (int)ui_buf_h, DC_UI_COLOR_BG);
}

void dc_ui_fill_rect(int x, int y, int w, int h, uint16_t color)
{
   int yy;

   if (!ui_buffer || !ui_buf_w || !ui_buf_h)
      return;

   clamp_rect(&x, &y, &w, &h);

   if (w <= 0 || h <= 0)
      return;

   for (yy = y; yy < y + h; yy++)
   {
      uint16_t *row = ui_buffer + yy * ui_buf_w;
      int xx;

      /* Fast path: single-pixel row fill with memset-like loop */
      for (xx = x; xx < x + w; xx++)
         row[xx] = color;
   }
}

void dc_ui_draw_text(int x, int y, int color, const char *text)
{
   int max_chars;
   char clip[64];
   size_t len;

   if (!ui_buffer || !text || x < 0 || y < 0)
      return;

   if ((unsigned)y >= ui_buf_h)
      return;

   if ((unsigned)x >= ui_buf_w)
      return;

   /* Clip text that would overflow the right edge */
   max_chars = ((int)ui_buf_w - x) / 8;
   if (max_chars < 1)
      return;

   len = strlen(text);
   if ((int)len > max_chars)
      len = (size_t)max_chars;

   /* Use stack buffer to avoid modifying the caller's string */
   if (len < sizeof(clip))
   {
      memcpy(clip, text, len);
      clip[len] = '\0';
      bfont_draw_str(ui_buffer + y * ui_buf_w + x, ui_buf_w, color, clip);
   }
   else
   {
      bfont_draw_str(ui_buffer + y * ui_buf_w + x, ui_buf_w, color, text);
   }
}

void dc_ui_draw_header(const char *title, const char *subtitle)
{
   dc_ui_layout_t layout;
   int x;
   int y;

   dc_ui_get_layout(&layout);
   x = layout.margin_x;
   y = 8;

   dc_ui_fill_rect(0, 0, (int)layout.width, layout.header_h, DC_UI_COLOR_HEADER);
   dc_ui_fill_rect(0, layout.header_h - 2, (int)layout.width, 2, DC_UI_COLOR_ACCENT);

   if (title)
      dc_ui_draw_text(x, y, DC_UI_COLOR_TEXT, title);

   if (subtitle)
   {
      y += (layout.height >= 400) ? 24 : 18;
      dc_ui_draw_text(x, y, DC_UI_COLOR_TEXT, subtitle);
   }
}

void dc_ui_draw_footer(const char *hint)
{
   dc_ui_layout_t layout;
   int y;

   if (!hint)
      return;

   dc_ui_get_layout(&layout);
   y = (int)layout.height - layout.footer_h;

   dc_ui_fill_rect(0, y, (int)layout.width, layout.footer_h, DC_UI_COLOR_HEADER);
   dc_ui_fill_rect(0, y, (int)layout.width, 1, DC_UI_COLOR_BORDER);
   dc_ui_draw_text(layout.margin_x, y + 8, DC_UI_COLOR_TEXT, hint);
}

void dc_ui_draw_panel(int x, int y, int w, int h)
{
   dc_ui_fill_rect(x, y, w, h, DC_UI_COLOR_PANEL);
   dc_ui_fill_rect(x, y, w, 1, DC_UI_COLOR_BORDER);
   dc_ui_fill_rect(x, y + h - 1, w, 1, DC_UI_COLOR_BORDER);
   dc_ui_fill_rect(x, y, 1, h, DC_UI_COLOR_BORDER);
   dc_ui_fill_rect(x + w - 1, y, 1, h, DC_UI_COLOR_BORDER);
}

void dc_ui_draw_separator(int y)
{
   dc_ui_layout_t layout;

   dc_ui_get_layout(&layout);
   dc_ui_fill_rect(layout.margin_x, y, (int)layout.width - layout.margin_x * 2,
         1, DC_UI_COLOR_BORDER);
}

void dc_ui_draw_menu_row(int y, int w, bool selected, const char *label,
      const char *value)
{
   dc_ui_layout_t layout;
   char line[96];
   int x;

   dc_ui_get_layout(&layout);
   x = layout.margin_x;

   if (selected)
      dc_ui_fill_rect(x - 4, y - 2, w + 8, layout.row_h, DC_UI_COLOR_SELECTED);

   if (value && value[0])
   {
      snprintf(line, sizeof(line), "%s%s", selected ? "> " : "  ", label);
      dc_ui_draw_text(x, y, DC_UI_COLOR_TEXT, line);
      dc_ui_draw_text(x + w - (int)strlen(value) * 8 - 8, y,
            DC_UI_COLOR_TEXT, value);
   }
   else
   {
      snprintf(line, sizeof(line), "%s%s", selected ? "> " : "  ", label);
      dc_ui_draw_text(x, y, DC_UI_COLOR_TEXT, line);
   }
}

void dc_ui_draw_hint(int y, const char *text)
{
   dc_ui_layout_t layout;

   if (!text || !text[0])
      return;

   dc_ui_get_layout(&layout);
   dc_ui_draw_panel(layout.margin_x, y,
         (int)layout.width - layout.margin_x * 2, layout.row_h + 6);
   dc_ui_draw_text(layout.margin_x + 8, y + 4, DC_UI_COLOR_TEXT, text);
}

void dc_ui_draw_badge(int x, int y, const char *text, bool active)
{
   int w;

   if (!text || !text[0])
      return;

   w = (int)strlen(text) * 8 + 8;

   if (active)
   {
      dc_ui_fill_rect(x, y, w, 14, DC_UI_COLOR_ACCENT);
      dc_ui_draw_text(x + 4, y + 2, DC_UI_COLOR_TEXT, text);
   }
   else
   {
      dc_ui_fill_rect(x, y, w, 14, DC_UI_COLOR_PANEL);
      dc_ui_fill_rect(x, y, w, 1, DC_UI_COLOR_BORDER);
      dc_ui_fill_rect(x, y + 13, w, 1, DC_UI_COLOR_BORDER);
      dc_ui_fill_rect(x, y, 1, 14, DC_UI_COLOR_BORDER);
      dc_ui_fill_rect(x + w - 1, y, 1, 14, DC_UI_COLOR_BORDER);
      dc_ui_draw_text(x + 4, y + 2, DC_UI_COLOR_TEXT_DIM, text);
   }
}

void dc_ui_draw_scrollbar(int x, int y, int h, int total, int visible,
      int scroll)
{
   int track_h;
   int thumb_h;
   int thumb_y;

   if (total <= visible || visible <= 0 || h < 8)
      return;

   track_h = h - 4;
   thumb_h = (track_h * visible) / total;
   if (thumb_h < 6)
      thumb_h = 6;
   if (thumb_h > track_h)
      thumb_h = track_h;

   thumb_y = y + 2;
   if (total > visible)
      thumb_y += (scroll * (track_h - thumb_h)) / (total - visible);

   dc_ui_fill_rect(x, y, 6, h, DC_UI_COLOR_PANEL);
   dc_ui_fill_rect(x + 1, thumb_y, 4, thumb_h, DC_UI_COLOR_ACCENT);
}

void dc_ui_list_init(dc_ui_list_t *list, const dc_ui_layout_t *layout)
{
   if (!list || !layout)
      return;

   list->selected   = 0;
   list->scroll     = 0;
   list->row_h      = layout->row_h;
   list->content_y  = layout->content_y;
   list->content_h  = layout->content_h;
}

void dc_ui_list_clamp(dc_ui_list_t *list, int count)
{
   int visible;

   if (!list || count <= 0)
      return;

   if (list->selected < 0)
      list->selected = 0;
   if (list->selected >= count)
      list->selected = count - 1;

   visible = dc_ui_list_visible_rows(list);
   if (list->selected < list->scroll)
      list->scroll = list->selected;
   if (list->selected >= list->scroll + visible)
      list->scroll = list->selected - visible + 1;

   if (list->scroll < 0)
      list->scroll = 0;
   if (list->scroll > count - visible)
      list->scroll = count - visible;
   if (list->scroll < 0)
      list->scroll = 0;
}

void dc_ui_list_move(dc_ui_list_t *list, int delta, int count)
{
   if (!list || count <= 0 || delta == 0)
      return;

   list->selected += delta;
   dc_ui_list_clamp(list, count);
}

int dc_ui_list_visible_rows(const dc_ui_list_t *list)
{
   if (!list || list->row_h <= 0)
      return 1;

   return list->content_h / list->row_h;
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
