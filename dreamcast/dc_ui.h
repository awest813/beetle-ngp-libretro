/* Shared UI framebuffer for menus (software VRAM or PVR fullscreen) */

#ifndef DC_UI_H
#define DC_UI_H

#include <stdbool.h>
#include <stdint.h>

/* ARGB1555 theme colors */
#define DC_UI_COLOR_BG       0x0000u
#define DC_UI_COLOR_HEADER   0x1084u
#define DC_UI_COLOR_PANEL    0x2104u
#define DC_UI_COLOR_BORDER   0x5AD6u
#define DC_UI_COLOR_ACCENT   0xA108u
#define DC_UI_COLOR_SELECTED 0x294Au
#define DC_UI_COLOR_TEXT_DIM 0
#define DC_UI_COLOR_TEXT     1

typedef struct dc_ui_layout
{
   unsigned width;
   unsigned height;
   int margin_x;
   int header_h;
   int footer_h;
   int content_y;
   int content_h;
   int row_h;
} dc_ui_layout_t;

typedef struct dc_ui_list
{
   int selected;
   int scroll;
   int row_h;
   int content_y;
   int content_h;
} dc_ui_list_t;

void dc_ui_init(void);
void dc_ui_shutdown(void);
void dc_ui_resize(void);

unsigned dc_ui_width(void);
unsigned dc_ui_height(void);

void dc_ui_get_layout(dc_ui_layout_t *layout);

void dc_ui_begin_frame(void);
void dc_ui_fill_rect(int x, int y, int w, int h, uint16_t color);
void dc_ui_draw_text(int x, int y, int color, const char *text);
void dc_ui_draw_header(const char *title, const char *subtitle);
void dc_ui_draw_footer(const char *hint);
void dc_ui_draw_panel(int x, int y, int w, int h);
void dc_ui_draw_separator(int y);
void dc_ui_draw_menu_row(int y, int w, bool selected, const char *label,
      const char *value);
void dc_ui_draw_hint(int y, const char *text);
void dc_ui_draw_badge(int x, int y, const char *text, bool active);
void dc_ui_draw_scrollbar(int x, int y, int h, int total, int visible,
      int scroll);

void dc_ui_list_init(dc_ui_list_t *list, const dc_ui_layout_t *layout);
void dc_ui_list_clamp(dc_ui_list_t *list, int count);
void dc_ui_list_move(dc_ui_list_t *list, int delta, int count);
int dc_ui_list_visible_rows(const dc_ui_list_t *list);

void dc_ui_present(bool vsync);

#endif
