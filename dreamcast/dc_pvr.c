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

static pvr_ptr_t ui_texture;
static pvr_poly_hdr_t ui_hdr;
static unsigned ui_tex_w;
static unsigned ui_tex_h;
static unsigned ui_tex_pow_w;
static unsigned ui_tex_pow_h;
static bool ui_hdr_ready;

static pvr_ptr_t menu_texture;
static pvr_poly_hdr_t menu_hdr;
static uint16_t *menu_staging;
static unsigned menu_tex_w;
static unsigned menu_tex_h;
static unsigned menu_tex_pow_w;
static unsigned menu_tex_pow_h;
static bool menu_hdr_ready;
static bool menu_visible;
static bool menu_has_frame;

static bool pvr_ready;
static bool pvr_dma_inflight;
static unsigned pvr_dma_idx;

static unsigned pvr_pow2(unsigned value)
{
   unsigned power = 1;

   while (power < value)
      power <<= 1;

   return power;
}

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

static void pvr_dma_wait(void)
{
   if (!pvr_dma_inflight)
      return;

   while (!pvr_dma_ready())
      ;

   pvr_front_idx   = pvr_dma_idx;
   pvr_dma_inflight = false;
}

static void pvr_upload_texture_async(void *src, pvr_ptr_t dst, size_t bytes,
      unsigned stride_w, unsigned idx)
{
   pvr_dma_wait();

   pvr_txr_load_dma(src, dst, (uint32_t)bytes, 0, NULL, 0);
   pvr_dma_inflight = true;
   pvr_dma_idx      = idx;

   if (stride_w)
      pvr_txr_set_stride(stride_w);
}

static void pvr_upload_texture_blocking(const void *src, pvr_ptr_t dst,
      size_t bytes, unsigned stride_w)
{
   pvr_txr_load_dma((void *)src, dst, (uint32_t)bytes, 1, NULL, 0);

   if (stride_w)
      pvr_txr_set_stride(stride_w);
}

static void pvr_draw_textured_quad(const pvr_poly_hdr_t *hdr,
      unsigned tex_w, unsigned tex_h, unsigned tex_pow_w, unsigned tex_pow_h,
      float ox, float oy, float draw_w, float draw_h, bool use_stride)
{
   pvr_vertex_t vert;
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

   vert.x = ox + draw_w;
   vert.y = oy;
   vert.u = umax;
   vert.v = 0.0f;
   pvr_prim(&vert, sizeof(vert));

   vert.x = ox;
   vert.y = oy + draw_h;
   vert.u = 0.0f;
   vert.v = vmax;
   pvr_prim(&vert, sizeof(vert));

   vert.flags = PVR_CMD_VERTEX_EOL;
   vert.x = ox + draw_w;
   vert.y = oy + draw_h;
   vert.u = umax;
   vert.v = vmax;
   pvr_prim(&vert, sizeof(vert));
}

static void pvr_draw_scaled_quad(const pvr_poly_hdr_t *hdr,
      unsigned tex_w, unsigned tex_h, unsigned tex_pow_w, unsigned tex_pow_h,
      unsigned scale, bool use_stride)
{
   unsigned draw_w = tex_w * scale;
   unsigned draw_h = tex_h * scale;
   float ox = (float)(dc_video_width() - draw_w) / 2.0f;
   float oy = (float)(dc_video_height() - draw_h) / 2.0f;

   pvr_draw_textured_quad(hdr, tex_w, tex_h, tex_pow_w, tex_pow_h,
         ox, oy, (float)draw_w, (float)draw_h, use_stride);
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
   pvr_upload_texture_blocking(notify_staging, notify_texture,
         DC_NOTIFY_TEX_BYTES, 0);
   pvr_prepare_notify_hdr();

   pvr_list_begin(PVR_LIST_TR_POLY);
   pvr_draw_notify_quad();
   pvr_list_finish();
}

static void pvr_prepare_menu_hdr(void)
{
   pvr_poly_cxt_t cxt;

   if (menu_hdr_ready && menu_texture)
      return;

   pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, DC_PVR_TEX_FMT,
         menu_tex_pow_w, menu_tex_pow_h, menu_texture, PVR_FILTER_NONE);
   cxt.gen.alpha = PVR_ALPHA_ENABLE;
   cxt.blend.src = PVR_BLEND_SRCALPHA;
   cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
   cxt.txr.alpha = PVR_TXRALPHA_ENABLE;
   pvr_poly_compile(&menu_hdr, &cxt);
   menu_hdr_ready = true;
}

static bool pvr_ensure_menu_texture(unsigned width, unsigned height)
{
   size_t bytes;

   if (!width || !height)
      return false;

   if (menu_texture && menu_tex_w == width && menu_tex_h == height)
      return true;

   pvr_free_menu_texture();

   menu_tex_w     = width;
   menu_tex_h     = height;
   menu_tex_pow_w = pvr_pow2(width);
   menu_tex_pow_h = pvr_pow2(height);
   bytes          = (size_t)width * height * sizeof(uint16_t);

   menu_texture = pvr_mem_malloc(bytes);
   menu_staging = (uint16_t *)memalign(32, bytes);

   return menu_texture && menu_staging;
}

static void pvr_draw_menu(void)
{
   size_t bytes;

   if (!menu_visible || !menu_has_frame || !menu_texture || !menu_staging)
      return;

   bytes = (size_t)menu_tex_w * menu_tex_h * sizeof(uint16_t);
   pvr_upload_texture_blocking(menu_staging, menu_texture, bytes, menu_tex_w);
   pvr_prepare_menu_hdr();

   pvr_list_begin(PVR_LIST_TR_POLY);
   pvr_draw_textured_quad(&menu_hdr, menu_tex_w, menu_tex_h,
         menu_tex_pow_w, menu_tex_pow_h,
         0.0f, 0.0f, (float)dc_video_width(), (float)dc_video_height(), true);
   pvr_list_finish();
}

static void pvr_compile_hdr(unsigned idx)
{
   pvr_poly_cxt_t cxt;

   pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, DC_PVR_TEX_FMT,
         DC_PVR_TEX_POW_W, DC_PVR_TEX_POW_H, pvr_texture[idx], PVR_FILTER_NONE);
   pvr_poly_compile(&pvr_hdr[idx], &cxt);
}

static void pvr_free_ui_texture(void)
{
   if (ui_texture)
   {
      pvr_mem_free(ui_texture);
      ui_texture = NULL;
   }

   ui_tex_w = 0;
   ui_tex_h = 0;
   ui_tex_pow_w = 0;
   ui_tex_pow_h = 0;
   ui_hdr_ready = false;
}

static void pvr_free_menu_texture(void)
{
   free(menu_staging);
   menu_staging = NULL;

   if (menu_texture)
   {
      pvr_mem_free(menu_texture);
      menu_texture = NULL;
   }

   menu_tex_w = 0;
   menu_tex_h = 0;
   menu_tex_pow_w = 0;
   menu_tex_pow_h = 0;
   menu_hdr_ready = false;
   menu_has_frame = false;
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

   pvr_free_ui_texture();
   pvr_free_menu_texture();
   menu_visible = false;

   notify_hdr_ready = false;
   pvr_front_idx = 0;
   pvr_has_frame = false;
   pvr_dma_inflight = false;
   pvr_dma_idx = 0;
}

static void pvr_prepare_ui_hdr(unsigned width, unsigned height)
{
   pvr_poly_cxt_t cxt;

   if (ui_hdr_ready && ui_tex_w == width && ui_tex_h == height)
      return;

   pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, DC_PVR_TEX_FMT,
         ui_tex_pow_w, ui_tex_pow_h, ui_texture, PVR_FILTER_NONE);
   pvr_poly_compile(&ui_hdr, &cxt);
   ui_hdr_ready = true;
}

static bool pvr_ensure_ui_texture(unsigned width, unsigned height)
{
   size_t bytes;

   if (!width || !height)
      return false;

   if (ui_texture && ui_tex_w == width && ui_tex_h == height)
      return true;

   pvr_free_ui_texture();

   ui_tex_w     = width;
   ui_tex_h     = height;
   ui_tex_pow_w = pvr_pow2(width);
   ui_tex_pow_h = pvr_pow2(height);
   bytes        = (size_t)width * height * sizeof(uint16_t);
   ui_texture   = pvr_mem_malloc(bytes);

   return ui_texture != NULL;
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

   pvr_dma_wait();
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
      if (!pvr_has_frame && !(menu_visible && menu_has_frame) && !dc_notify_active())
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

      if (!pvr_has_frame
            || memcmp(pvr_staging[back_idx], pvr_staging[pvr_front_idx],
                  DC_PVR_TEX_BYTES) != 0)
      {
         pvr_upload_texture_async(pvr_staging[back_idx], pvr_texture[back_idx],
               DC_PVR_TEX_BYTES, DC_PVR_TEX_W, back_idx);
         pvr_has_frame = true;
      }
   }

   if (pvr_dma_inflight && pvr_dma_ready())
   {
      pvr_front_idx    = pvr_dma_idx;
      pvr_dma_inflight = false;
   }

   scale = pvr_clamp_scale(scale ? scale : 1, DC_PVR_TEX_W, DC_PVR_TEX_H);

   pvr_scene_begin();

   if (pvr_has_frame)
   {
      pvr_list_begin(PVR_LIST_OP_POLY);
      pvr_draw_scaled_quad(&pvr_hdr[pvr_front_idx], DC_PVR_TEX_W, DC_PVR_TEX_H,
            DC_PVR_TEX_POW_W, DC_PVR_TEX_POW_H, scale, true);
      pvr_list_finish();
   }

   pvr_draw_menu();
   pvr_draw_notify();
   pvr_scene_finish();

   if (vsync)
      vid_waitvbl();
}

void dc_pvr_present_ui(const uint16_t *src, unsigned width, unsigned height,
      bool vsync)
{
   size_t bytes;

   if (!pvr_ready || !src || !width || !height)
      return;

   if (!pvr_ensure_ui_texture(width, height))
      return;

   bytes = (size_t)width * height * sizeof(uint16_t);
   pvr_upload_texture_blocking(src, ui_texture, bytes, width);
   pvr_prepare_ui_hdr(width, height);

   pvr_scene_begin();
   pvr_list_begin(PVR_LIST_OP_POLY);
   pvr_draw_textured_quad(&ui_hdr, width, height, ui_tex_pow_w, ui_tex_pow_h,
         0.0f, 0.0f, (float)width, (float)height, true);
   pvr_list_finish();
   pvr_scene_finish();

   if (vsync)
      vid_waitvbl();
}

void dc_pvr_menu_set_visible(bool visible)
{
   menu_visible = visible;

   if (!visible)
      menu_has_frame = false;
}

void dc_pvr_menu_set_frame(const uint16_t *src, unsigned width, unsigned height)
{
   unsigned y;

   if (!src || !width || !height)
   {
      menu_has_frame = false;
      return;
   }

   if (!pvr_ensure_menu_texture(width, height))
   {
      menu_has_frame = false;
      return;
   }

   for (y = 0; y < height; y++)
      memcpy(menu_staging + y * width, src + y * width,
            (size_t)width * sizeof(uint16_t));

   menu_has_frame = true;
}
