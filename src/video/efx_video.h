#ifndef EFX_VIDEO_H
#define EFX_VIDEO_H

/*
 * MPEG-1 video playback core (video-playback change, ADR 0063). Pure C: no
 * Sokol, no quickjs (ADR 0003 module walls). Decodes over an in-memory buffer
 * provided by the F6a resource provider; the buffer must stay valid for the
 * decoder's lifetime (the pl_mpeg decoder reads it in place, it is never
 * written).
 *
 * One playhead (the video clock, seconds) drives both streams: while playing,
 * `efx_video_advance` decodes the video frames due in the elapsed window (the
 * last one wins; long stalls jump the playhead with bounded decode work) and
 * decodes the audio due in the same window into an internal ring.
 * `efx_video_pull_audio` drains that ring as interleaved stereo float32 at the
 * stream's native rate — the shape the F14 audio pull-voice consumes; the
 * audio core owns rate conversion to the device rate.
 *
 * Video frames are produced as interleaved RGBA8 (pl_mpeg's YCbCr->RGB
 * conversion) exactly `width * height * 4` bytes per frame. Decoding is
 * deterministic for identical inputs on every target (ADR 0020 contract).
 */

#include <stddef.h>
#include <stdint.h>

/* error codes */
#define EFX_VIDEO_OK 0
#define EFX_VIDEO_ERR_FORMAT 1  /* not a parseable MPEG-1 program stream */
#define EFX_VIDEO_ERR_NOMEM 2
#define EFX_VIDEO_ERR_ARG 3

typedef struct efx_video efx_video;

/* Opens a decoder over an in-memory buffer. Returns EFX_VIDEO_OK and sets
 * *out, or an error code (*out untouched on failure). */
int efx_video_open(const uint8_t *bytes, size_t size, efx_video **out);

/* Releases the decoder; the caller keeps owning the buffer. NULL safe. */
void efx_video_destroy(efx_video *v);

int efx_video_width(const efx_video *v);
int efx_video_height(const efx_video *v);
/* frames per second (> 0), total duration in seconds (> 0) */
double efx_video_fps(const efx_video *v);
double efx_video_duration(const efx_video *v);
/* 1 when the resource carries an MPEG audio layer 2 stream */
int efx_video_has_audio(const efx_video *v);
/* native sample rate of the audio stream; 0 when there is no audio */
uint32_t efx_video_audio_rate(const efx_video *v);

/* playback state: a video starts paused at time 0. `set_playing` starts and
 * holds advancement; `ended` becomes 1 when a non-looping video reached its
 * duration (the last frame stays on screen; setting playing again restarts
 * from the beginning). */
void efx_video_set_playing(efx_video *v, int playing);
int efx_video_playing(const efx_video *v);
int efx_video_ended(const efx_video *v);
double efx_video_time(const efx_video *v);

void efx_video_set_loop(efx_video *v, int loop);
int efx_video_loop(const efx_video *v);

/* Advances the playhead by `dt` seconds when playing (dt < 0 is treated as
 * 0; a paused video ignores the call besides validating args). Decodes the
 * video frames due in the elapsed window — at most EFX_VIDEO_MAX_CATCHUP per
 * call, the last decoded frame wins — and the audio due in the window into
 * the ring. Sets *out_frame (may be NULL) to 1 when a new video frame was
 * decoded this call. Long stalls skip: the playhead jumps to now and the
 * texture shows the latest due frame. */
#define EFX_VIDEO_MAX_CATCHUP 4
int efx_video_advance(efx_video *v, double dt, int *out_frame);

/* RGBA pixels (w * h * 4) of the most recent decoded frame; owned by the
 * decoder, valid until the next advance. Never NULL after a successful
 * open. */
const uint8_t *efx_video_frame_rgba(const efx_video *v);

/* Pulls up to `frames` decoded interleaved stereo float32 samples at the
 * stream's native rate out of the ring. Returns the number of frames
 * written; 0 when starved (no audio, paused-before-decode, drained after
 * end). */
uint64_t efx_video_pull_audio(efx_video *v, float *out, uint64_t frames);

/* Ring diagnostic for tests: frames currently buffered. */
uint64_t efx_video_audio_buffered(const efx_video *v);

#endif
