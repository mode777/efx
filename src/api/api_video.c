#include "api/api_internal.h"


/* ------------------------------------------------- video-playback bindings */

/* host-state registry of live videos (borrowed links, ADR 0057 sweep) */

static efxjs_video **video_list(struct efx_host_state *h) {
    return (efxjs_video **)&h->videos;
}

void efx_api_videos_tick(struct efx_host_state *h, double dt) {
    efxjs_video *w = *video_list(h);
    while (w) {
        if (w->alive && w->v) {
            int frame = 0;
            if (efx_video_advance(w->v, dt, &frame) == EFX_VIDEO_OK &&
                frame && w->texture) {
                efx_render_texture_update(w->texture,
                                          efx_video_frame_rgba(w->v));
            }
        }
        w = w->next;
    }
}

void efx_api_videos_release(struct efx_host_state *h) {
    efxjs_video *w = *video_list(h);
    while (w) {
        efxjs_video *next = w->next;
        if (w->alive) {
            /* native release only: the JS wrappers are going away with the
               context (game swap); texture + voice + decoder all engine-side */
            if (w->voice >= 0) {
                efx_audio_stop_voice(w->voice);
                w->voice = -1;
            }
            if (w->texture) {
                efx_render_texture_destroy(w->texture);
                w->texture = 0;
            }
            efx_video_destroy(w->v);
            w->v = NULL;
            efx_resource_free(w->bytes);
            w->bytes = NULL;
            w->alive = 0;
        }
        w = next;
    }
    *video_list(h) = NULL;
}

static void video_register(struct efx_host_state *h, efxjs_video *w) {
    w->host = h;
    w->next = *video_list(h);
    *video_list(h) = w;
}

static void video_unregister(efxjs_video *w) {
    if (!w->host) {
        return;
    }
    efxjs_video **pp = video_list(w->host);
    while (*pp) {
        if (*pp == w) {
            *pp = w->next;
            break;
        }
        pp = &(*pp)->next;
    }
    w->host = NULL;
}

/* release the engine-side resources of a live video (destroy + game swap) */
static void video_release_native(efxjs_video *w) {
    if (w->voice >= 0) {
        efx_audio_stop_voice(w->voice);
        w->voice = -1;
    }
    if (w->texture) {
        efx_render_texture_destroy(w->texture);
        w->texture = 0;
    }
    efx_video_destroy(w->v);
    w->v = NULL;
    efx_resource_free(w->bytes);
    w->bytes = NULL;
}

void video_release(void *p) {
    efxjs_video *w = (efxjs_video *)p;
    if (w->alive) {
        video_unregister(w);
        video_release_native(w);
        w->alive = 0;
    }
}

JSValue video_destroy(JSContext *ctx, void *p) {
    (void)ctx;
    video_release(p);
    return JS_UNDEFINED;
}

static efxjs_video *get_live_video(JSContext *ctx, JSValueConst v) {
    void *p = JS_GetOpaque2(ctx, v, video_class_id);
    if (!p) {
        efx_api_type_error(ctx, "expected a Video");
        return NULL;
    }
    efxjs_video *w = (efxjs_video *)p;
    if (!w->alive) {
        efx_api_type_error(ctx, "using a destroyed resource");
        return NULL;
    }
    return w;
}


/* ---- audio coupling: implicit engine-managed pull-voice (design D5) ---- */

/* audio-core shim: the pull contract is void*-typed, module walls intact */
static uint64_t video_audio_pull(void *ud, float *out, uint64_t frames) {
    return efx_video_pull_audio((efx_video *)ud, out, frames);
}

static void video_voice_attach(efxjs_video *w) {
    if (!efx_video_has_audio(w->v)) {
        return;
    }
    if (w->voice < 0) {
        /* cap/steal policy decides; -1 plays silent (spec) */
        w->voice = efx_audio_play_pull(video_audio_pull, w->v,
                                       efx_video_audio_rate(w->v),
                                       w->muted ? 0.0f : 1.0f);
        if (w->voice < 0) {
            return;
        }
    }
    efx_audio_set_voice_paused(w->voice, 0);
    efx_audio_set_voice_volume(w->voice, w->muted ? 0.0f : 1.0f);
}

static void video_voice_pause(efxjs_video *w) {
    if (w->voice >= 0) {
        efx_audio_set_voice_paused(w->voice, 1);
    }
}


/* ---- prototype members (native-validated: hot playback path, ADR 0049) -- */

static JSValue efx_js_video_play(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    efx_video_set_playing(w->v, 1);
    video_voice_attach(w);
    return JS_UNDEFINED;
}

static JSValue efx_js_video_pause(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    efx_video_set_playing(w->v, 0);
    video_voice_pause(w);
    return JS_UNDEFINED;
}

static JSValue efx_js_video_getTime(JSContext *ctx, JSValueConst this_val) {
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    return JS_NewFloat64(ctx, efx_video_time(w->v));
}

static JSValue efx_js_video_getDuration(JSContext *ctx, JSValueConst this_val) {
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    return JS_NewFloat64(ctx, efx_video_duration(w->v));
}

static JSValue efx_js_video_getWidth(JSContext *ctx, JSValueConst this_val) {
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    return JS_NewInt32(ctx, efx_video_width(w->v));
}

static JSValue efx_js_video_getHeight(JSContext *ctx, JSValueConst this_val) {
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    return JS_NewInt32(ctx, efx_video_height(w->v));
}

static JSValue efx_js_video_getEnded(JSContext *ctx, JSValueConst this_val) {
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    return JS_NewBool(ctx, efx_video_ended(w->v));
}

static JSValue efx_js_video_getLoop(JSContext *ctx, JSValueConst this_val) {
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    return JS_NewBool(ctx, efx_video_loop(w->v));
}

static JSValue efx_js_video_setLoop(JSContext *ctx, JSValueConst this_val,
                                    JSValueConst val) {
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    int b = JS_ToBool(ctx, val);
    if (b < 0) {
        return efx_api_type_error(ctx, "loop must be a boolean");
    }
    efx_video_set_loop(w->v, b);
    return JS_UNDEFINED;
}

static JSValue efx_js_video_getMuted(JSContext *ctx, JSValueConst this_val) {
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    return JS_NewBool(ctx, w->muted);
}

static JSValue efx_js_video_setMuted(JSContext *ctx, JSValueConst this_val,
                                     JSValueConst val) {
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    int b = JS_ToBool(ctx, val);
    if (b < 0) {
        return efx_api_type_error(ctx, "muted must be a boolean");
    }
    w->muted = b ? 1 : 0;
    if (w->voice >= 0) {
        efx_audio_set_voice_volume(w->voice, w->muted ? 0.0f : 1.0f);
    }
    return JS_UNDEFINED;
}

/* a fresh wrapper per read; engine-owned (permanent) so scripts cannot
 * destroy the video's texture directly and the GC backstop skips it — the
 * video releases the texture (design D4) */
static JSValue efx_js_video_getTexture(JSContext *ctx, JSValueConst this_val) {
    efxjs_video *w = get_live_video(ctx, this_val);
    if (!w) {
        return JS_EXCEPTION;
    }
    efxjs_texture *t = calloc(1, sizeof(*t));
    if (!t) {
        return efx_api_generic_error(ctx, "out of memory");
    }
    t->handle = w->texture;
    t->alive = 1;
    t->permanent = 1;
    t->dynamic = 1;
    t->owner = w;
    JSValue obj = JS_NewObjectClass(ctx, texture_class_id);
    JS_SetOpaque(obj, t);
    return obj;
}


const JSCFunctionListEntry video_proto_funcs[10] = {    JS_CFUNC_DEF("play", 0, efx_js_video_play),
    JS_CFUNC_DEF("pause", 0, efx_js_video_pause),
    JS_CGETSET_DEF("currentTime", efx_js_video_getTime, NULL),
    JS_CGETSET_DEF("duration", efx_js_video_getDuration, NULL),
    JS_CGETSET_DEF("width", efx_js_video_getWidth, NULL),
    JS_CGETSET_DEF("height", efx_js_video_getHeight, NULL),
    JS_CGETSET_DEF("ended", efx_js_video_getEnded, NULL),
    JS_CGETSET_DEF("loop", efx_js_video_getLoop, efx_js_video_setLoop),
    JS_CGETSET_DEF("muted", efx_js_video_getMuted, efx_js_video_setMuted),
    JS_CGETSET_DEF("texture", efx_js_video_getTexture, NULL),
};


/* (path, loop, muted) — resource bytes are decoded over in place and must
 * outlive the video: the decoded buffer is retained for the wrapper's
 * lifetime (freed at destroy/release, like glTF meshdata) */
JSValue efx_js_open_video_wire(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) {
        return efx_api_type_error(ctx, "openVideo wire native requires (path, loop, muted)");
    }
    struct efx_host_state *h = efx_api_host_state(ctx);
    if (!h->resource) {
        return efx_api_plain_error(ctx, "no resource root");
    }
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) {
        return JS_EXCEPTION;
    }
    int loop = JS_ToBool(ctx, argv[1]);
    int muted = JS_ToBool(ctx, argv[2]);
    JS_FreeCString(ctx, path);
    if (loop < 0 || muted < 0) {
        return JS_EXCEPTION;
    }
    size_t size = 0;
    int err = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(h->resource, path, &size, &err);
    if (!bytes) {
        return JS_NewInt32(ctx, -err);
    }
    efx_video *v = NULL;
    int rc = efx_video_open(bytes, size, &v);
    if (rc != EFX_VIDEO_OK) {
        efx_resource_free(bytes);
        /* -100: not a parseable MPEG-1 stream (distinct from resource
         * error codes), mirroring loadImage's decode-failure convention */
        return JS_NewInt32(ctx, rc == EFX_VIDEO_ERR_FORMAT ? -100 : -101);
    }
    /* the texture: engine-created dynamic texture holding the first frame */
    uint64_t tex = efx_render_texture_create(efx_video_width(v),
                                             efx_video_height(v),
                                             efx_video_frame_rgba(v),
                                             EFX_TEX_WRAP_REPEAT,
                                             EFX_FILTER_LINEAR, 0, 1);
    if (!tex) {
        efx_video_destroy(v);
        efx_resource_free(bytes);
        return efx_api_generic_error(ctx, "video texture creation failed");
    }
    efxjs_video *w = calloc(1, sizeof(*w));
    if (!w) {
        efx_render_texture_destroy(tex);
        efx_video_destroy(v);
        efx_resource_free(bytes);
        return efx_api_generic_error(ctx, "out of memory");
    }
    w->v = v;
    w->bytes = bytes; /* decoder reads in place; freed with the wrapper */
    w->texture = tex;
    w->voice = -1;
    w->muted = muted ? 1 : 0;
    w->alive = 1;
    efx_video_set_loop(v, loop);
    video_register(h, w);
    JSValue obj = JS_NewObjectClass(ctx, video_class_id);
    JS_SetOpaque(obj, w);
    return obj;
}


int efx_api_register_video(JSContext *ctx, JSValueConst efx) {
    JSValue video = JS_NewObject(ctx);
    /* openVideo is installed by the shared prelude (ADR 0049) */
    JS_SetPropertyStr(ctx, efx, "video", video);
    return 0;
}
