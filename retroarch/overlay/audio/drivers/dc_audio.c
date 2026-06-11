/* RetroArch Dreamcast audio driver (wraps shared KOS backend) */

#include "../../dreamcast/dc_audio.h"

#include <boolean.h>
#include <retro_miscellaneous.h>

#include "../audio_driver.h"

typedef struct dc_audio
{
   dc_audio_stream_t *stream;
   unsigned rate;
   bool nonblock;
   bool paused;
} dc_audio_t;

static void *dc_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   dc_audio_t *wa;
   unsigned picked;

   (void)device;
   (void)latency;
   (void)block_frames;

   wa = (dc_audio_t *)calloc(1, sizeof(*wa));
   if (!wa)
      return NULL;

   picked = dc_audio_pick_rate(rate);
   wa->stream = dc_audio_create(picked);
   if (!wa->stream)
   {
      free(wa);
      return NULL;
   }

   if (!dc_audio_start(wa->stream, picked))
   {
      dc_audio_destroy(wa->stream);
      free(wa);
      return NULL;
   }

   wa->rate    = picked;
   *new_rate   = picked;
   wa->paused  = false;
   return wa;
}

static ssize_t dc_audio_write(void *data, const void *buf_, size_t len)
{
   dc_audio_t *wa = (dc_audio_t *)data;
   size_t frames;
   size_t written;

   if (!wa || wa->paused || !wa->stream)
      return 0;

   frames = len >> 2;
   written = dc_audio_write(wa->stream, (const int16_t *)buf_, frames,
         wa->nonblock);

   return (ssize_t)(written << 2);
}

static bool dc_audio_stop_cb(void *data)
{
   dc_audio_t *wa = (dc_audio_t *)data;

   if (!wa || !wa->stream)
      return false;

   dc_audio_stop(wa->stream);
   wa->paused = true;
   return true;
}

static bool dc_audio_start_cb(void *data, bool is_shutdown)
{
   dc_audio_t *wa = (dc_audio_t *)data;

   (void)is_shutdown;

   if (!wa || !wa->stream)
      return false;

   if (!dc_audio_start(wa->stream, wa->rate))
      return false;

   wa->paused = false;
   return true;
}

static bool dc_audio_alive(void *data)
{
   dc_audio_t *wa = (dc_audio_t *)data;

   if (!wa || !wa->stream)
      return false;

   dc_audio_poll(wa->stream);
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

   dc_audio_destroy(wa->stream);
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

   if (!wa || !wa->stream)
      return 0;

   return dc_audio_write_avail_frames(wa->stream) << 2;
}

static size_t dc_audio_buffer_size(void *data)
{
   dc_audio_t *wa = (dc_audio_t *)data;

   if (!wa || !wa->stream)
      return 0;

   return dc_audio_buffer_frames(wa->stream) << 2;
}

audio_driver_t audio_dc = {
   dc_audio_init,
   dc_audio_write,
   dc_audio_stop_cb,
   dc_audio_start_cb,
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
