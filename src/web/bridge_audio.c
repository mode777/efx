#include "bridge_internal.h"

/* ================================================= F14 audio bridge */

typedef struct {
    efx_audio_data *data;
    int alive;
} web_adata_slot;

typedef struct {
    efx_audio_stream *stream;
    int alive;
} web_astream_slot;

typedef struct {
    int voice;
    long long serial;
    int alive;
} web_audio_slot;

static struct {
    web_adata_slot *slots;
    int count;
    int cap;
} WDATA;

static struct {
    web_astream_slot *slots;
    int count;
    int cap;
} WSTREAM;

static struct {
    web_audio_slot *slots;
    int count;
    int cap;
} WAUDIO;

static web_adata_slot *web_adata_get(int id) {
    if (id <= 0 || id > WDATA.count) return NULL;
    return &WDATA.slots[id - 1];
}

static web_astream_slot *web_astream_get(int id) {
    if (id <= 0 || id > WSTREAM.count) return NULL;
    return &WSTREAM.slots[id - 1];
}

static web_audio_slot *web_audio_get(int id) {
    if (id <= 0 || id > WAUDIO.count) return NULL;
    return &WAUDIO.slots[id - 1];
}

static int web_data_push(efx_audio_data *data) {
    if (WDATA.count == WDATA.cap) {
        int ncap = WDATA.cap ? WDATA.cap * 2 : 8;
        web_adata_slot *ns = realloc(WDATA.slots, (size_t)ncap * sizeof(*ns));
        if (!ns) return 0;
        WDATA.slots = ns;
        WDATA.cap = ncap;
    }
    WDATA.slots[WDATA.count].data = data;
    WDATA.slots[WDATA.count].alive = 1;
    return ++WDATA.count;
}

static int web_stream_push(efx_audio_stream *stream) {
    if (WSTREAM.count == WSTREAM.cap) {
        int ncap = WSTREAM.cap ? WSTREAM.cap * 2 : 8;
        web_astream_slot *ns =
            realloc(WSTREAM.slots, (size_t)ncap * sizeof(*ns));
        if (!ns) return 0;
        WSTREAM.slots = ns;
        WSTREAM.cap = ncap;
    }
    WSTREAM.slots[WSTREAM.count].stream = stream;
    WSTREAM.slots[WSTREAM.count].alive = 1;
    return ++WSTREAM.count;
}

static int web_audio_push(int voice) {
    if (WAUDIO.count == WAUDIO.cap) {
        int ncap = WAUDIO.cap ? WAUDIO.cap * 2 : 8;
        web_audio_slot *ns = realloc(WAUDIO.slots, (size_t)ncap * sizeof(*ns));
        if (!ns) return 0;
        WAUDIO.slots = ns;
        WAUDIO.cap = ncap;
    }
    WAUDIO.slots[WAUDIO.count].voice = voice;
    WAUDIO.slots[WAUDIO.count].serial = efx_audio_voice_serial(voice);
    WAUDIO.slots[WAUDIO.count].alive = 1;
    return ++WAUDIO.count;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_load_data(const char *path) {
    if (!W.resource || !path) return 0;
    size_t n = 0;
    int e = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(W.resource, path, &n, &e);
    /* negative codes are loader failures the prelude maps to messages
     * (ADR 0049 D4): -1 unreadable, -2 undecodable */
    if (!bytes) return -1;
    int de = 0;
    efx_audio_data *data = efx_audio_data_load(bytes, n, &de);
    efx_resource_free(bytes);
    if (!data) return -2;
    int id = web_data_push(data);
    if (!id) {
        efx_audio_data_release(data);
        return -2;
    }
    return id;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_load_stream(const char *path) {
    if (!W.resource || !path) return 0;
    size_t n = 0;
    int e = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(W.resource, path, &n, &e);
    if (!bytes) return -1;
    int de = 0;
    efx_audio_stream *stream = efx_audio_stream_load(bytes, n, &de);
    efx_resource_free(bytes);
    if (!stream) return -2;
    int id = web_stream_push(stream);
    if (!id) {
        efx_audio_stream_release(stream);
        return -2;
    }
    return id;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_data_destroy(int id) {
    web_adata_slot *s = web_adata_get(id);
    if (!s || !s->alive) return;
    s->alive = 0;
    if (s->data) {
        efx_audio_data_release(s->data);
        s->data = NULL;
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_stream_destroy(int id) {
    web_astream_slot *s = web_astream_get(id);
    if (!s || !s->alive) return;
    s->alive = 0;
    if (s->stream) {
        efx_audio_stream_release(s->stream);
        s->stream = NULL;
    }
}

static int web_audio_play(int voice) {
    if (voice < 0) return 0;
    int id = web_audio_push(voice);
    if (!id) {
        efx_audio_stop_voice(voice);
    }
    return id;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_play_data(int data_id, float volume,
                                                    float pan, float pitch,
                                                    int loop) {
    web_adata_slot *d = web_adata_get(data_id);
    if (!d || !d->alive || !d->data) return 0;
    return web_audio_play(
        efx_audio_play_data(d->data, volume, pan, pitch, loop));
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_play_stream(int stream_id,
                                                      float volume, float pan,
                                                      float pitch, int loop) {
    web_astream_slot *s = web_astream_get(stream_id);
    if (!s || !s->alive || !s->stream) return 0;
    return web_audio_play(
        efx_audio_play_stream(s->stream, volume, pan, pitch, loop));
}

static int web_audio_live(const web_audio_slot *a) {
    return a->voice >= 0 && efx_audio_voice_serial(a->voice) == a->serial;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_handle_playing(int id) {
    web_audio_slot *a = web_audio_get(id);
    if (!a || !a->alive) return 0;
    return web_audio_live(a) && efx_audio_voice_playing(a->voice) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_handle_paused(int id) {
    web_audio_slot *a = web_audio_get(id);
    if (!a || !a->alive) return 0;
    return web_audio_live(a) && efx_audio_voice_paused(a->voice) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_stop(int id) {
    web_audio_slot *a = web_audio_get(id);
    if (!a || !a->alive) return;
    if (web_audio_live(a)) {
        efx_audio_stop_voice(a->voice);
    }
    a->voice = -1;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_pause(int id, int paused) {
    web_audio_slot *a = web_audio_get(id);
    if (a && a->alive && web_audio_live(a)) {
        efx_audio_set_voice_paused(a->voice, paused);
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_destroy(int id) {
    web_audio_slot *a = web_audio_get(id);
    if (!a || !a->alive) return;
    a->alive = 0;
    if (web_audio_live(a)) {
        efx_audio_stop_voice(a->voice);
    }
    a->voice = -1;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_set_volume(int id, float v) {
    web_audio_slot *a = web_audio_get(id);
    if (a && a->alive && web_audio_live(a)) {
        efx_audio_set_voice_volume(a->voice, v);
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_set_pan(int id, float v) {
    web_audio_slot *a = web_audio_get(id);
    if (a && a->alive && web_audio_live(a)) {
        efx_audio_set_voice_pan(a->voice, v);
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_set_pitch(int id, float v) {
    web_audio_slot *a = web_audio_get(id);
    if (a && a->alive && web_audio_live(a)) {
        efx_audio_set_voice_pitch(a->voice, v);
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_set_loop(int id, int loop) {
    web_audio_slot *a = web_audio_get(id);
    if (a && a->alive && web_audio_live(a)) {
        efx_audio_set_voice_loop(a->voice, loop);
    }
}


