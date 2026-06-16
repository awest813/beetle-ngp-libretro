/* Shared Dreamcast audio backend (KallistiOS snd_stream) */

#ifndef DC_AUDIO_H
#define DC_AUDIO_H

#include <kos.h>
#include <dc/sound/stream.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct dc_audio_stream dc_audio_stream_t;

/* NGP / Beetle outputs 44100 Hz stereo PCM. */
#define DC_AUDIO_NATIVE_RATE 44100

unsigned dc_audio_pick_rate(unsigned requested);

dc_audio_stream_t *dc_audio_create(unsigned rate);
void dc_audio_destroy(dc_audio_stream_t *stream);

bool dc_audio_start(dc_audio_stream_t *stream, unsigned rate);
void dc_audio_stop(dc_audio_stream_t *stream);
void dc_audio_poll(dc_audio_stream_t *stream);

size_t dc_audio_write(dc_audio_stream_t *stream,
      const int16_t *data, size_t frames, bool nonblock);

size_t dc_audio_write_avail_frames(dc_audio_stream_t *stream);
size_t dc_audio_buffer_frames(dc_audio_stream_t *stream);

void dc_audio_set_volume(dc_audio_stream_t *stream, uint8_t volume);
void dc_audio_set_enabled(dc_audio_stream_t *stream, bool enabled);

void dc_audio_pause(dc_audio_stream_t *stream);
void dc_audio_resume(dc_audio_stream_t *stream);
void dc_audio_flush(dc_audio_stream_t *stream);

unsigned dc_audio_underruns(dc_audio_stream_t *stream);

#endif
