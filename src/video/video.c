/*
 * video-playback: engine wrapper around the vendored pl_mpeg decoder
 * (vendor/pl_mpeg, MIT — ADR 0063). Implements the contract in efx_video.h.
 *
 * Determinism notes (ADR 0020): all decode ordering is driven by the single
 * video playhead with integer-frame accounting (no float accumulation);
 * pl_mpeg itself is integer/fixed-point, so identical bytes decode to
 * identical RGBA on every target. Audio drains through a fixed-size ring
 * (drops on overflow — a stall artifact, never a steady-state path).
 */

#include "video/efx_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pl_mpeg.h"

/* audio ring: interleaved stereo float32 frames kept buffered ahead of the
 * pull side (~370 ms at 44.1 kHz) */
#define RING_FRAMES 16384

struct efx_video {
    plm_t *plm;

    int width, height;
    double fps;
    double duration;
    uint32_t audio_rate; /* 0 = no audio stream */

    int playing;
    int loop;
    int ended;
    double playhead;      /* video clock, seconds */
    double next_frame;    /* decode-due time of the next video frame */
    uint64_t audio_pos;   /* source frames decoded since (re)start */

    uint8_t *rgba;        /* width * height * 4, latest decoded frame */
    int frame_valid;

    float *ring;          /* RING_FRAMES * 2 floats */
    uint64_t ring_head;   /* next frame to read */
    uint64_t ring_count;  /* frames buffered */
};

/* decode the next video frame into the scratch buffer; 0 at stream end */
static int decode_one_video(efx_video *v) {
    plm_frame_t *f = plm_decode_video(v->plm);
    if (!f) {
        return 0;
    }
    plm_frame_to_rgba(f, v->rgba, (int)v->width * 4);
    v->frame_valid = 1;
    v->next_frame += 1.0 / v->fps;
    return 1;
}

/* decode one MP2 frame (1152 samples/frame) into the ring; drops on overflow */
static void decode_one_audio(efx_video *v) {
    plm_samples_t *s = plm_decode_audio(v->plm);
    if (!s) {
        return;
    }
    v->audio_pos += s->count;
    for (unsigned i = 0; i < s->count; i++) {
        uint64_t slot = (v->ring_head + v->ring_count) % RING_FRAMES;
        v->ring[slot * 2 + 0] = s->interleaved[i * 2 + 0];
        v->ring[slot * 2 + 1] = s->interleaved[i * 2 + 1];
        if (v->ring_count < RING_FRAMES) {
            v->ring_count++;
        } else {
            v->ring_head = (v->ring_head + 1) % RING_FRAMES; /* overwrite old */
        }
    }
}

static void restart(efx_video *v) {
    plm_rewind(v->plm);
    v->playhead = 0.0;
    v->next_frame = 0.0;
    v->audio_pos = 0;
    v->frame_valid = 0;
    v->ring_head = 0;
    v->ring_count = 0;
}

int efx_video_open(const uint8_t *bytes, size_t size, efx_video **out) {
    if (!bytes || !size || !out) {
        return EFX_VIDEO_ERR_ARG;
    }
    efx_video *v = calloc(1, sizeof(*v));
    if (!v) {
        return EFX_VIDEO_ERR_NOMEM;
    }
    /* pl_mpeg takes a non-const pointer for its optional free-when-done
     * ownership; with free_when_done = 0 it never writes the buffer */
    v->plm = plm_create_with_memory((uint8_t *)bytes, size, 0);
    if (!v->plm) {
        free(v);
        return EFX_VIDEO_ERR_NOMEM;
    }
    plm_set_loop(v->plm, 0); /* looping is the wrapper's state machine */

    /* eager first decode: pins the display dimensions (and rejects
     * non-MPEG-1 bytes) before the caller builds anything on top */
    plm_frame_t *f = plm_decode_video(v->plm);
    if (!f) {
        plm_destroy(v->plm);
        free(v);
        return EFX_VIDEO_ERR_FORMAT;
    }
    v->width = (int)f->width;
    v->height = (int)f->height;
    v->fps = plm_get_framerate(v->plm);
    v->duration = plm_get_duration(v->plm);
    if (v->width <= 0 || v->height <= 0 || v->fps <= 0.0 ||
        v->duration <= 0.0) {
        plm_destroy(v->plm);
        free(v);
        return EFX_VIDEO_ERR_FORMAT;
    }
    v->rgba = malloc((size_t)v->width * (size_t)v->height * 4u);
    v->ring = malloc((size_t)RING_FRAMES * 2u * sizeof(float));
    if (!v->rgba || !v->ring) {
        efx_video_destroy(v);
        return EFX_VIDEO_ERR_NOMEM;
    }
    plm_frame_to_rgba(f, v->rgba, v->width * 4);
    v->frame_valid = 1;
    v->next_frame = 1.0 / v->fps;

    if (plm_get_num_audio_streams(v->plm) > 0) {
        plm_samples_t *s = plm_decode_audio(v->plm);
        if (s) {
            v->audio_rate = plm_get_samplerate(v->plm);
            v->audio_pos += s->count;
            for (unsigned i = 0; i < s->count; i++) {
                v->ring[i * 2 + 0] = s->interleaved[i * 2 + 0];
                v->ring[i * 2 + 1] = s->interleaved[i * 2 + 1];
            }
            v->ring_count = s->count;
        }
    }
    *out = v;
    return EFX_VIDEO_OK;
}

void efx_video_destroy(efx_video *v) {
    if (!v) {
        return;
    }
    if (v->plm) {
        plm_destroy(v->plm);
    }
    free(v->rgba);
    free(v->ring);
    free(v);
}

int efx_video_width(const efx_video *v) { return v ? v->width : 0; }

int efx_video_height(const efx_video *v) { return v ? v->height : 0; }

double efx_video_fps(const efx_video *v) { return v ? v->fps : 0.0; }

double efx_video_duration(const efx_video *v) {
    return v ? v->duration : 0.0;
}

int efx_video_has_audio(const efx_video *v) {
    return v && v->audio_rate ? 1 : 0;
}

uint32_t efx_video_audio_rate(const efx_video *v) {
    return v ? v->audio_rate : 0;
}

void efx_video_set_playing(efx_video *v, int playing) {
    if (!v) {
        return;
    }
    playing = playing ? 1 : 0;
    if (playing && !v->playing && v->ended) {
        restart(v); /* play() after ending restarts from the beginning */
        v->ended = 0;
    }
    v->playing = playing;
}

int efx_video_playing(const efx_video *v) { return v ? v->playing : 0; }

int efx_video_ended(const efx_video *v) { return v ? v->ended : 0; }

double efx_video_time(const efx_video *v) { return v ? v->playhead : 0.0; }

void efx_video_set_loop(efx_video *v, int loop) {
    if (v) {
        v->loop = loop ? 1 : 0;
    }
}

int efx_video_loop(const efx_video *v) { return v ? v->loop : 0; }

int efx_video_advance(efx_video *v, double dt, int *out_frame) {
    if (!v) {
        return EFX_VIDEO_ERR_ARG;
    }
    if (out_frame) {
        *out_frame = 0;
    }
    if (dt < 0.0) {
        dt = 0.0;
    }
    if (!v->playing || v->ended) {
        return EFX_VIDEO_OK;
    }
    v->playhead += dt;

    /* loop wrap / end of stream */
    if (v->playhead >= v->duration) {
        if (v->loop) {
            v->playhead -= v->duration;
            restart(v);
        } else {
            v->playhead = v->duration;
            v->ended = 1;
            v->playing = 0;
            return EFX_VIDEO_OK;
        }
    }

    /* video frames due in the window; a long stall snaps the due time
     * forward (bounded decode, sequential dependency chain intact, the
     * texture catches up over subsequent calls) */
    int decoded = 0;
    for (int i = 0; i < EFX_VIDEO_MAX_CATCHUP && v->next_frame <= v->playhead;
         i++) {
        if (!decode_one_video(v)) {
            break;
        }
        decoded++;
    }
    if (decoded && out_frame) {
        *out_frame = 1;
    }
    if (v->next_frame <= v->playhead) {
        v->next_frame = v->playhead; /* snap; decoding stays sequential */
    }

    /* audio due in the same window (source frames <= playhead * rate) */
    if (v->audio_rate) {
        double due = v->playhead * (double)v->audio_rate;
        int guard = 64; /* 1152 samples/frame: > 1 s of slack per advance */
        while (v->audio_pos < due && guard-- > 0) {
            decode_one_audio(v);
        }
    }
    return EFX_VIDEO_OK;
}

const uint8_t *efx_video_frame_rgba(const efx_video *v) {
    return v && v->frame_valid ? v->rgba : NULL;
}

uint64_t efx_video_pull_audio(efx_video *v, float *out, uint64_t frames) {
    if (!v || !out) {
        return 0;
    }
    uint64_t got = 0;
    while (got < frames && v->ring_count > 0) {
        uint64_t slot = v->ring_head % RING_FRAMES;
        out[got * 2 + 0] = v->ring[slot * 2 + 0];
        out[got * 2 + 1] = v->ring[slot * 2 + 1];
        v->ring_head = (v->ring_head + 1) % RING_FRAMES;
        v->ring_count--;
        got++;
    }
    return got;
}

uint64_t efx_video_audio_buffered(const efx_video *v) {
    return v ? v->ring_count : 0;
}
