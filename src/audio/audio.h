#ifndef EFX_AUDIO_H
#define EFX_AUDIO_H

/*
 * F14 audio core. Pure C: no Sokol, no quickjs (ADR 0003 module walls).
 *
 * Push model (design D2): all state lives on the calling (main) thread. Each
 * frame the platform layer pumps the streaming music decoder and mixes a block
 * with efx_audio_mix, then pushes it to the device. There are no threads, no
 * atomics, and no cross-thread lifetimes, so the core is a deterministic pure
 * function of its state and is unit-testable headlessly.
 *
 * The engine owns all mixing: scripts never see channels, buses, or buffers.
 * A fixed bank of 32 sound-effect voices is mixed from fully-decoded PCM; a
 * single streamed background-music source is decoded incrementally.
 */

#include <stddef.h>
#include <stdint.h>

#define EFX_AUDIO_MAX_VOICES 32
#define EFX_AUDIO_DEFAULT_RATE 44100

/* error codes */
#define EFX_AUDIO_OK 0
#define EFX_AUDIO_ERR_FORMAT 1 /* not a parseable WAV/MP3 */
#define EFX_AUDIO_ERR_DECODE 2
#define EFX_AUDIO_ERR_NOMEM 3
#define EFX_AUDIO_ERR_ARG 4

typedef struct efx_sound_data efx_sound_data;

/* ---- lifecycle (device sample rate; resets all state) ---- */
void efx_audio_init(int sample_rate);
void efx_audio_shutdown(void);
int efx_audio_sample_rate(void);
/* Device readiness. The platform backend clears this when no device is
 * available (headless CI) or, on web, until the autoplay unlock; defaults to
 * ready so headless tests can mix. */
void efx_audio_set_available(int available);
int efx_audio_available(void);

/* Web autoplay unlock: the binding calls efx_audio_request_resume(), which
 * invokes the platform-registered callback (if any). Desktop registers none. */
typedef void (*efx_audio_resume_fn)(void *ud);
void efx_audio_set_resume_callback(efx_audio_resume_fn fn, void *ud);
void efx_audio_request_resume(void);

/* ---- decoded sound data (fully in memory, source rate, stereo) ---- */
efx_sound_data *efx_audio_sound_data_load(const uint8_t *bytes, size_t size,
                                          int *err);
void efx_audio_sound_data_retain(efx_sound_data *s);
void efx_audio_sound_data_release(efx_sound_data *s);
int efx_audio_sound_data_rate(const efx_sound_data *s);
uint64_t efx_audio_sound_data_frames(const efx_sound_data *s);

/* ---- sound-effect voices (fixed bank, 32) ----
 * Returns a voice id in [0, EFX_AUDIO_MAX_VOICES), or -1 when no voice is
 * available (all busy and all looping) or the device is not ready. */
int efx_audio_play_effect(const efx_sound_data *s, float volume, float pan,
                          float pitch, int loop);
void efx_audio_stop_voice(int voice);
void efx_audio_set_voice_volume(int voice, float v);
void efx_audio_set_voice_pan(int voice, float p);
void efx_audio_set_voice_pitch(int voice, float p);
int efx_audio_voice_playing(int voice);
/* Unique per voice start (0,1,2,...); -1 when the voice is not playing. Lets a
 * script handle detect that its voice was stolen and reused. */
long long efx_audio_voice_serial(int voice);
int efx_audio_active_voice_count(void);

/* ---- background music (single streaming source) ----
 * play_music copies the compressed bytes and opens a stream; it is allowed
 * while the device is not ready (web pre-unlock) so playback begins on unlock.
 * pump decodes ahead into the ring (call once per frame from the main thread). */
int efx_audio_play_music(const uint8_t *bytes, size_t size, float volume,
                         int loop, int *err);
void efx_audio_stop_music(void);
void efx_audio_pause_music(int paused);
void efx_audio_set_music_volume(float v);
int efx_audio_music_playing(void);
int efx_audio_music_paused(void);
void efx_audio_music_pump(void);

/* ---- mixing (interleaved stereo float32) ---- */
void efx_audio_mix(float *out, int frames);

#endif
