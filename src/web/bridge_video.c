#include "bridge_internal.h"

#include <stdlib.h>
#include <string.h>

/* video-playback (ADR 0063): one slot table, ids 1-based and never reused
 * (like ImageData); the resource buffer is owned and freed with the slot
 * (the decoder reads it in place). */
typedef struct {
    int used, alive;
    efx_video *v;
    uint8_t *bytes;
    uint64_t texture;
    int voice; /* audio pull-voice, -1 = none */
    int muted;
} vid_slot;

static struct {
    vid_slot *slots;
    int count, cap;
} VID;

static vid_slot *vid_get(double handle) {
    int id = (int)handle;
    if (id <= 0 || id > VID.count) {
        return NULL;
    }
    vid_slot *s = &VID.slots[id - 1];
    return s->alive ? s : NULL;
}

/* the audio-core pull shim: void*-typed contract (module walls) */
static uint64_t vid_audio_pull(void *ud, float *out, uint64_t frames) {
    return efx_video_pull_audio((efx_video *)ud, out, frames);
}

static void vid_voice_attach(vid_slot *s) {
    if (!efx_video_has_audio(s->v)) {
        return;
    }
    if (s->voice < 0) {
        /* cap/steal policy decides; -1 plays silent (spec) */
        s->voice = efx_audio_play_pull(vid_audio_pull, s->v,
                                       efx_video_audio_rate(s->v),
                                       s->muted ? 0.0f : 1.0f);
        if (s->voice < 0) {
            return;
        }
    }
    efx_audio_set_voice_paused(s->voice, 0);
    efx_audio_set_voice_volume(s->voice, s->muted ? 0.0f : 1.0f);
}

/* negative codes match the resource-loader convention the prelude maps
 * (ADR 0049 D4): -err for resource errors, -100 not-MPEG-1, 0 no memory */
EMSCRIPTEN_KEEPALIVE double efx_bridge_open_video(const char *path, int loop,
                                                  int muted) {
    if (!W.resource) {
        return -1;
    }
    size_t n = 0;
    int e = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(W.resource, path, &n, &e);
    if (!bytes) {
        return -e;
    }
    efx_video *v = NULL;
    int rc = efx_video_open(bytes, n, &v);
    if (rc != EFX_VIDEO_OK) {
        efx_resource_free(bytes);
        return rc == EFX_VIDEO_ERR_FORMAT ? -100 : -101;
    }
    uint64_t tex = efx_render_texture_create(efx_video_width(v),
                                             efx_video_height(v),
                                             efx_video_frame_rgba(v),
                                             EFX_TEX_WRAP_REPEAT,
                                             EFX_FILTER_LINEAR, 0, 1);
    if (!tex) {
        efx_video_destroy(v);
        efx_resource_free(bytes);
        return 0;
    }
    if (VID.count >= VID.cap) {
        int cap = VID.cap ? VID.cap * 2 : 8;
        vid_slot *grown = realloc(VID.slots, (size_t)cap * sizeof(vid_slot));
        if (!grown) {
            efx_render_texture_destroy(tex);
            efx_video_destroy(v);
            efx_resource_free(bytes);
            return 0;
        }
        VID.slots = grown;
        VID.cap = cap;
    }
    vid_slot *s = &VID.slots[VID.count++];
    memset(s, 0, sizeof(*s));
    s->used = 1;
    s->alive = 1;
    s->v = v;
    s->bytes = bytes;
    s->texture = tex;
    s->voice = -1;
    s->muted = muted ? 1 : 0;
    efx_video_set_loop(v, loop);
    return (double)VID.count;
}

static void vid_release(vid_slot *s) {
    if (s->voice >= 0) {
        efx_audio_stop_voice(s->voice);
        s->voice = -1;
    }
    if (s->texture) {
        efx_render_texture_destroy(s->texture);
        s->texture = 0;
    }
    efx_video_destroy(s->v);
    s->v = NULL;
    efx_resource_free(s->bytes);
    s->bytes = NULL;
    s->alive = 0;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_video_destroy(double handle) {
    vid_slot *s = vid_get(handle);
    if (s) {
        vid_release(s);
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_video_play(double handle) {
    vid_slot *s = vid_get(handle);
    if (!s) {
        return;
    }
    efx_video_set_playing(s->v, 1);
    vid_voice_attach(s);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_video_pause(double handle) {
    vid_slot *s = vid_get(handle);
    if (!s) {
        return;
    }
    efx_video_set_playing(s->v, 0);
    if (s->voice >= 0) {
        efx_audio_set_voice_paused(s->voice, 1);
    }
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_video_time(double handle) {
    vid_slot *s = vid_get(handle);
    return s ? efx_video_time(s->v) : 0.0;
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_video_duration(double handle) {
    vid_slot *s = vid_get(handle);
    return s ? efx_video_duration(s->v) : 0.0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_video_width(double handle) {
    vid_slot *s = vid_get(handle);
    return s ? efx_video_width(s->v) : 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_video_height(double handle) {
    vid_slot *s = vid_get(handle);
    return s ? efx_video_height(s->v) : 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_video_ended(double handle) {
    vid_slot *s = vid_get(handle);
    return s ? efx_video_ended(s->v) : 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_video_get_loop(double handle) {
    vid_slot *s = vid_get(handle);
    return s ? efx_video_loop(s->v) : 0;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_video_set_loop(double handle, int loop) {
    vid_slot *s = vid_get(handle);
    if (s) {
        efx_video_set_loop(s->v, loop);
    }
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_video_get_muted(double handle) {
    vid_slot *s = vid_get(handle);
    return s ? s->muted : 0;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_video_set_muted(double handle,
                                                     int muted) {
    vid_slot *s = vid_get(handle);
    if (!s) {
        return;
    }
    s->muted = muted ? 1 : 0;
    if (s->voice >= 0) {
        efx_audio_set_voice_volume(s->voice, s->muted ? 0.0f : 1.0f);
    }
}

/* the engine-owned dynamic texture; permanent flag on the JS side blocks
 * script destroy and the GC backstop — the video releases it (design D4) */
EMSCRIPTEN_KEEPALIVE double efx_bridge_video_texture(double handle) {
    vid_slot *s = vid_get(handle);
    return s ? (double)s->texture : 0.0;
}

/* engine-frame tick, before the update hooks (mirrors the desktop path) */
void efx_bridge_video_tick(double dt) {
    for (int i = 0; i < VID.count; i++) {
        vid_slot *s = &VID.slots[i];
        if (!s->alive) {
            continue;
        }
        int frame = 0;
        if (efx_video_advance(s->v, dt, &frame) == EFX_VIDEO_OK && frame &&
            s->texture) {
            efx_render_texture_update(s->texture, efx_video_frame_rgba(s->v));
        }
    }
}
