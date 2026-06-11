/* Shared Dreamcast audio backend (KallistiOS snd_stream) */

#include "dc_audio.h"

#include <stdlib.h>
#include <string.h>

#define DC_AUDIO_RING_SAMPLES 16384
#define DC_AUDIO_OUT_BYTES    (SND_STREAM_BUFFER_MAX * 2)

struct dc_audio_stream
{
   snd_stream_hnd_t handle;
   int16_t *ring;
   int16_t *out_buf;
   volatile uint32_t read_pos;
   volatile uint32_t write_pos;
   unsigned rate;
   bool started;
};

static dc_audio_stream_t *dc_audio_active;

unsigned dc_audio_pick_rate(unsigned requested)
{
   (void)requested;
   return DC_AUDIO_NATIVE_RATE;
}

static void dc_audio_clear_out(int16_t *out, int stereo_frames)
{
   memset(out, 0, (size_t)stereo_frames * 2 * sizeof(int16_t));
}

static void *dc_audio_stream_callback(snd_stream_hnd_t hnd, int req_bytes,
      int *got_bytes)
{
   dc_audio_stream_t *stream = dc_audio_active;
   int16_t *dst;
   int stereo_frames;
   int i;

   (void)hnd;

   if (!stream || !stream->out_buf || req_bytes <= 0)
   {
      if (got_bytes)
         *got_bytes = 0;
      return NULL;
   }

   stereo_frames = req_bytes / ((int)sizeof(int16_t) * 2);
   if (stereo_frames <= 0)
   {
      *got_bytes = 0;
      return NULL;
   }

   if (stereo_frames > (int)(DC_AUDIO_OUT_BYTES / (sizeof(int16_t) * 2)))
      stereo_frames = (int)(DC_AUDIO_OUT_BYTES / (sizeof(int16_t) * 2));

   dst = stream->out_buf;

   for (i = 0; i < stereo_frames; i++)
   {
      if (stream->read_pos == stream->write_pos)
      {
         dst[i * 2]     = 0;
         dst[i * 2 + 1] = 0;
         continue;
      }

      dst[i * 2]     = stream->ring[stream->read_pos++];
      dst[i * 2 + 1] = stream->ring[stream->read_pos++];
      stream->read_pos %= (DC_AUDIO_RING_SAMPLES * 2);
   }

   *got_bytes = stereo_frames * (int)sizeof(int16_t) * 2;
   *got_bytes &= ~7;
   return dst;
}

static size_t dc_audio_queue(dc_audio_stream_t *stream,
      const int16_t *src, size_t frames)
{
   size_t queued = 0;

   for (; queued < frames; queued++)
   {
      uint32_t next = (stream->write_pos + 2) % (DC_AUDIO_RING_SAMPLES * 2);

      if (next == stream->read_pos)
         break;

      stream->ring[stream->write_pos++] = src[queued * 2];
      stream->ring[stream->write_pos++] = src[queued * 2 + 1];
      stream->write_pos %= (DC_AUDIO_RING_SAMPLES * 2);
   }

   return queued;
}

dc_audio_stream_t *dc_audio_create(unsigned rate)
{
   dc_audio_stream_t *stream;

   stream = (dc_audio_stream_t *)calloc(1, sizeof(*stream));
   if (!stream)
      return NULL;

   stream->ring = (int16_t *)calloc(DC_AUDIO_RING_SAMPLES * 2, sizeof(int16_t));
   stream->out_buf = (int16_t *)memalign(32, DC_AUDIO_OUT_BYTES);

   if (!stream->ring || !stream->out_buf)
   {
      free(stream->ring);
      free(stream->out_buf);
      free(stream);
      return NULL;
   }

   dc_audio_clear_out(stream->out_buf,
         (int)(DC_AUDIO_OUT_BYTES / (sizeof(int16_t) * 2)));

   stream->handle = SND_STREAM_INVALID;
   stream->rate   = dc_audio_pick_rate(rate);
   stream->started = false;

   return stream;
}

void dc_audio_destroy(dc_audio_stream_t *stream)
{
   if (!stream)
      return;

   dc_audio_stop(stream);

   if (stream->handle != SND_STREAM_INVALID)
   {
      snd_stream_destroy(stream->handle);
      stream->handle = SND_STREAM_INVALID;
   }

   if (dc_audio_active == stream)
   {
      snd_stream_shutdown();
      dc_audio_active = NULL;
   }

   free(stream->ring);
   free(stream->out_buf);
   free(stream);
}

bool dc_audio_start(dc_audio_stream_t *stream, unsigned rate)
{
   if (!stream)
      return false;

   stream->rate = dc_audio_pick_rate(rate);

   if (stream->handle == SND_STREAM_INVALID)
   {
      if (!dc_audio_active)
      {
         if (snd_stream_init() < 0)
            return false;
      }

      dc_audio_active = stream;
      stream->handle = snd_stream_alloc(dc_audio_stream_callback,
            SND_STREAM_BUFFER_MAX);
      if (stream->handle == SND_STREAM_INVALID)
      {
         if (dc_audio_active == stream)
            dc_audio_active = NULL;
         return false;
      }
   }

   stream->read_pos  = 0;
   stream->write_pos = 0;
   snd_stream_start(stream->handle, stream->rate, 1);
   stream->started = true;
   return true;
}

void dc_audio_stop(dc_audio_stream_t *stream)
{
   if (!stream || stream->handle == SND_STREAM_INVALID)
      return;

   snd_stream_stop(stream->handle);
   stream->read_pos  = 0;
   stream->write_pos = 0;
   stream->started   = false;
}

void dc_audio_poll(dc_audio_stream_t *stream)
{
   if (stream && stream->started && stream->handle != SND_STREAM_INVALID)
      snd_stream_poll(stream->handle);
}

size_t dc_audio_write(dc_audio_stream_t *stream,
      const int16_t *data, size_t frames, bool nonblock)
{
   size_t queued;

   if (!stream || !stream->started || !data || !frames)
      return 0;

   dc_audio_poll(stream);
   queued = dc_audio_queue(stream, data, frames);

   if (!nonblock)
   {
      while (queued < frames)
      {
         dc_audio_poll(stream);
         queued += dc_audio_queue(stream, data + queued * 2, frames - queued);
      }
   }

   return queued;
}

size_t dc_audio_write_avail_frames(dc_audio_stream_t *stream)
{
   uint32_t used;

   if (!stream)
      return 0;

   used = (stream->write_pos + (DC_AUDIO_RING_SAMPLES * 2) - stream->read_pos)
        % (DC_AUDIO_RING_SAMPLES * 2);

   return (DC_AUDIO_RING_SAMPLES * 2 - used) / 2;
}

size_t dc_audio_buffer_frames(dc_audio_stream_t *stream)
{
   (void)stream;
   return DC_AUDIO_RING_SAMPLES;
}
