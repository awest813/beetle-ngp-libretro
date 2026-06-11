/* RetroArch Dreamcast audio driver (KallistiOS snd_stream) */

#include <kos.h>
#include <dc/sound/stream.h>

#include <boolean.h>
#include <memalign.h>
#include <retro_miscellaneous.h>

#include "../audio_driver.h"

#define DC_AUDIO_RING_SAMPLES 16384
#define DC_AUDIO_CHUNK_FRAMES 512

typedef struct dc_audio
{
   snd_stream_hnd_t stream;
   int16_t *ring;
   int16_t *out_buf;
   volatile uint32_t read_pos;
   volatile uint32_t write_pos;
   unsigned rate;
   bool nonblock;
   bool paused;
} dc_audio_t;

static dc_audio_t *dc_audio_global;

static void *dc_stream_callback(snd_stream_hnd_t hnd, int smp_req, int *smp_recv)
{
   dc_audio_t *wa = dc_audio_global;
   int i;

   (void)hnd;

   if (!wa || !wa->out_buf)
   {
      *smp_recv = 0;
      return NULL;
   }

   for (i = 0; i < smp_req; i++)
   {
      if (wa->read_pos == wa->write_pos)
      {
         wa->out_buf[i * 2]     = 0;
         wa->out_buf[i * 2 + 1] = 0;
         continue;
      }

      wa->out_buf[i * 2]     = wa->ring[wa->read_pos++];
      wa->out_buf[i * 2 + 1] = wa->ring[wa->read_pos++];
      wa->read_pos %= (DC_AUDIO_RING_SAMPLES * 2);
   }

   *smp_recv = smp_req;
   return wa->out_buf;
}

static void dc_audio_queue(dc_audio_t *wa, const int16_t *src, size_t frames)
{
   size_t i;

   for (i = 0; i < frames; i++)
   {
      uint32_t next = (wa->write_pos + 2) % (DC_AUDIO_RING_SAMPLES * 2);

      if (next == wa->read_pos)
         break;

      wa->ring[wa->write_pos++] = src[i * 2];
      wa->ring[wa->write_pos++] = src[i * 2 + 1];
      wa->write_pos %= (DC_AUDIO_RING_SAMPLES * 2);
   }
}

static void *dc_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   dc_audio_t *wa;

   (void)device;
   (void)latency;
   (void)block_frames;

   wa = (dc_audio_t *)calloc(1, sizeof(*wa));
   if (!wa)
      return NULL;

   wa->ring = (int16_t *)calloc(DC_AUDIO_RING_SAMPLES * 2, sizeof(int16_t));
   wa->out_buf = (int16_t *)memalign(32, SND_STREAM_BUFFER_MAX * sizeof(int16_t));

   if (!wa->ring || !wa->out_buf)
   {
      free(wa->ring);
      free(wa->out_buf);
      free(wa);
      return NULL;
   }

   wa->rate = rate;
   *new_rate = rate;

   snd_stream_init();
   wa->stream = snd_stream_alloc(dc_stream_callback, SND_STREAM_BUFFER_MAX);
   if (wa->stream == SND_STREAM_INVALID)
   {
      free(wa->ring);
      free(wa->out_buf);
      free(wa);
      return NULL;
   }

   dc_audio_global = wa;
   snd_stream_start(wa->stream, wa->rate, 1);
   wa->paused = false;

   return wa;
}

static ssize_t dc_audio_write(void *data, const void *buf_, size_t len)
{
   const uint32_t *buf = (const uint32_t *)buf_;
   dc_audio_t *wa = (dc_audio_t *)data;
   size_t frames = len >> 2;
   size_t queued = 0;

   if (!wa || wa->paused)
      return 0;

   if (wa->stream != SND_STREAM_INVALID)
      snd_stream_poll(wa->stream);

   while (frames)
   {
      size_t chunk = frames;

      if (chunk > DC_AUDIO_CHUNK_FRAMES)
         chunk = DC_AUDIO_CHUNK_FRAMES;

      dc_audio_queue(wa, (const int16_t *)buf, chunk);
      queued += chunk;
      frames -= chunk;
      buf    += chunk;

      if (wa->nonblock)
         break;
   }

   return (ssize_t)(queued << 2);
}

static bool dc_audio_stop(void *data)
{
   dc_audio_t *wa = (dc_audio_t *)data;

   if (!wa)
      return false;

   if (wa->stream != SND_STREAM_INVALID)
      snd_stream_stop(wa->stream);

   wa->read_pos  = 0;
   wa->write_pos = 0;
   wa->paused    = true;
   return true;
}

static bool dc_audio_start(void *data, bool is_shutdown)
{
   dc_audio_t *wa = (dc_audio_t *)data;

   (void)is_shutdown;

   if (!wa)
      return false;

   if (wa->stream != SND_STREAM_INVALID)
      snd_stream_start(wa->stream, wa->rate, 1);

   wa->paused = false;
   return true;
}

static bool dc_audio_alive(void *data)
{
   dc_audio_t *wa = (dc_audio_t *)data;

   if (!wa)
      return false;

   if (wa->stream != SND_STREAM_INVALID)
      snd_stream_poll(wa->stream);

   return !wa->paused;
}

static void dc_audio_set_nonblock_state(void *data, bool state)
{
   dc_audio_t *wa = (dc_audio_t *)data;

   if (wa)
      wa->nonblock = state;
}

static void dc_audio_free(void *data)
{
   dc_audio_t *wa = (dc_audio_t *)data;

   if (!wa)
      return;

   if (wa->stream != SND_STREAM_INVALID)
   {
      snd_stream_stop(wa->stream);
      snd_stream_destroy(wa->stream);
      snd_stream_shutdown();
   }

   dc_audio_global = NULL;
   free(wa->ring);
   free(wa->out_buf);
   free(wa);
}

static bool dc_audio_use_float(void *data)
{
   (void)data;
   return false;
}

static size_t dc_audio_write_avail(void *data)
{
   dc_audio_t *wa = (dc_audio_t *)data;
   uint32_t used;

   if (!wa)
      return 0;

   used = (wa->write_pos + DC_AUDIO_RING_SAMPLES * 2 - wa->read_pos)
        % (DC_AUDIO_RING_SAMPLES * 2);

   return ((DC_AUDIO_RING_SAMPLES * 2) - used) * sizeof(int16_t);
}

static size_t dc_audio_buffer_size(void *data)
{
   (void)data;
   return DC_AUDIO_RING_SAMPLES * 2 * sizeof(int16_t);
}

audio_driver_t audio_dc = {
   dc_audio_init,
   dc_audio_write,
   dc_audio_stop,
   dc_audio_start,
   dc_audio_alive,
   dc_audio_set_nonblock_state,
   dc_audio_free,
   dc_audio_use_float,
   "dc",
   NULL,
   NULL,
   dc_audio_write_avail,
   dc_audio_buffer_size,
   NULL
};
