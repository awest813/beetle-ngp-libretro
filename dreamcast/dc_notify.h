#ifndef DC_NOTIFY_H
#define DC_NOTIFY_H

#include <stdbool.h>
#include <stdint.h>

void dc_notify_show(const char *text, unsigned frames);
void dc_notify_tick(void);
bool dc_notify_active(void);
void dc_notify_render_argb1555(uint16_t *buf, unsigned width, unsigned height,
      unsigned pitch);
void dc_notify_draw(void);

#endif
