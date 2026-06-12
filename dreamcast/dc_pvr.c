/* Dreamcast PowerVR hardware frame presentation */

#include "dc_pvr.h"
#include "dc_notify.h"
#include "dc_video.h"

#include <dc/pvr.h>

#include <stdlib.h>
#include <string.h>

#define DC_PVR_TEX_W        160
#define DC_PVR_TEX_H        152
#define DC_PVR_TEX_POW_W    256
#define DC_PVR_TEX_POW_H    256
#define DC_PVR_TEX_BYTES    (DC_PVR_TEX_W * DC_PVR_TEX_H * 2)
#define DC_PVR_BUF_COUNT    2

#define DC_NOTIFY_TEX_W     256
#define DC_NOTIFY_TEX_H     32
#define DC_NOTIFY_TEX_BYTES (DC_NOTIFY_TEX_W * DC_NOTIFY_TEX_H * 2)

#define DC_PVR_TEX_FMT \
   (PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_X32_STRIDE)

#define DC_NOTIFY_TEX_FMT (PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED)

static pvr_ptr_t pvr_texture[DC_PVR_BUF_COUNT];
static pvr_poly_hdr_t pvr_hdr[DC_PVR_BUF_COUNT];
static uint16_t *pvr_staging[DC_PVR_BUF_COUNT];
static unsigned pvr_front_idx;
static bool pvr_has_frame;

static pvr_ptr_t notify_texture;
static pvr_poly_hdr_t notify_hdr;
static uint16_t *notify_staging;
static bool notify_hdr_ready;
static bool pvr_ready;

static unsigned pvr_clamp_scale(unsigned scale, unsigned src_w, unsigned src_h)
{
   unsigned draw_w = src_w * scale;
   unsigned draw_h = src_h * scale;

   if (draw_w > dc_video_width())
   {
      scale = dc_video_width() / src_w;
      if (scale < 1)
         scale = 1;
   }

   if (src_h * scale > dc_video_height())
   {
      unsigned hscale = dc_video_height() / src_h;

      if (hscale < scale)
         scale = hscale;
      if (scale < 1)
         scale = 1;
   }

   return scale;
}

static void pvr_copy_frame(const uint16_t *src, unsigned src_w, unsigned src_h,
      size_t src_pitch, uint16_t *dst)
{
   unsigned y;

   for (y = 0; y < src_h; y++)
   {
      const uint16_t *row = (const uint16_t *)((const uint8_t *)src + y * src_pitch);

      memcpy(dst + y * src_w, row, (size_t)src_w * sizeof(uint16_t));
   }
}

static void pvr_upload_texture(const void *src, pvr_ptr_t dst, size_t bytes,
      unsigned stride_w)
{
   pvr_txr_load_dma((void *)src, dst, (uint32_t)bytes, 1, NULL, 0);

   if (stride_w)
      pvr_txr_set_stride(stride_w);
}

static void pvr_draw_textured_quad(const pvr_poly_hdr_t *hdr,
      unsigned tex_w, unsigned tex_h, unsigned tex_pow_w, unsigned tex_pow_h,
      unsigned scale, bool use_stride)
{
   pvr_vertex_t vert;
   unsigned draw_w = tex_w * scale;
   unsigned draw_h = tex_h * scale;
   float ox = (float)(dc_video_width() - draw_w) / 2.0f;
   float oy = (float)(dc_video_height() - draw_h) / 2.0f;
   float umax = (float)tex_w / (float)tex_pow_w;
   float vmax = (float)tex_h / (float)tex_pow_h;
   int color = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);

   if (use_stride)
      pvr_txr_set_stride(tex_w);

   pvr_prim(hdr, sizeof(*hdr));

   vert.flags = PVR_CMD_VERTEX;
   vert.x = ox;
   vert.y = oy;
   vert.z = 1.0f;
   vert.u = 0.0f;
   vert.v = 0.0f;
   vert.argb = color;
   vert.oargb = 0;
   pvr_prim(&vert, sizeof(vert));

   vert.x = ox + (float)draw_w;
   vert.y = oy;
   vert.u = umax;
   vert.v = 0.0f;
   pvr_prim(&vert, sizeof(vert));

   vert.x = ox;
   vert.y = oy + (float)draw_h;
   vert.u = 0.0f;
   vert.v = vmax;
   pvr_prim(&vert, sizeof(vert));

   vert.flags = PVR_CMD_VERTEX_EOL;
   vert.x = ox + (float)draw_w;
   vert.y = oy + (float)draw_h;
   vert.u = umax;
   vert.v = vmax;
   pvr_prim(&vert, sizeof(vert));
}

static void pvr_draw_notify_quad(void)
{
   pvr_vertex_t vert;
   float draw_w = (float)DC_NOTIFY_TEX_W;
   float draw_h = (float)DC_NOTIFY_TEX_H;
   float ox = (float)(dc_video_width() - (unsigned)draw_w) / 2.0f;
   float oy = (float)dc_video_height() - draw_h - 8.0f;
   int color = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);

   if (oy < 0.0f)
      oy = 0.0f;

   pvr_prim(&notify_hdr, sizeof(notify_hdr));

   vert.flags = PVR_CMD_VERTEX;
   vert.x = ox;
   vert.y = oy;
   vert.z = 1.0f;
   vert.u = 0.0f;
   vert.v = 0.0f;
   vert.argb = color;
   vert.oargb = 0;
   pvr_prim(&vert, sizeof(vert));

   vert.x = ox + draw_w;
   vert.y = oy;
   vert.u = 1.0f;
   vert.v = 0.0f;
   pvr_prim(&vert, sizeof(vert));

   vert.x = ox;
   vert.y = oy + draw_h;
   vert.u = 0.0f;
   vert.v = 1.0f;
   pvr_prim(&vert, sizeof(vert));

   vert.flags = PVR_CMD_VERTEX_EOL;
   vert.x = ox + draw_w;
   vert.y = oy + draw_h;
   vert.u = 1.0f;
   vert.v = 1.0f;
   pvr_prim(&vert, sizeof(vert));
}

static void pvr_prepare_notify_hdr(void)
{
   pvr_poly_cxt_t cxt;

   if (notify_hdr_ready)
      return;

   pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, DC_NOTIFY_TEX_FMT,
         DC_NOTIFY_TEX_W, DC_NOTIFY_TEX_H, notify_texture, PVR_FILTER_NONE);
   cxt.gen.alpha = PVR_ALPHA_ENABLE;
   cxt.blend.src = PVR_BLEND_SRCALPHA;
   cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
   cxt.txr.alpha = PVR_TXRALPHA_ENABLE;
   pvr_poly_compile(&notify_hdr, &cxt);
   notify_hdr_ready = true;
}

static void pvr_draw_notify(void)
{
   if (!dc_notify_active() || !notify_texture || !notify_staging)
      return;

   dc_notify_render_argb1555(notify_staging, DC_NOTIFY_TEX_W, DC_NOTIFY_TEX_H,
         DC_NOTIFY_TEX_W);
   pvr_upload_texture(notify_staging, notify_texture, DC_NOTIFY_TEX_BYTES, 0);
   pvr_prepare_notify_hdr();

   pvr_list_begin(PVR_LIST_TR_POLY);
   pvr_draw_notify_quad();
   pvr_list_finish();
}

static void pvr_compile_hdr(unsigned idx)
{
   pvr_poly_cxt_t cxt;

   pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, DC_PVR_TEX_FMT,
         DC_PVR_TEX_POW_W, DC_PVR_TEX_POW_H, pvr_texture[idx], PVR_FILTER_NONE);
   pvr_poly_compile(&pvr_hdr[idx], &cxt);
}

static void pvr_free_buffers(void)
{
   unsigned i;

   for (i = 0; i < DC_PVR_BUF_COUNT; i++)
   {
      free(pvr_staging[i]);
      pvr_staging[i] = NULL;

      if (pvr_texture[i])
      {
         pvr_mem_free(pvr_texture[i]);
         pvr_texture[i] = NULL;
      }
   }

   free(notify_staging);
   notify_staging = NULL;

   if (notify_texture)
   {
      pvr_mem_free(notify_texture);
      notify_texture = NULL;
   }

   notify_hdr_ready = false;
   pvr_front_idx = 0;
   pvr_has_frame = false;
}

bool dc_pvr_init(void)
{
   unsigned i;

   if (pvr_ready)
      return true;

   if (pvr_init_defaults() < 0)
      return false;

   pvr_set_bg_color(0.0f, 0.0f, 0.0f);

   for (i = 0; i < DC_PVR_BUF_COUNT; i++)
   {
      pvr_texture[i] = pvr_mem_malloc(DC_PVR_TEX_BYTES);
      pvr_staging[i] = (uint16_t *)memalign(32, DC_PVR_TEX_BYTES);

      if (!pvr_texture[i] || !pvr_staging[i])
      {
         pvr_free_buffers();
         pvr_shutdown();
         return false;
      }

      memset(pvr_staging[i], 0, DC_PVR_TEX_BYTES);
      pvr_compile_hdr(i);
   }

   notify_texture = pvr_mem_malloc(DC_NOTIFY_TEX_BYTES);
   notify_staging = (uint16_t *)memalign(32, DC_NOTIFY_TEX_BYTES);

   if (!notify_texture || !notify_staging)
   {
      pvr_free_buffers();
      pvr_shutdown();
      return false;
   }

   memset(notify_staging, 0, DC_NOTIFY_TEX_BYTES);

   pvr_ready = true;
   return true;
}

void dc_pvr_shutdown(void)
{
   if (!pvr_ready)
      return;

   pvr_free_buffers();
   pvr_shutdown();
   pvr_ready = false;
}

bool dc_pvr_is_ready(void)
{
   return pvr_ready;
}

void dc_pvr_present(const uint16_t *src, unsigned src_w, unsigned src_h,
      size_t src_pitch, unsigned scale, bool vsync)
{
   unsigned back_idx;

   if (!pvr_ready)
      return;

   if (!src)
   {
      if (!pvr_has_frame)
         return;
   }
   else
   {
      if (!src_w || !src_h)
         return;

      if (src_w != DC_PVR_TEX_W || src_h != DC_PVR_TEX_H)
         return;

      back_idx = 1u - pvr_front_idx;
      pvr_copy_frame(src, src_w, src_h, src_pitch, pvr_staging[back_idx]);
      pvr_upload_texture(pvr_staging[back_idx], pvr_texture[back_idx],
            DC_PVR_TEX_BYTES, DC_PVR_TEX_W);
      pvr_front_idx = back_idx;
      pvr_has_frame = true;
   }

   scale = pvr_clamp_scale(scale ? scale : 1, DC_PVR_TEX_W, DC_PVR_TEX_H);

   pvr_scene_begin();
   pvr_list_begin(PVR_LIST_OP_POLY);
   pvr_draw_textured_quad(&pvr_hdr[pvr_front_idx], DC_PVR_TEX_W, DC_PVR_TEX_H,
         DC_PVR_TEX_POW_W, DC_PVR_TEX_POW_H, scale, true);
   pvr_list_finish();

   pvr_draw_notify();
   pvr_scene_finish();

   if (vsync)
      vid_waitvbl();
}
