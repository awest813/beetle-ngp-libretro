/* Shared Dreamcast video backend (KallistiOS / RGB555 VRAM) */

#ifndef DC_VIDEO_H
#define DC_VIDEO_H

#include <kos.h>
#include <dc/video.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum dc_video_output
{
   DC_VIDEO_OUTPUT_AUTO = 0,
   DC_VIDEO_OUTPUT_VGA,
   DC_VIDEO_OUTPUT_TV,
   DC_VIDEO_OUTPUT_COUNT
} dc_video_output_t;

typedef struct dc_video_blitter
{
   uint16_t *buffer;
   unsigned buf_w;
   unsigned buf_h;
   unsigned scale;
} dc_video_blitter_t;

int8_t dc_video_get_cable(void);
const char *dc_video_cable_name(int8_t cable);
const char *dc_video_output_name(dc_video_output_t output);

bool dc_video_init(dc_video_output_t output);
void dc_video_shutdown(void);
bool dc_video_reinit(dc_video_output_t output);

unsigned dc_video_width(void);
unsigned dc_video_height(void);
dc_video_output_t dc_video_active_output(void);
int dc_video_active_mode(void);

void dc_video_clear_screen(void);
void dc_video_present(const uint16_t *pixels, bool vsync);

dc_video_blitter_t *dc_video_blitter_create(unsigned scale);
void dc_video_blitter_destroy(dc_video_blitter_t *blitter);
void dc_video_blitter_set_scale(dc_video_blitter_t *blitter, unsigned scale);
void dc_video_blitter_clear(dc_video_blitter_t *blitter);
void dc_video_blitter_rgb555(dc_video_blitter_t *blitter,
      const void *src, unsigned src_w, unsigned src_h, size_t src_pitch);
void dc_video_blitter_present(dc_video_blitter_t *blitter, bool vsync);

#endif
