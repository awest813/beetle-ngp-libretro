/* Dreamcast PowerVR hardware frame presentation */

#ifndef DC_PVR_H
#define DC_PVR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool dc_pvr_init(void);
void dc_pvr_shutdown(void);
bool dc_pvr_is_ready(void);

void dc_pvr_present(const uint16_t *src, unsigned src_w, unsigned src_h,
      size_t src_pitch, unsigned scale, bool vsync);

void dc_pvr_present_ui(const uint16_t *src, unsigned width, unsigned height,
      bool vsync);

void dc_pvr_menu_set_visible(bool visible);
void dc_pvr_menu_set_frame(const uint16_t *src, unsigned width, unsigned height);

#endif
