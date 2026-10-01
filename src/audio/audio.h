#ifndef EFX_AUDIO_H
#define EFX_AUDIO_H

/*
 * F14 audio core. Pure C: no Sokol, no quickjs (ADR 0003 module walls).
 *
 * Push model (design D2): all state lives on the calling (main) thread. Each
 * frame the platform layer advances streaming source playheads and mixes a
 * block with efx_audio_mix, then pushes it to the device. There are no
 * threads, no atomics, and no cross-thread lifetimes, so the core is a
 * deterministic pure function of its state and is unit-testable headlessly.
 *
 * Sources come in two kinds and are loaded separately from playback:
 *   - static  (efx_audio_data): the fully-decoded PCM is held in memory and is
 *     re-playable; one buffer can back many simultaneous playheads.
 *   - streamed (efx_audio_stream): the compressed resource is decoded
 *     incrementally by each playhead through its own decoder + ~1 s ring.
 *
 * Playback is one fixed bank of voices (EFX_AUDIO_MAX_VOICES), each holding a
 * volume/pan/pitch/loop/paused state and referencing either source kind. The
 * number of simultaneously *streaming* voices is bounded by
 * EFX_AUDIO_MAX_STREAMS. The engine owns all mixing: scripts never see
 * channels, buses, or buffers.
 */

#include <stddef.h>
#include <stdint.h>

#define EFX_AUDIO_MAX_VOICES 32
#define EFX_AUDIO_MAX_STREAMS 4
#define EFX_AUDIO_DEFAULT_RATE 44100

/* error codes */
#define EFX_AUDIO_OK 0
#define EFX_AUDIO_ERR_FORMAT 1 /* not a parseable WAV/MP3 */
#define EFX_AUDIO_ERR_DECODE 2
#define EFX_AUDIO_ERR_NOMEM 3
#define EFX_AUDIO_ERR_ARG 4

typedef struct efx_audio_data efx_audio_data;
typedef struct efx_audio_stream efx_audio_stream;

/* ---- lifecycle (device sample rate; resets all state) ---- */
void efx_audio_init(int sample_rate);
void efx_audio_shutdown(void);
/* Device readiness. The platform backend clears this when no device is
 * available (headless CI) or, on web, until the autoplay unlock; defaults to
 * ready so headless tests can mix. */
void efx_audio_set_available(int available);

/* Web autoplay unlock: the binding calls efx_audio_request_resume(), which
 * invokes the platform-registered callback (if any). Desktop registers none. */
typedef void (*efx_audio_resume_fn)(void *ud);
void efx_audio_set_resume_callback(efx_audio_resume_fn fn, void *ud);
void efx_audio_request_resume(void);

/* ---- static sound data (fully in memory, source rate, stereo) ---- */
efx_audio_data *efx_audio_data_load(const uint8_t *bytes, size_t size,
                                    int *err);
void efx_audio_data_release(efx_audio_data *d);
int efx_audio_data_rate(const efx_audio_data *d);
uint64_t efx_audio_data_frames(const efx_audio_data *d);

/* ---- streamed source (compressed bytes; decoded per playhead) ---- */
efx_audio_stream *efx_audio_stream_load(const uint8_t *bytes, size_t size,
                                        int *err);
void efx_audio_stream_release(efx_audio_stream *s);

/* ---- unified playback (fixed bank of voices) ----
 * Allocate a voice for a static or streamed source. Returns a voice id in
 * [0, EFX_AUDIO_MAX_VOICES), or -1 when the request cannot be satisfied
 * (stream cap reached with no stealable stream, or the whole bank is looping).
 * A voice is allocated even when no device is ready (web pre-unlock); it stays
 * silent until the device becomes available. */
int efx_audio_play_data(const efx_audio_data *d, float volume, float pan,
                        float pitch, int loop);
int efx_audio_play_stream(const efx_audio_stream *s, float volume, float pan,
                          float pitch, int loop);

void efx_audio_stop_voice(int voice);
void efx_audio_set_voice_volume(int voice, float v);
void efx_audio_set_voice_pan(int voice, float p);
void efx_audio_set_voice_pitch(int voice, float p);
void efx_audio_set_voice_loop(int voice, int loop);
void efx_audio_set_voice_paused(int voice, int paused);
/* 1 when the voice is active, not paused, and the device is available. */
int efx_audio_voice_playing(int voice);
int efx_audio_voice_paused(int voice);
/* Unique per voice start (0,1,2,...); -1 when the voice is not active. Lets a
 * script handle detect that its voice was stolen and reused. */
long long efx_audio_voice_serial(int voice);
int efx_audio_active_voice_count(void);
int efx_audio_stream_voice_count(void);

/* ---- streaming pump (call once per frame from the main thread) ---- */
void efx_audio_pump(void);

/* ---- master output gain (applied after the voice sum) ---- */
void efx_audio_set_master_volume(float v);
float efx_audio_master_volume(void);

/* ---- mixing (interleaved stereo float32) ---- */
void efx_audio_mix(float *out, int frames);

#endif
