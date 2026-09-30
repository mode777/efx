#include "audio/decode.h"

#include <stdlib.h>
#include <string.h>

#include "dr_mp3.h"
#include "dr_wav.h"

struct efx_decoder {
    int kind; /* 0 = wav, 1 = mp3 */
    drwav wav;
    drmp3 mp3;
    uint32_t rate;
    uint32_t channels;
    uint64_t frames;
    /* staging buffer for the source channel layout (grown on demand) */
    float *scratch;
    uint64_t scratch_frames;
};

static int decoder_scratch(efx_decoder *d, uint64_t frames) {
    if (frames <= d->scratch_frames) {
        return 0;
    }
    uint64_t need = frames;
    if (need < 1024) {
        need = 1024;
    }
    float *p = realloc(d->scratch, (size_t)need * (size_t)d->channels *
                                       sizeof(float));
    if (!p) {
        return -1;
    }
    d->scratch = p;
    d->scratch_frames = need;
    return 0;
}

int efx_decoder_open(const uint8_t *data, size_t size, efx_decoder **out) {
    if (!data || size == 0 || !out) {
        return 1;
    }
    *out = NULL;
    efx_decoder *d = calloc(1, sizeof(*d));
    if (!d) {
        return 1;
    }
    if (drwav_init_memory(&d->wav, data, size, NULL)) {
        d->kind = 0;
        d->rate = d->wav.sampleRate;
        d->channels = d->wav.channels;
        d->frames = d->wav.totalPCMFrameCount;
    } else if (drmp3_init_memory(&d->mp3, data, size, NULL)) {
        d->kind = 1;
        d->rate = d->mp3.sampleRate;
        d->channels = d->mp3.channels;
        d->frames = drmp3_get_pcm_frame_count(&d->mp3);
    } else {
        free(d);
        return 1;
    }
    if (d->rate == 0 || d->channels == 0) {
        efx_decoder_close(d);
        return 1;
    }
    if (decoder_scratch(d, 1024) != 0) {
        efx_decoder_close(d);
        return 1;
    }
    *out = d;
    return 0;
}

void efx_decoder_close(efx_decoder *d) {
    if (!d) {
        return;
    }
    if (d->kind == 0) {
        drwav_uninit(&d->wav);
    } else {
        drmp3_uninit(&d->mp3);
    }
    free(d->scratch);
    free(d);
}

uint32_t efx_decoder_rate(const efx_decoder *d) {
    return d ? d->rate : 0;
}

uint32_t efx_decoder_channels(const efx_decoder *d) {
    return d ? d->channels : 0;
}

uint64_t efx_decoder_frames(const efx_decoder *d) {
    return d ? d->frames : 0;
}

uint64_t efx_decoder_read_stereo(efx_decoder *d, float *out, uint64_t frames) {
    if (!d || !out || frames == 0) {
        return 0;
    }
    if (decoder_scratch(d, frames) != 0) {
        return 0;
    }
    uint64_t got;
    if (d->kind == 0) {
        got = (uint64_t)drwav_read_pcm_frames_f32(&d->wav, frames, d->scratch);
    } else {
        got = (uint64_t)drmp3_read_pcm_frames_f32(&d->mp3, frames, d->scratch);
    }
    if (got > frames) {
        got = frames;
    }
    for (uint64_t i = 0; i < got; i++) {
        if (d->channels == 1) {
            float m = d->scratch[i];
            out[i * 2] = m;
            out[i * 2 + 1] = m;
        } else {
            out[i * 2] = d->scratch[i * (uint64_t)d->channels];
            out[i * 2 + 1] = d->scratch[i * (uint64_t)d->channels + 1];
        }
    }
    return got;
}

void efx_decoder_rewind(efx_decoder *d) {
    if (!d) {
        return;
    }
    if (d->kind == 0) {
        drwav_seek_to_pcm_frame(&d->wav, 0);
    } else {
        drmp3_seek_to_pcm_frame(&d->mp3, 0);
    }
}
