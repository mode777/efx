#ifndef EFX_AUDIO_BACKEND_H
#define EFX_AUDIO_BACKEND_H

/*
 * F14 platform audio backend: owns the sokol_audio device (push model, design
 * D2) and the per-frame pump/mix/push tick. Confined to efx_platform (ADR 0003
 * module walls); the pure-C core never touches sokol_audio.
 */

/* Sets up sokol_audio and initializes the core with the device sample rate.
 * Soft-fails to a silent, safe state when no device is available. Idempotent. */
void efx_audio_backend_init(void);

/* Per-frame tick: refresh device availability, pump the music decoder, mix and
 * push frames. Call once per frame from the main thread. */
void efx_audio_backend_frame(void);

void efx_audio_backend_shutdown(void);

/* Explicit web autoplay unlock (script `efx.audio.resume()`); no-op elsewhere. */
void efx_audio_backend_resume(void);

#endif
