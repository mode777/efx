#ifndef EFX_AUDIO_DECODE_H
#define EFX_AUDIO_DECODE_H

/*
 * F14 audio decoder abstraction over the vendored dr_libs headers (`dr_wav`
 * for WAV, `dr_mp3` for MP3). Pure C: no Sokol, no quickjs (ADR 0003 module
 * walls). Decodes from an in-memory buffer provided by the F6a resource
 * provider; the buffer must stay valid for the decoder's lifetime.
 *
 * Output is always interleaved stereo float32 (mono is duplicated, >2 channels
 * take the first two). This is the engine's internal sample format.
 */

#include <stddef.h>
#include <stdint.h>

typedef struct efx_decoder efx_decoder;

/* Opens a decoder over an in-memory buffer. Returns 0 on success, non-zero
 * when the bytes are neither a parseable WAV nor MP3 stream. */
int efx_decoder_open(const uint8_t *data, size_t size, efx_decoder **out);

void efx_decoder_close(efx_decoder *d);

uint32_t efx_decoder_rate(const efx_decoder *d);
uint32_t efx_decoder_channels(const efx_decoder *d);
/* Total frame count; 0 when unknown. */
uint64_t efx_decoder_frames(const efx_decoder *d);

/* Reads up to `frames` frames as interleaved stereo float32. Returns the
 * number of frames written (0 at end of stream). */
uint64_t efx_decoder_read_stereo(efx_decoder *d, float *out, uint64_t frames);

/* Rewinds to the first frame (used for looping music). */
void efx_decoder_rewind(efx_decoder *d);

#endif
