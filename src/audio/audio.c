#include "audio/audio.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "audio/decode.h"

/* streaming decode chunk (source frames) and a device-rate output bound that
 * always fits one resampled chunk */
#define EFX_MUSIC_SRC_CHUNK 2048

typedef struct efx_voice {
    int active;
    int loop;
    const efx_sound_data *sound;
    double pos; /* fractional source frame index */
    float volume;
    float pan;
    float pitch;
    long long start_seq;
} efx_voice;

struct efx_sound_data {
    int refs;
    int rate;
    uint64_t frames;
    float *pcm; /* stereo interleaved, frames*2 */
};

typedef struct efx_music {
    int loaded;
    int paused;
    int loop;
    int ended;
    float volume;
    uint8_t *bytes; /* owned copy of the compressed stream */
    size_t size;
    efx_decoder *dec;
    /* device-rate stereo ring (~1s) */
    float *ring;
    int ring_cap;
    int ring_head;
    int ring_count;
    /* source-rate -> device-rate linear resampler state */
    double pos;
    int have_carry;
    float carry_l;
    float carry_r;
    float *src_buf;
    float *dst_buf;
} efx_music;

static struct {
    int initialized;
    int rate;
    int available;
    efx_voice voices[EFX_AUDIO_MAX_VOICES];
    efx_music music;
    long long seq;
    efx_audio_resume_fn resume_fn;
    void *resume_ud;
} g;

/* ------------------------------------------------------------------ music */

static void music_free(void) {
    if (g.music.dec) {
        efx_decoder_close(g.music.dec);
        g.music.dec = NULL;
    }
    free(g.music.bytes);
    g.music.bytes = NULL;
    g.music.size = 0;
    free(g.music.ring);
    g.music.ring = NULL;
    g.music.ring_cap = 0;
    g.music.ring_head = 0;
    g.music.ring_count = 0;
    free(g.music.src_buf);
    g.music.src_buf = NULL;
    free(g.music.dst_buf);
    g.music.dst_buf = NULL;
    g.music.loaded = 0;
    g.music.paused = 0;
    g.music.ended = 0;
}

static int music_ring_free(void) {
    return g.music.ring_cap - g.music.ring_count;
}

static void music_ring_push(float l, float r) {
    int idx = (g.music.ring_head + g.music.ring_count) % g.music.ring_cap;
    g.music.ring[idx * 2] = l;
    g.music.ring[idx * 2 + 1] = r;
    g.music.ring_count++;
}

static int music_ring_pop(float *l, float *r) {
    if (g.music.ring_count <= 0) {
        return 0;
    }
    int idx = g.music.ring_head;
    *l = g.music.ring[idx * 2];
    *r = g.music.ring[idx * 2 + 1];
    g.music.ring_head = (idx + 1) % g.music.ring_cap;
    g.music.ring_count--;
    return 1;
}

/* ------------------------------------------------------------------ voices */

static void voice_gain(float pan, float *gl, float *gr) {
    float p = pan;
    if (p < -1.0f) {
        p = -1.0f;
    }
    if (p > 1.0f) {
        p = 1.0f;
    }
    *gl = (p <= 0.0f) ? 1.0f : 1.0f - p;
    *gr = (p >= 0.0f) ? 1.0f : 1.0f + p;
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

/* ------------------------------------------------------------------- life */

void efx_audio_shutdown(void) {
    for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
        if (g.voices[i].active && g.voices[i].sound) {
            efx_audio_sound_data_release(
                (efx_sound_data *)g.voices[i].sound);
        }
        g.voices[i].active = 0;
        g.voices[i].sound = NULL;
    }
    music_free();
    g.initialized = 0;
}

void efx_audio_init(int sample_rate) {
    efx_audio_shutdown();
    if (sample_rate <= 0) {
        sample_rate = EFX_AUDIO_DEFAULT_RATE;
    }
    memset(g.voices, 0, sizeof(g.voices));
    memset(&g.music, 0, sizeof(g.music));
    g.rate = sample_rate;
    g.available = 1;
    g.seq = 0;
    g.resume_fn = NULL;
    g.resume_ud = NULL;
    g.initialized = 1;
}

int efx_audio_sample_rate(void) {
    if (!g.initialized) {
        efx_audio_init(EFX_AUDIO_DEFAULT_RATE);
    }
    return g.rate;
}

/* Headless/`--script` builds never run the platform backend, so any public
 * entry point lazily initializes a device-free core (available = 1) to keep
 * the API usable in script tests. The windowed player initializes the backend
 * with the real device rate before scripts run. */
static void ensure_init(void) {
    if (!g.initialized) {
        efx_audio_init(EFX_AUDIO_DEFAULT_RATE);
    }
}

void efx_audio_set_available(int available) {
    ensure_init();
    g.available = available ? 1 : 0;
}

int efx_audio_available(void) {
    ensure_init();
    return g.available;
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

/* ------------------------------------------------------------- sound data */

efx_sound_data *efx_audio_sound_data_load(const uint8_t *bytes, size_t size,
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
        cap = EFX_MUSIC_SRC_CHUNK;
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
        if (total + EFX_MUSIC_SRC_CHUNK > cap) {
            uint64_t ncap = cap * 2;
            float *np =
                realloc(pcm, (size_t)ncap * 2 * sizeof(float));
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
        if (want > EFX_MUSIC_SRC_CHUNK) {
            want = EFX_MUSIC_SRC_CHUNK;
        }
        uint64_t got = efx_decoder_read_stereo(dec, pcm + total * 2, want);
        total += got;
        if (got == 0) {
            break;
        }
    }
    efx_decoder_close(dec);
    efx_sound_data *s = calloc(1, sizeof(*s));
    if (!s) {
        free(pcm);
        if (err) {
            *err = EFX_AUDIO_ERR_NOMEM;
        }
        return NULL;
    }
    s->refs = 1;
    s->rate = (int)rate;
    s->frames = total;
    s->pcm = pcm;
    if (err) {
        *err = EFX_AUDIO_OK;
    }
    return s;
}

void efx_audio_sound_data_retain(efx_sound_data *s) {
    if (s) {
        s->refs++;
    }
}

void efx_audio_sound_data_release(efx_sound_data *s) {
    if (!s) {
        return;
    }
    s->refs--;
    if (s->refs <= 0) {
        free(s->pcm);
        free(s);
    }
}

int efx_audio_sound_data_rate(const efx_sound_data *s) {
    return s ? s->rate : 0;
}

uint64_t efx_audio_sound_data_frames(const efx_sound_data *s) {
    return s ? s->frames : 0;
}

/* ------------------------------------------------------------------ voices */

int efx_audio_play_effect(const efx_sound_data *s, float volume, float pan,
                          float pitch, int loop) {
    ensure_init();
    if (!s || s->frames == 0) {
        return -1;
    }
    if (!g.available) {
        return -1;
    }
    int slot = -1;
    for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
        if (!g.voices[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        /* steal the quietest non-looping voice (oldest breaks ties); if every
         * voice is looping, reject deterministically */
        int best = -1;
        for (int i = 0; i < EFX_AUDIO_MAX_VOICES; i++) {
            if (g.voices[i].loop) {
                continue;
            }
            if (best < 0 || g.voices[i].volume < g.voices[best].volume ||
                (g.voices[i].volume == g.voices[best].volume &&
                 g.voices[i].start_seq < g.voices[best].start_seq)) {
                best = i;
            }
        }
        if (best < 0) {
            return -1;
        }
        slot = best;
        if (g.voices[slot].sound) {
            efx_audio_sound_data_release(
                (efx_sound_data *)g.voices[slot].sound);
        }
    }
    efx_voice *v = &g.voices[slot];
    efx_audio_sound_data_retain((efx_sound_data *)s);
    v->active = 1;
    v->loop = loop ? 1 : 0;
    v->sound = s;
    v->pos = 0.0;
    v->volume = clampf(volume, 0.0f, 16.0f);
    v->pan = pan;
    v->pitch = (pitch > 0.0f) ? pitch : 1.0f;
    v->start_seq = g.seq++;
    return slot;
}

void efx_audio_stop_voice(int voice) {
    if (voice < 0 || voice >= EFX_AUDIO_MAX_VOICES) {
        return;
    }
    efx_voice *v = &g.voices[voice];
    if (v->active && v->sound) {
        efx_audio_sound_data_release((efx_sound_data *)v->sound);
    }
    v->active = 0;
    v->sound = NULL;
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

int efx_audio_voice_playing(int voice) {
    if (voice < 0 || voice >= EFX_AUDIO_MAX_VOICES) {
        return 0;
    }
    return g.voices[voice].active ? 1 : 0;
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

/* ------------------------------------------------------------------- music */

int efx_audio_play_music(const uint8_t *bytes, size_t size, float volume,
                         int loop, int *err) {
    ensure_init();
    if (!bytes || size == 0) {
        if (err) {
            *err = EFX_AUDIO_ERR_ARG;
        }
        return 1;
    }
    music_free();
    uint8_t *copy = malloc(size);
    if (!copy) {
        if (err) {
            *err = EFX_AUDIO_ERR_NOMEM;
        }
        return 1;
    }
    memcpy(copy, bytes, size);
    efx_decoder *dec = NULL;
    if (efx_decoder_open(copy, size, &dec) != 0) {
        free(copy);
        if (err) {
            *err = EFX_AUDIO_ERR_FORMAT;
        }
        return 1;
    }
    int cap = g.rate > 0 ? g.rate : EFX_AUDIO_DEFAULT_RATE;
    if (cap < 1024) {
        cap = 1024;
    }
    float *ring = calloc((size_t)cap * 2, sizeof(float));
    float *src = malloc((size_t)EFX_MUSIC_SRC_CHUNK * 2 * sizeof(float));
    float *dst = malloc((size_t)(EFX_MUSIC_SRC_CHUNK * 2 + 8) * 2 *
                        sizeof(float));
    if (!ring || !src || !dst) {
        free(ring);
        free(src);
        free(dst);
        efx_decoder_close(dec);
        free(copy);
        if (err) {
            *err = EFX_AUDIO_ERR_NOMEM;
        }
        return 1;
    }
    g.music.bytes = copy;
    g.music.size = size;
    g.music.dec = dec;
    g.music.ring = ring;
    g.music.ring_cap = cap;
    g.music.ring_head = 0;
    g.music.ring_count = 0;
    g.music.src_buf = src;
    g.music.dst_buf = dst;
    g.music.loaded = 1;
    g.music.paused = 0;
    g.music.loop = loop ? 1 : 0;
    g.music.ended = 0;
    g.music.volume = clampf(volume, 0.0f, 16.0f);
    g.music.pos = 0.0;
    g.music.have_carry = 0;
    g.music.carry_l = 0.0f;
    g.music.carry_r = 0.0f;
    if (err) {
        *err = EFX_AUDIO_OK;
    }
    return 0;
}

void efx_audio_stop_music(void) {
    music_free();
}

void efx_audio_pause_music(int paused) {
    if (g.music.loaded) {
        g.music.paused = paused ? 1 : 0;
    }
}

void efx_audio_set_music_volume(float v) {
    g.music.volume = clampf(v, 0.0f, 16.0f);
}

int efx_audio_music_playing(void) {
    if (!g.available || !g.music.loaded || g.music.paused) {
        return 0;
    }
    if (g.music.ended && g.music.ring_count == 0) {
        return 0;
    }
    return 1;
}

int efx_audio_music_paused(void) {
    return (g.music.loaded && g.music.paused) ? 1 : 0;
}

void efx_audio_music_pump(void) {
    ensure_init();
    if (!g.music.loaded || g.music.paused ||
        g.music.ended) {
        return;
    }
    double step = (double)efx_decoder_rate(g.music.dec) / (double)g.rate;
    if (step <= 0.0) {
        step = 1.0;
    }
    int dst_max = (int)((double)EFX_MUSIC_SRC_CHUNK / step) + 8;
    while (music_ring_free() >= dst_max && !g.music.ended) {
        uint64_t got = efx_decoder_read_stereo(
            g.music.dec, g.music.src_buf, EFX_MUSIC_SRC_CHUNK);
        if (got == 0) {
            if (g.music.loop) {
                efx_decoder_rewind(g.music.dec);
                g.music.pos = 0.0;
                g.music.have_carry = 0;
                continue;
            }
            g.music.ended = 1;
            break;
        }
        int m = 0;
        double pos = g.music.pos;
        while (pos < (double)got - 1.0 && m < dst_max) {
            int idx = (int)floor(pos);
            float f = (float)(pos - (double)idx);
            float a_l;
            float a_r;
            if (idx < 0) {
                a_l = g.music.carry_l;
                a_r = g.music.carry_r;
            } else {
                a_l = g.music.src_buf[idx * 2];
                a_r = g.music.src_buf[idx * 2 + 1];
            }
            float b_l = g.music.src_buf[(idx + 1) * 2];
            float b_r = g.music.src_buf[(idx + 1) * 2 + 1];
            g.music.dst_buf[m * 2] = a_l + (b_l - a_l) * f;
            g.music.dst_buf[m * 2 + 1] = a_r + (b_r - a_r) * f;
            m++;
            pos += step;
        }
        g.music.pos = pos - (double)got;
        g.music.carry_l = g.music.src_buf[(got - 1) * 2];
        g.music.carry_r = g.music.src_buf[(got - 1) * 2 + 1];
        g.music.have_carry = 1;
        for (int i = 0; i < m; i++) {
            music_ring_push(g.music.dst_buf[i * 2], g.music.dst_buf[i * 2 + 1]);
        }
    }
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
            if (!v->active) {
                continue;
            }
            uint64_t n = v->sound->frames;
            if (v->pos >= (double)n - 1.0) {
                if (v->loop && n > 0) {
                    v->pos = fmod(v->pos, (double)n);
                } else {
                    efx_audio_sound_data_release(
                        (efx_sound_data *)v->sound);
                    v->active = 0;
                    v->sound = NULL;
                    continue;
                }
            }
            double step =
                ((double)v->sound->rate / (double)g.rate) * (double)v->pitch;
            if (step <= 0.0) {
                step = 1.0;
            }
            int idx = (int)v->pos;
            float f = (float)(v->pos - (double)idx);
            const float *pcm = v->sound->pcm;
            float a_l = pcm[idx * 2];
            float a_r = pcm[idx * 2 + 1];
            float b_l = pcm[(idx + 1) * 2];
            float b_r = pcm[(idx + 1) * 2 + 1];
            float sl = a_l + (b_l - a_l) * f;
            float sr = a_r + (b_r - a_r) * f;
            float gl;
            float gr;
            voice_gain(v->pan, &gl, &gr);
            l += sl * v->volume * gl;
            r += sr * v->volume * gr;
            v->pos += step;
        }
        if (g.music.loaded && !g.music.paused) {
            float ml;
            float mr;
            if (music_ring_pop(&ml, &mr)) {
                l += ml * g.music.volume;
                r += mr * g.music.volume;
            }
        }
        out[i * 2] = clampf(l, -1.0f, 1.0f);
        out[i * 2 + 1] = clampf(r, -1.0f, 1.0f);
    }
}
