#include "audio/audio.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "audio/decode.h"

/* streaming decode chunk (source frames) */
#define EFX_STREAM_SRC_CHUNK 2048

struct efx_audio_data {
    int refs;
    int rate;
    uint64_t frames;
    float *pcm; /* stereo interleaved, frames*2 */
};

struct efx_audio_stream {
    int refs;
    uint8_t *bytes; /* owned copy of the compressed stream */
    size_t size;
};

typedef struct efx_voice {
    int active;
    int is_stream;
    int loop;
    int paused;
    int ended;
    const efx_audio_data *data; /* static source, retained while active */
    efx_audio_stream *stream;   /* streamed source, retained while active */
    double pos;                 /* static: fractional source frame index */
    float volume;
    float pan;
    float pitch;
    long long start_seq;
    /* streaming playhead */
    efx_decoder *dec;
    float *ring; /* device-rate stereo ring (~1s) */
    int ring_cap;
    int ring_head;
    int ring_count;
    double spos; /* source-rate -> device-rate resampler position */
    int have_carry;
    float carry_l;
    float carry_r;
    float *src_buf;
    float *dst_buf;
} efx_voice;

static struct {
    int initialized;
    int rate;
    int available;
    efx_voice voices[EFX_AUDIO_MAX_VOICES];
    long long seq;
    float master;
    efx_audio_resume_fn resume_fn;
    void *resume_ud;
} g;

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static void voice_gain(float pan, float *gl, float *gr) {
    float p = clampf(pan, -1.0f, 1.0f);
    *gl = (p <= 0.0f) ? 1.0f : 1.0f - p;
    *gr = (p >= 0.0f) ? 1.0f : 1.0f + p;
}

static void voice_clear(efx_voice *v) {
    if (v->data) {
        efx_audio_data_release((efx_audio_data *)v->data);
    }
    if (v->stream) {
        efx_audio_stream_release(v->stream);
    }
    if (v->dec) {
        efx_decoder_close(v->dec);
    }
    free(v->ring);
    free(v->src_buf);
    free(v->dst_buf);
    memset(v, 0, sizeof(*v));
}

static int voice_stream_count(void) {
    int n = 0;
    for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
        if (g.voices[i].active && g.voices[i].is_stream) {
            n++;
        }
    }
    return n;
}

/* quietest non-looping voice (oldest breaks ties); -1 if all looping. When
 * streams_only, only streamed voices are candidates. */
static int voice_steal(int streams_only) {
    int best = -1;
    for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
        efx_voice *v = &g.voices[i];
        if (!v->active || v->loop) {
            continue;
        }
        if (streams_only && !v->is_stream) {
            continue;
        }
        if (best < 0 || v->volume < g.voices[best].volume ||
            (v->volume == g.voices[best].volume &&
             v->start_seq < g.voices[best].start_seq)) {
            best = i;
        }
    }
    return best;
}

static int voice_alloc(int want_stream) {
    int slot;
    if (want_stream && voice_stream_count() >= EFX_AUDIO_MAX_STREAMS) {
        slot = voice_steal(1);
        if (slot < 0) {
            return -1;
        }
        voice_clear(&g.voices[slot]);
        return slot;
    }
    for (slot = 0; slot < EFX_AUDIO_MAX_VOICES; slot++) {
        if (!g.voices[slot].active) {
            return slot;
        }
    }
    slot = voice_steal(0);
    if (slot < 0) {
        return -1;
    }
    voice_clear(&g.voices[slot]);
    return slot;
}

/* ------------------------------------------------------------------- life */

void efx_audio_shutdown(void) {
    for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
        voice_clear(&g.voices[i]);
    }
    g.initialized = 0;
}

void efx_audio_init(int sample_rate) {
    efx_audio_shutdown();
    if (sample_rate <= 0) {
        sample_rate = EFX_AUDIO_DEFAULT_RATE;
    }
    memset(g.voices, 0, sizeof(g.voices));
    g.rate = sample_rate;
    g.available = 1;
    g.seq = 0;
    g.master = 1.0f;
    g.resume_fn = NULL;
    g.resume_ud = NULL;
    g.initialized = 1;
}

/* Headless/`--script` builds never run the platform backend, so any public
 * entry point lazily initializes a device-free core (available = 1) to keep
 * the API usable in script tests. */
static void ensure_init(void) {
    if (!g.initialized) {
        efx_audio_init(EFX_AUDIO_DEFAULT_RATE);
    }
}

void efx_audio_set_available(int available) {
    ensure_init();
    g.available = available ? 1 : 0;
}

void efx_audio_set_resume_callback(efx_audio_resume_fn fn, void *ud) {
    g.resume_fn = fn;
    g.resume_ud = ud;
}

void efx_audio_request_resume(void) {
    ensure_init();
    if (g.resume_fn) {
        g.resume_fn(g.resume_ud);
    }
}

/* ------------------------------------------------------------- static data */

efx_audio_data *efx_audio_data_load(const uint8_t *bytes, size_t size,
                                    int *err) {
    if (!bytes || size == 0) {
        if (err) {
            *err = EFX_AUDIO_ERR_ARG;
        }
        return NULL;
    }
    efx_decoder *dec = NULL;
    if (efx_decoder_open(bytes, size, &dec) != 0) {
        if (err) {
            *err = EFX_AUDIO_ERR_FORMAT;
        }
        return NULL;
    }
    uint32_t rate = efx_decoder_rate(dec);
    uint64_t cap = efx_decoder_frames(dec);
    if (cap == 0) {
        cap = EFX_STREAM_SRC_CHUNK;
    }
    float *pcm = malloc((size_t)cap * 2 * sizeof(float));
    if (!pcm) {
        efx_decoder_close(dec);
        if (err) {
            *err = EFX_AUDIO_ERR_NOMEM;
        }
        return NULL;
    }
    uint64_t total = 0;
    for (;;) {
        if (total + EFX_STREAM_SRC_CHUNK > cap) {
            uint64_t ncap = cap * 2;
            float *np = realloc(pcm, (size_t)ncap * 2 * sizeof(float));
            if (!np) {
                free(pcm);
                efx_decoder_close(dec);
                if (err) {
                    *err = EFX_AUDIO_ERR_NOMEM;
                }
                return NULL;
            }
            pcm = np;
            cap = ncap;
        }
        uint64_t want = cap - total;
        if (want > EFX_STREAM_SRC_CHUNK) {
            want = EFX_STREAM_SRC_CHUNK;
        }
        uint64_t got = efx_decoder_read_stereo(dec, pcm + total * 2, want);
        total += got;
        if (got == 0) {
            break;
        }
    }
    efx_decoder_close(dec);
    efx_audio_data *d = calloc(1, sizeof(*d));
    if (!d) {
        free(pcm);
        if (err) {
            *err = EFX_AUDIO_ERR_NOMEM;
        }
        return NULL;
    }
    d->refs = 1;
    d->rate = (int)rate;
    d->frames = total;
    d->pcm = pcm;
    if (err) {
        *err = EFX_AUDIO_OK;
    }
    return d;
}

void efx_audio_data_retain(efx_audio_data *d) {
    if (d) {
        d->refs++;
    }
}

void efx_audio_data_release(efx_audio_data *d) {
    if (!d) {
        return;
    }
    d->refs--;
    if (d->refs <= 0) {
        free(d->pcm);
        free(d);
    }
}

int efx_audio_data_rate(const efx_audio_data *d) {
    return d ? d->rate : 0;
}

uint64_t efx_audio_data_frames(const efx_audio_data *d) {
    return d ? d->frames : 0;
}

/* ----------------------------------------------------------- streamed data */

efx_audio_stream *efx_audio_stream_load(const uint8_t *bytes, size_t size,
                                        int *err) {
    if (!bytes || size == 0) {
        if (err) {
            *err = EFX_AUDIO_ERR_ARG;
        }
        return NULL;
    }
    /* validate that the resource parses before accepting it */
    efx_decoder *dec = NULL;
    if (efx_decoder_open(bytes, size, &dec) != 0) {
        if (err) {
            *err = EFX_AUDIO_ERR_FORMAT;
        }
        return NULL;
    }
    efx_decoder_close(dec);
    efx_audio_stream *s = calloc(1, sizeof(*s));
    if (!s) {
        if (err) {
            *err = EFX_AUDIO_ERR_NOMEM;
        }
        return NULL;
    }
    s->bytes = malloc(size);
    if (!s->bytes) {
        free(s);
        if (err) {
            *err = EFX_AUDIO_ERR_NOMEM;
        }
        return NULL;
    }
    memcpy(s->bytes, bytes, size);
    s->size = size;
    s->refs = 1;
    if (err) {
        *err = EFX_AUDIO_OK;
    }
    return s;
}

void efx_audio_stream_retain(efx_audio_stream *s) {
    if (s) {
        s->refs++;
    }
}

void efx_audio_stream_release(efx_audio_stream *s) {
    if (!s) {
        return;
    }
    s->refs--;
    if (s->refs <= 0) {
        free(s->bytes);
        free(s);
    }
}

/* ------------------------------------------------------------------ voices */

int efx_audio_play_data(const efx_audio_data *d, float volume, float pan,
                        float pitch, int loop) {
    ensure_init();
    if (!d || d->frames == 0) {
        return -1;
    }
    int slot = voice_alloc(0);
    if (slot < 0) {
        return -1;
    }
    efx_voice *v = &g.voices[slot];
    efx_audio_data_retain((efx_audio_data *)d);
    v->active = 1;
    v->is_stream = 0;
    v->data = d;
    v->stream = NULL;
    v->loop = loop ? 1 : 0;
    v->paused = 0;
    v->ended = 0;
    v->pos = 0.0;
    v->volume = clampf(volume, 0.0f, 16.0f);
    v->pan = pan;
    v->pitch = (pitch > 0.0f) ? pitch : 1.0f;
    v->start_seq = g.seq++;
    return slot;
}

int efx_audio_play_stream(const efx_audio_stream *s, float volume, float pan,
                          float pitch, int loop) {
    ensure_init();
    if (!s || s->size == 0) {
        return -1;
    }
    efx_decoder *dec = NULL;
    if (efx_decoder_open(s->bytes, s->size, &dec) != 0) {
        return -1;
    }
    int cap = g.rate > 0 ? g.rate : EFX_AUDIO_DEFAULT_RATE;
    if (cap < 1024) {
        cap = 1024;
    }
    float *ring = calloc((size_t)cap * 2, sizeof(float));
    float *src = malloc((size_t)EFX_STREAM_SRC_CHUNK * 2 * sizeof(float));
    float *dst = malloc((size_t)(EFX_STREAM_SRC_CHUNK * 2 + 8) * 2 *
                        sizeof(float));
    if (!ring || !src || !dst) {
        free(ring);
        free(src);
        free(dst);
        efx_decoder_close(dec);
        return -1;
    }
    int slot = voice_alloc(1);
    if (slot < 0) {
        free(ring);
        free(src);
        free(dst);
        efx_decoder_close(dec);
        return -1;
    }
    efx_voice *v = &g.voices[slot];
    efx_audio_stream_retain((efx_audio_stream *)s);
    v->active = 1;
    v->is_stream = 1;
    v->data = NULL;
    v->stream = (efx_audio_stream *)s;
    v->loop = loop ? 1 : 0;
    v->paused = 0;
    v->ended = 0;
    v->volume = clampf(volume, 0.0f, 16.0f);
    v->pan = pan;
    v->pitch = (pitch > 0.0f) ? pitch : 1.0f;
    v->dec = dec;
    v->ring = ring;
    v->ring_cap = cap;
    v->ring_head = 0;
    v->ring_count = 0;
    v->src_buf = src;
    v->dst_buf = dst;
    v->spos = 0.0;
    v->have_carry = 0;
    v->carry_l = 0.0f;
    v->carry_r = 0.0f;
    v->start_seq = g.seq++;
    return slot;
}

void efx_audio_stop_voice(int voice) {
    if (voice < 0 || voice >= EFX_AUDIO_MAX_VOICES) {
        return;
    }
    voice_clear(&g.voices[voice]);
}

void efx_audio_set_voice_volume(int voice, float v) {
    if (voice < 0 || voice >= EFX_AUDIO_MAX_VOICES) {
        return;
    }
    if (g.voices[voice].active) {
        g.voices[voice].volume = clampf(v, 0.0f, 16.0f);
    }
}

void efx_audio_set_voice_pan(int voice, float p) {
    if (voice < 0 || voice >= EFX_AUDIO_MAX_VOICES) {
        return;
    }
    if (g.voices[voice].active) {
        g.voices[voice].pan = p;
    }
}

void efx_audio_set_voice_pitch(int voice, float p) {
    if (voice < 0 || voice >= EFX_AUDIO_MAX_VOICES) {
        return;
    }
    if (g.voices[voice].active) {
        g.voices[voice].pitch = (p > 0.0f) ? p : 1.0f;
    }
}

void efx_audio_set_voice_loop(int voice, int loop) {
    if (voice < 0 || voice >= EFX_AUDIO_MAX_VOICES) {
        return;
    }
    if (g.voices[voice].active) {
        g.voices[voice].loop = loop ? 1 : 0;
    }
}

void efx_audio_set_voice_paused(int voice, int paused) {
    if (voice < 0 || voice >= EFX_AUDIO_MAX_VOICES) {
        return;
    }
    if (g.voices[voice].active) {
        g.voices[voice].paused = paused ? 1 : 0;
    }
}

int efx_audio_voice_playing(int voice) {
    if (voice < 0 || voice >= EFX_AUDIO_MAX_VOICES) {
        return 0;
    }
    efx_voice *v = &g.voices[voice];
    return (v->active && !v->paused && g.available) ? 1 : 0;
}

int efx_audio_voice_paused(int voice) {
    if (voice < 0 || voice >= EFX_AUDIO_MAX_VOICES) {
        return 0;
    }
    return (g.voices[voice].active && g.voices[voice].paused) ? 1 : 0;
}

long long efx_audio_voice_serial(int voice) {
    if (voice < 0 || voice >= EFX_AUDIO_MAX_VOICES) {
        return -1;
    }
    if (!g.voices[voice].active) {
        return -1;
    }
    return g.voices[voice].start_seq;
}

int efx_audio_active_voice_count(void) {
    int n = 0;
    for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
        if (g.voices[i].active) {
            n++;
        }
    }
    return n;
}

int efx_audio_stream_voice_count(void) {
    return voice_stream_count();
}

/* ------------------------------------------------------------------- pump */

static int ring_free(const efx_voice *v) {
    return v->ring_cap - v->ring_count;
}

static void ring_push(efx_voice *v, float l, float r) {
    int idx = (v->ring_head + v->ring_count) % v->ring_cap;
    v->ring[idx * 2] = l;
    v->ring[idx * 2 + 1] = r;
    v->ring_count++;
}

static int ring_pop(efx_voice *v, float *l, float *r) {
    if (v->ring_count <= 0) {
        return 0;
    }
    int idx = v->ring_head;
    *l = v->ring[idx * 2];
    *r = v->ring[idx * 2 + 1];
    v->ring_head = (idx + 1) % v->ring_cap;
    v->ring_count--;
    return 1;
}

static void stream_fill(efx_voice *v) {
    double step = ((double)efx_decoder_rate(v->dec) / (double)g.rate) *
                  (double)v->pitch;
    if (step <= 0.0) {
        step = 1.0;
    }
    int dst_max = (int)((double)EFX_STREAM_SRC_CHUNK / step) + 8;
    while (ring_free(v) >= dst_max && !v->ended) {
        uint64_t got = efx_decoder_read_stereo(v->dec, v->src_buf,
                                               EFX_STREAM_SRC_CHUNK);
        if (got == 0) {
            if (v->loop) {
                efx_decoder_rewind(v->dec);
                v->spos = 0.0;
                v->have_carry = 0;
                continue;
            }
            v->ended = 1;
            break;
        }
        int m = 0;
        double pos = v->spos;
        while (pos < (double)got - 1.0 && m < dst_max) {
            int idx = (int)floor(pos);
            float f = (float)(pos - (double)idx);
            float a_l;
            float a_r;
            if (idx < 0) {
                a_l = v->carry_l;
                a_r = v->carry_r;
            } else {
                a_l = v->src_buf[idx * 2];
                a_r = v->src_buf[idx * 2 + 1];
            }
            float b_l = v->src_buf[(idx + 1) * 2];
            float b_r = v->src_buf[(idx + 1) * 2 + 1];
            v->dst_buf[m * 2] = a_l + (b_l - a_l) * f;
            v->dst_buf[m * 2 + 1] = a_r + (b_r - a_r) * f;
            m++;
            pos += step;
        }
        v->spos = pos - (double)got;
        v->carry_l = v->src_buf[(got - 1) * 2];
        v->carry_r = v->src_buf[(got - 1) * 2 + 1];
        v->have_carry = 1;
        for (int i = 0; i < m; i++) {
            ring_push(v, v->dst_buf[i * 2], v->dst_buf[i * 2 + 1]);
        }
    }
}

void efx_audio_pump(void) {
    ensure_init();
    for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
        efx_voice *v = &g.voices[i];
        if (v->active && v->is_stream && !v->paused && !v->ended) {
            stream_fill(v);
        }
    }
}

/* ------------------------------------------------------------------ master */

void efx_audio_set_master_volume(float v) {
    ensure_init();
    g.master = clampf(v, 0.0f, 16.0f);
}

float efx_audio_master_volume(void) {
    ensure_init();
    return g.master;
}

/* -------------------------------------------------------------------- mix */

void efx_audio_mix(float *out, int frames) {
    if (!out || frames <= 0) {
        return;
    }
    ensure_init();
    if (!g.available) {
        memset(out, 0, (size_t)frames * 2 * sizeof(float));
        return;
    }
    for (int i = 0; i < frames; i++) {
        float l = 0.0f;
        float r = 0.0f;
        for (int vi = 0; vi < EFX_AUDIO_MAX_VOICES; vi++) {
            efx_voice *v = &g.voices[vi];
            if (!v->active || v->paused) {
                continue;
            }
            float sl = 0.0f;
            float sr = 0.0f;
            if (v->is_stream) {
                float ml;
                float mr;
                if (!ring_pop(v, &ml, &mr)) {
                    if (v->ended) {
                        voice_clear(v); /* drained after end */
                    }
                    continue; /* underrun: silence for this frame */
                }
                sl = ml;
                sr = mr;
            } else {
                uint64_t n = v->data->frames;
                if (v->pos >= (double)n - 1.0) {
                    if (v->loop && n > 0) {
                        v->pos = fmod(v->pos, (double)n);
                    } else {
                        voice_clear(v);
                        continue;
                    }
                }
                double step = ((double)v->data->rate / (double)g.rate) *
                              (double)v->pitch;
                if (step <= 0.0) {
                    step = 1.0;
                }
                int idx = (int)v->pos;
                float f = (float)(v->pos - (double)idx);
                const float *pcm = v->data->pcm;
                float a_l = pcm[idx * 2];
                float a_r = pcm[idx * 2 + 1];
                float b_l = pcm[(idx + 1) * 2];
                float b_r = pcm[(idx + 1) * 2 + 1];
                sl = a_l + (b_l - a_l) * f;
                sr = a_r + (b_r - a_r) * f;
                v->pos += step;
            }
            float gl;
            float gr;
            voice_gain(v->pan, &gl, &gr);
            l += sl * v->volume * gl;
            r += sr * v->volume * gr;
        }
        out[i * 2] = clampf(l * g.master, -1.0f, 1.0f);
        out[i * 2 + 1] = clampf(r * g.master, -1.0f, 1.0f);
    }
}
