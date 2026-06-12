/* Shared UI framebuffer for menus (software VRAM or PVR fullscreen) */

#ifndef DC_UI_H
#define DC_UI_H

#include <stdbool.h>

void dc_ui_init(void);
void dc_ui_shutdown(void);
void dc_ui_resize(void);

void dc_ui_begin_frame(void);
void dc_ui_draw_text(int x, int y, int color, const char *text);
void dc_ui_present(bool vsync);

#endif
