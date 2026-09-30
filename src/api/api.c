#include "api/api.h"
#include "runtime/runtime.h"
#include "runtime/runtime_internal.h"
#include "audio/audio.h"
#include "input/efx_input.h"
#include "input/efx_gamepad.h"
#include "physics/physics.h"
#include "physics/broadphase.h"
#include "render/render.h"
#include "render/text.h"
#include "resource/gltf.h"
#include "resource/image.h"
#include "resource/resource.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct efx_host_state *host_state(JSContext *ctx) {
    return (struct efx_host_state *)JS_GetContextOpaque(ctx);
}

void efx_log(const char *msg) {
    fprintf(stdout, "%s\n", msg ? msg : "");
    fflush(stdout);
}

JSValue efx_js_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    const char *s = NULL;
    if (argc > 0) {
        s = JS_ToCString(ctx, argv[0]);
    }
    fprintf(stdout, "%s\n", s ? s : "");
    if (s) {
        JS_FreeCString(ctx, s);
    }
    fflush(stdout);
    return JS_UNDEFINED;
}

JSValue efx_js_quit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    struct efx_host_state *h = host_state(ctx);
    int32_t code = 0;
    if (argc > 0) {
        JS_ToInt32(ctx, &code, argv[0]);
    }
    h->quit_requested = 1;
    h->quit_code = (int)code;
    return JS_Throw(ctx, JS_DupValue(ctx, h->quit_sentinel));
}

JSValue efx_js_args(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    struct efx_host_state *h = host_state(ctx);
    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < h->arg_count; i++) {
        char idx[16];
        snprintf(idx, sizeof(idx), "%d", i);
        JS_SetPropertyStr(ctx, arr, idx, JS_NewString(ctx, h->args[i]));
    }
    return arr;
}

/* ------------------------------------------------------------ helpers */

static JSValue type_error(JSContext *ctx, const char *msg) {
    return JS_ThrowTypeError(ctx, "%s", msg);
}

static JSValue range_error(JSContext *ctx, const char *msg) {
    return JS_ThrowRangeError(ctx, "%s", msg);
}

static JSValue generic_error(JSContext *ctx, const char *msg) {
    return JS_ThrowInternalError(ctx, "%s", msg);
}

/* reject unknown fields on an object with a TypeError naming the field
 * (defined with the F3 bindings; used from F2/F5a too) */
static int check_known_fields(JSContext *ctx, JSValueConst obj,
                              const char **known, int nknown,
                              const char *where);

/* F11: live Texture/RenderTarget coercion (defined with the F5a bindings) */
static int get_live_sample(JSContext *ctx, JSValueConst v, uint64_t *out_handle);

/* --------------------------------------------- F1 lifecycle hooks */

/* unsubscribe closure: magic selects the host callback list (see
   EFX_HOOK_LIST_* in runtime_internal.h), func_data[0] carries the stable
   entry index (design D1/D2, extended by F9 input) */
static JSValue efx_js_unsubscribe(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic,
                                  JSValue *func_data) {
    (void)this_val;
    (void)argc;
    (void)argv;
    struct efx_host_state *h = host_state(ctx);
    struct efx_hook_list *list = efx_host_hook_list(h, magic);
    int32_t idx = -1;
    JS_ToInt32(ctx, &idx, func_data[0]);
    if (idx >= 0 && idx < list->count) {
        list->entries[idx].active = 0; /* idempotent: repeated calls are no-ops */
    }
    return JS_UNDEFINED;
}

static JSValue register_hook(JSContext *ctx, JSValueConst fn, int which) {
    if (!JS_IsFunction(ctx, fn)) {
        return type_error(ctx, "hook must be a function");
    }
    struct efx_host_state *h = host_state(ctx);
    struct efx_hook_list *list = efx_host_hook_list(h, which);
    int idx = efx_hooks_append(ctx, list, fn);
    if (idx < 0) {
        return generic_error(ctx, "out of memory");
    }
    JSValue data = JS_NewInt32(ctx, idx);
    JSValue unsub = JS_NewCFunctionData(ctx, efx_js_unsubscribe, 0, which, 1,
                                        &data);
    JS_FreeValue(ctx, data);
    return unsub;
}

JSValue efx_js_registerUpdateHook(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "registerUpdateHook requires a function");
    }
    return register_hook(ctx, argv[0], EFX_HOOK_LIST_UPDATE);
}

JSValue efx_js_registerRenderHook(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "registerRenderHook requires a function");
    }
    return register_hook(ctx, argv[0], EFX_HOOK_LIST_RENDER);
}

/* read a flat array (JS array or typed array) of exactly n floats;
   returns 0 ok, -1 wrong type (TypeError thrown), -2 wrong length or
   non-finite/out-of-range element (RangeError thrown) */
static int get_float_array(JSContext *ctx, JSValueConst v, float *out, int n) {
    uint8_t *bytes = NULL;
    size_t blen = 0;

    if (JS_IsArray(v)) {
        /* fall through to element loop */
    } else if ((bytes = JS_GetUint8Array(ctx, &blen, v)) != NULL) {
        if ((int)blen != n) {
            range_error(ctx, "wrong buffer length");
            return -2;
        }
        for (int i = 0; i < n; i++) {
            out[i] = (float)bytes[i];
        }
        return 0;
    } else {
        type_error(ctx, "expected an array");
        return -1;
    }

    JSValue lenv = JS_GetPropertyStr(ctx, v, "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    if (len != n) {
        range_error(ctx, "wrong array length");
        return -2;
    }
    for (int i = 0; i < n; i++) {
        double d;
        JSValue ev = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
        if (JS_ToFloat64(ctx, &d, ev) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, ev);
            range_error(ctx, "array elements must be finite numbers");
            return -2;
        }
        JS_FreeValue(ctx, ev);
        out[i] = (float)d;
    }
    return 0;
}

/* ------------------------------------------------- resource classes */

typedef struct {
    uint64_t handle;
    int alive;
    int permanent; /* engine-owned (white texture) */
} efxjs_texture;

typedef struct {
    uint8_t *pixels;
    int w, h;
    int alive;
} efxjs_imagedata;

typedef struct {
    efx_meshdata *md;
    int alive;
} efxjs_meshdata;

typedef struct efxjs_mesh {
    uint64_t handle;
    int alive;
} efxjs_mesh;

typedef struct {
    uint64_t handle;
    int alive;
} efxjs_rendertarget;

typedef struct {
    efx_text_fontdata *fd;
    int alive;
} efxjs_fontdata;

typedef struct {
    efx_text_font *font;
    int alive;
} efxjs_font;

typedef struct {
    uint64_t handle;
    int alive;
} efxjs_particlesystem;

/* ------------------------------------------------ F14 audio classes */

typedef struct {
    efx_audio_data *data;
    int alive;
} efxjs_audiodata;

typedef struct {
    efx_audio_stream *stream;
    int alive;
} efxjs_audiostream;

typedef struct {
    int voice;        /* core voice id, -1 once stopped */
    long long serial; /* core voice serial at start (steal detection) */
    int alive;
    int loop;
    float volume, pan, pitch;
} efxjs_audio;

static JSClassID texture_class_id;
static JSClassID imagedata_class_id;
static JSClassID meshdata_class_id;
static JSClassID mesh_class_id;
static JSClassID rendertarget_class_id;
static JSClassID fontdata_class_id;
static JSClassID font_class_id;
static JSClassID particlesystem_class_id;
static JSClassID audiodata_class_id;
static JSClassID audiostream_class_id;
static JSClassID audio_class_id;

static void texture_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_texture *t = JS_GetOpaque(val, texture_class_id);
    if (t) {
        if (t->alive && !t->permanent) {
            efx_render_texture_destroy(t->handle);
        }
        free(t);
    }
}

static void imagedata_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_imagedata *d = JS_GetOpaque(val, imagedata_class_id);
    if (d) {
        free(d->pixels);
        free(d);
    }
}

static void meshdata_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_meshdata *m = JS_GetOpaque(val, meshdata_class_id);
    if (m) {
        efx_meshdata_destroy(m->md);
        free(m);
    }
}

static void mesh_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_mesh *m = JS_GetOpaque(val, mesh_class_id);
    if (m) {
        if (m->alive) {
            efx_render_mesh_destroy(m->handle);
        }
        free(m);
    }
}

static void rendertarget_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_rendertarget *t = JS_GetOpaque(val, rendertarget_class_id);
    if (t) {
        if (t->alive) {
            efx_render_target_destroy(t->handle);
        }
        free(t);
    }
}

static void fontdata_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_fontdata *d = JS_GetOpaque(val, fontdata_class_id);
    if (d) {
        efx_text_fontdata_destroy(d->fd);
        free(d);
    }
}

static void font_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_font *f = JS_GetOpaque(val, font_class_id);
    if (f) {
        efx_text_font_destroy(f->font);
        free(f);
    }
}

static JSValue js_destroy_resource(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_texture *t = JS_GetOpaque2(ctx, this_val, texture_class_id);
    if (t) {
        if (!t->alive) {
            return JS_UNDEFINED; /* destroy() is idempotent */
        }
        if (t->permanent) {
            return type_error(ctx, "cannot destroy an engine-owned texture");
        }
        t->alive = 0;
        efx_render_texture_destroy(t->handle);
        return JS_UNDEFINED;
    }
    efxjs_imagedata *d = JS_GetOpaque2(ctx, this_val, imagedata_class_id);
    if (d) {
        d->alive = 0; /* native bytes released by the GC finalizer */
        return JS_UNDEFINED;
    }
    efxjs_meshdata *md = JS_GetOpaque2(ctx, this_val, meshdata_class_id);
    if (md) {
        if (!md->alive) {
            return JS_UNDEFINED;
        }
        md->alive = 0;
        efx_meshdata_destroy(md->md);
        md->md = NULL;
        return JS_UNDEFINED;
    }
    efxjs_mesh *m = JS_GetOpaque2(ctx, this_val, mesh_class_id);
    if (m) {
        if (!m->alive) {
            return JS_UNDEFINED;
        }
        m->alive = 0;
        efx_render_mesh_destroy(m->handle);
        return JS_UNDEFINED;
    }
    efxjs_rendertarget *tgt = JS_GetOpaque2(ctx, this_val, rendertarget_class_id);
    if (tgt) {
        if (!tgt->alive) {
            return JS_UNDEFINED;
        }
        tgt->alive = 0;
        efx_render_target_destroy(tgt->handle);
        return JS_UNDEFINED;
    }
    efxjs_fontdata *fd = JS_GetOpaque2(ctx, this_val, fontdata_class_id);
    if (fd) {
        if (!fd->alive) {
            return JS_UNDEFINED;
        }
        fd->alive = 0;
        efx_text_fontdata_destroy(fd->fd);
        fd->fd = NULL;
        return JS_UNDEFINED;
    }
    efxjs_font *font = JS_GetOpaque2(ctx, this_val, font_class_id);
    if (font) {
        if (!font->alive) {
            return JS_UNDEFINED;
        }
        font->alive = 0;
        efx_text_font_destroy(font->font);
        font->font = NULL;
        return JS_UNDEFINED;
    }
    efxjs_particlesystem *ps =
        JS_GetOpaque2(ctx, this_val, particlesystem_class_id);
    if (ps) {
        if (!ps->alive) {
            return JS_UNDEFINED;
        }
        ps->alive = 0;
        efx_render_particles_destroy(ps->handle);
        return JS_UNDEFINED;
    }
    efxjs_audiodata *ad = JS_GetOpaque2(ctx, this_val, audiodata_class_id);
    if (ad) {
        if (!ad->alive) {
            return JS_UNDEFINED;
        }
        ad->alive = 0;
        if (ad->data) {
            efx_audio_data_release(ad->data);
            ad->data = NULL;
        }
        return JS_UNDEFINED;
    }
    efxjs_audiostream *as = JS_GetOpaque2(ctx, this_val, audiostream_class_id);
    if (as) {
        if (!as->alive) {
            return JS_UNDEFINED;
        }
        as->alive = 0;
        if (as->stream) {
            efx_audio_stream_release(as->stream);
            as->stream = NULL;
        }
        return JS_UNDEFINED;
    }
    efxjs_audio *aud = JS_GetOpaque2(ctx, this_val, audio_class_id);
    if (aud) {
        if (!aud->alive) {
            return JS_UNDEFINED;
        }
        aud->alive = 0;
        if (aud->voice >= 0 &&
            efx_audio_voice_serial(aud->voice) == aud->serial) {
            efx_audio_stop_voice(aud->voice);
        }
        aud->voice = -1;
        return JS_UNDEFINED;
    }
    return type_error(ctx, "not a resource object");
}

static void particlesystem_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_particlesystem *p = JS_GetOpaque(val, particlesystem_class_id);
    if (p) {
        if (p->alive) {
            efx_render_particles_destroy(p->handle);
        }
        free(p);
    }
}

static void audiodata_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_audiodata *d = JS_GetOpaque(val, audiodata_class_id);
    if (d) {
        if (d->data) {
            efx_audio_data_release(d->data);
        }
        free(d);
    }
}

static void audiostream_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_audiostream *s = JS_GetOpaque(val, audiostream_class_id);
    if (s) {
        if (s->stream) {
            efx_audio_stream_release(s->stream);
        }
        free(s);
    }
}

static void audio_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_audio *a = JS_GetOpaque(val, audio_class_id);
    if (a) {
        free(a); /* dropping the handle never cuts off a fire-and-forget sound */
    }
}

static JSClassDef texture_class_def = {
    "Texture",
    .finalizer = texture_finalizer,
};
static JSClassDef imagedata_class_def = {
    "ImageData",
    .finalizer = imagedata_finalizer,
};
static JSClassDef meshdata_class_def = {
    "MeshData",
    .finalizer = meshdata_finalizer,
};
static JSClassDef mesh_class_def = {
    "Mesh",
    .finalizer = mesh_finalizer,
};
static JSClassDef rendertarget_class_def = {
    "RenderTarget",
    .finalizer = rendertarget_finalizer,
};
static JSClassDef fontdata_class_def = {
    "FontData",
    .finalizer = fontdata_finalizer,
};
static JSClassDef font_class_def = {
    "Font",
    .finalizer = font_finalizer,
};
static JSClassDef particlesystem_class_def = {
    "ParticleSystem",
    .finalizer = particlesystem_finalizer,
};
static JSClassDef audiodata_class_def = {
    "AudioData",
    .finalizer = audiodata_finalizer,
};
static JSClassDef audiostream_class_def = {
    "AudioStream",
    .finalizer = audiostream_finalizer,
};
static JSClassDef audio_class_def = {
    "Audio",
    .finalizer = audio_finalizer,
};

/* ------------------------------------------------ F12 physics classes */

/* `self` is an owned reference while `pinned` (the collider is in the world,
 * so the world keeps its wrapper alive), otherwise borrowed (valid while this
 * struct is linked) */
typedef struct efxjs_body {
    efx_physics_world *w;
    efx_phys_body handle;
    int alive;
    int pinned;
    JSValue self;
    struct efxjs_body *next;
    struct efx_host_state *host;
} efxjs_body;

typedef struct efxjs_character {
    efx_physics_world *w;
    efx_phys_character handle;
    int alive;
    int pinned;
    JSValue self;
    struct efxjs_character *next;
    struct efx_host_state *host;
} efxjs_character;

static JSClassID body_class_id;
static JSClassID character_class_id;

static efx_physics_world *physics_world(JSContext *ctx) {
    struct efx_host_state *h = host_state(ctx);
    if (!h->physics_world) {
        h->physics_world = efx_physics_world_new();
    }
    return (efx_physics_world *)h->physics_world;
}

static void body_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_body *b = JS_GetOpaque(val, body_class_id);
    if (!b) return;
    if (b->host) {
        struct efx_host_state *h = b->host;
        efxjs_body **pp = (efxjs_body **)&h->physics_bodies;
        while (*pp) {
            if (*pp == b) {
                *pp = b->next;
                break;
            }
            pp = &(*pp)->next;
        }
    }
    if (b->alive && b->w) {
        efx_physics_destroy_body(b->w, b->handle);
    }
    free(b);
}

static void character_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_character *c = JS_GetOpaque(val, character_class_id);
    if (!c) return;
    if (c->host) {
        struct efx_host_state *h = c->host;
        efxjs_character **pp = (efxjs_character **)&h->physics_characters;
        while (*pp) {
            if (*pp == c) {
                *pp = c->next;
                break;
            }
            pp = &(*pp)->next;
        }
    }
    if (c->alive && c->w) {
        efx_physics_destroy_character(c->w, c->handle);
    }
    free(c);
}

static JSClassDef body_class_def = {
    "Body",
    .finalizer = body_finalizer,
};
static JSClassDef character_class_def = {
    "Character",
    .finalizer = character_finalizer,
};

/* drop the world's reference; this may finalize and free `b` */
static void body_unpin(JSContext *ctx, efxjs_body *b) {
    if (!b->pinned) return;
    b->pinned = 0;
    JS_FreeValue(ctx, b->self);
}

static void character_unpin(JSContext *ctx, efxjs_character *c) {
    if (!c->pinned) return;
    c->pinned = 0;
    JS_FreeValue(ctx, c->self);
}

/* marks every wrapper destroyed and releases the world's references (after
 * efx_physics_clear, and at runtime teardown before the context is freed) */
static void physics_release_wrappers(JSContext *ctx) {
    struct efx_host_state *h = host_state(ctx);
    efxjs_body *b = (efxjs_body *)h->physics_bodies;
    while (b) {
        efxjs_body *next = b->next;
        b->alive = 0;
        body_unpin(ctx, b);
        b = next;
    }
    efxjs_character *c = (efxjs_character *)h->physics_characters;
    while (c) {
        efxjs_character *next = c->next;
        c->alive = 0;
        character_unpin(ctx, c);
        c = next;
    }
}

void efx_api_physics_release(JSContext *ctx) {
    physics_release_wrappers(ctx);
}

/* ---- F12 binding helpers ---- */

/* live Mesh resolution for static-mesh colliders (defined with the F3
 * bindings); on failure it throws and returns NULL */
static efxjs_mesh *get_live_mesh(JSContext *ctx, JSValueConst v);

static JSValue vec3_to_js(JSContext *ctx, efx_vec3 v) {
    JSValue a = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, a, 0, JS_NewFloat64(ctx, (double)v.x));
    JS_SetPropertyUint32(ctx, a, 1, JS_NewFloat64(ctx, (double)v.y));
    JS_SetPropertyUint32(ctx, a, 2, JS_NewFloat64(ctx, (double)v.z));
    return a;
}

static efxjs_body *get_live_body(JSContext *ctx, JSValueConst v) {
    efxjs_body *b = JS_GetOpaque2(ctx, v, body_class_id);
    if (!b) {
        type_error(ctx, "expected a Body");
        return NULL;
    }
    if (!b->alive || !b->w || !efx_physics_body_alive(b->w, b->handle)) {
        type_error(ctx, "using a destroyed Body");
        return NULL;
    }
    return b;
}

static efxjs_character *get_live_character(JSContext *ctx, JSValueConst v) {
    efxjs_character *c = JS_GetOpaque2(ctx, v, character_class_id);
    if (!c) {
        type_error(ctx, "expected a Character");
        return NULL;
    }
    if (!c->alive || !c->w ||
        !efx_physics_character_alive(c->w, c->handle)) {
        type_error(ctx, "using a destroyed Character");
        return NULL;
    }
    return c;
}

/* borrowed wrapper lookup by native id (a live wrapper is guaranteed to exist
 * while its C struct is linked) */
static JSValue find_body_wrapper(JSContext *ctx, efx_phys_body id) {
    if (id == 0) return JS_NULL;
    struct efx_host_state *h = host_state(ctx);
    for (efxjs_body *b = (efxjs_body *)h->physics_bodies; b; b = b->next) {
        if (b->alive && b->handle == id) {
            return JS_DupValue(ctx, b->self);
        }
    }
    return JS_NULL;
}

static JSValue find_character_wrapper(JSContext *ctx, efx_phys_character id) {
    if (id == 0) return JS_NULL;
    struct efx_host_state *h = host_state(ctx);
    for (efxjs_character *c = (efxjs_character *)h->physics_characters; c;
         c = c->next) {
        if (c->alive && c->handle == id) {
            return JS_DupValue(ctx, c->self);
        }
    }
    return JS_NULL;
}

/* option readers: 1 present, 0 absent, -1 error (throws) */
static int phys_opt_number(JSContext *ctx, JSValueConst obj, const char *key,
                          double *out) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (JS_ToFloat64(ctx, out, v) < 0 || !isfinite(*out)) {
        JS_FreeValue(ctx, v);
        type_error(ctx, "numeric option fields must be finite numbers");
        return -1;
    }
    JS_FreeValue(ctx, v);
    return 1;
}

static int phys_opt_bool(JSContext *ctx, JSValueConst obj, const char *key,
                        int *out) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    *out = JS_ToBool(ctx, v) ? 1 : 0;
    JS_FreeValue(ctx, v);
    return 1;
}

static int phys_opt_vec3(JSContext *ctx, JSValueConst obj, const char *key,
                        efx_vec3 *out) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    float f[3];
    if (get_float_array(ctx, v, f, 3) != 0) {
        JS_FreeValue(ctx, v);
        return -1;
    }
    JS_FreeValue(ctx, v);
    *out = efx_v3(f[0], f[1], f[2]);
    return 1;
}

static int phys_opt_mask(JSContext *ctx, JSValueConst obj, const char *key,
                        uint32_t *out) {
    double d;
    int r = phys_opt_number(ctx, obj, key, &d);
    if (r <= 0) return r;
    if (d < 0 || d > 4294967295.0 || floor(d) != d) {
        range_error(ctx, "layer/mask must be a 32-bit unsigned integer");
        return -1;
    }
    *out = (uint32_t)d;
    return 1;
}

/* the parsed shape carries the source live Mesh for a mesh collider so the
 * caller can build the triangle data where it is needed */
typedef struct parsed_shape {
    efx_shape shape;
    efxjs_mesh *mesh_src;
} parsed_shape;

static int parse_shape(JSContext *ctx, JSValueConst v, parsed_shape *out) {
    if (!JS_IsObject(v)) {
        type_error(ctx, "shape must be an options object");
        return -1;
    }
    memset(out, 0, sizeof(*out));
    JSValue tv = JS_GetPropertyStr(ctx, v, "type");
    const char *type = JS_ToCString(ctx, tv);
    JS_FreeValue(ctx, tv);
    if (!type) return -1;

    int rc = -1;
    if (strcmp(type, "sphere") == 0) {
        static const char *known[] = {"type", "radius"};
        double r;
        if (check_known_fields(ctx, v, known, 2, "shape") != 0) {
            rc = -1;
        } else if (phys_opt_number(ctx, v, "radius", &r) != 1) {
            type_error(ctx, "sphere shapes require a radius");
            rc = -1;
        } else if (!(r > 0)) {
            range_error(ctx, "radius must be positive");
            rc = -1;
        } else {
            out->shape = efx_shape_sphere((float)r);
            rc = 0;
        }
    } else if (strcmp(type, "box") == 0) {
        static const char *known[] = {"type", "size"};
        JSValue sv = JS_GetPropertyStr(ctx, v, "size");
        float f[3];
        if (check_known_fields(ctx, v, known, 2, "shape") != 0) {
            rc = -1;
        } else if (get_float_array(ctx, sv, f, 3) != 0) {
            type_error(ctx, "box shapes require a [x,y,z] size");
            rc = -1;
        } else if (!(f[0] > 0 && f[1] > 0 && f[2] > 0)) {
            range_error(ctx, "box size components must be positive");
            rc = -1;
        } else {
            out->shape = efx_shape_box(efx_v3(f[0], f[1], f[2]));
            rc = 0;
        }
        JS_FreeValue(ctx, sv);
    } else if (strcmp(type, "capsule") == 0) {
        static const char *known[] = {"type", "radius", "height"};
        double r = 0, h = 0;
        int has_r = phys_opt_number(ctx, v, "radius", &r);
        int has_h = phys_opt_number(ctx, v, "height", &h);
        if (check_known_fields(ctx, v, known, 3, "shape") != 0) {
            rc = -1;
        } else if (has_r != 1 || has_h != 1) {
            type_error(ctx, "capsule shapes require radius and height");
            rc = -1;
        } else if (!(r > 0)) {
            range_error(ctx, "radius must be positive");
            rc = -1;
        } else if (!(h >= 2 * r)) {
            range_error(ctx, "capsule height must be at least 2 * radius");
            rc = -1;
        } else {
            out->shape = efx_shape_capsule((float)r, (float)h);
            rc = 0;
        }
    } else if (strcmp(type, "mesh") == 0) {
        static const char *known[] = {"type", "mesh"};
        JSValue mv = JS_GetPropertyStr(ctx, v, "mesh");
        if (check_known_fields(ctx, v, known, 2, "shape") != 0) {
            rc = -1;
        } else {
            efxjs_mesh *m = get_live_mesh(ctx, mv);
            if (!m) {
                rc = -1;
            } else {
                out->shape = efx_shape_sphere(0);
                out->shape.type = EFX_PHYS_SHAPE_MESH;
                out->mesh_src = m;
                rc = 0;
            }
        }
        JS_FreeValue(ctx, mv);
    } else {
        type_error(ctx, "unknown shape type");
        rc = -1;
    }
    JS_FreeCString(ctx, type);
    return rc;
}

/* build a temporary efx_phys_mesh from a live Mesh (queries only; the caller
 * frees it) */
static efx_phys_mesh *build_temp_mesh(JSContext *ctx, efxjs_mesh *m) {
    int verts = 0, indices = 0;
    if (!efx_render_mesh_geometry_count(m->handle, &verts, &indices) ||
        verts <= 0 || indices < 3) {
        return NULL;
    }
    if (indices % 3 != 0) return NULL;
    float *pos = malloc((size_t)verts * 3 * sizeof(float));
    uint32_t *idx = malloc((size_t)indices * sizeof(uint32_t));
    if (!pos || !idx) {
        free(pos);
        free(idx);
        generic_error(ctx, "out of memory");
        return NULL;
    }
    efx_render_mesh_geometry(m->handle, pos, idx);
    efx_phys_mesh *mesh =
        efx_phys_mesh_create(pos, verts, idx, indices / 3);
    free(pos);
    free(idx);
    if (!mesh) {
        generic_error(ctx, "failed to build collision mesh");
    }
    return mesh;
}

static JSValue wrap_body(JSContext *ctx, efx_physics_world *w,
                         efx_phys_body handle) {
    efxjs_body *b = calloc(1, sizeof(*b));
    if (!b) {
        efx_physics_destroy_body(w, handle);
        return generic_error(ctx, "out of memory");
    }
    b->w = w;
    b->handle = handle;
    b->alive = 1;
    b->host = host_state(ctx);
    JSValue obj = JS_NewObjectClass(ctx, body_class_id);
    if (JS_IsException(obj)) {
        efx_physics_destroy_body(w, handle);
        free(b);
        return obj;
    }
    JS_SetOpaque(obj, b);
    b->self = JS_DupValue(ctx, obj);
    b->pinned = 1;
    b->next = (efxjs_body *)b->host->physics_bodies;
    b->host->physics_bodies = b;
    return obj;
}

static JSValue wrap_character(JSContext *ctx, efx_physics_world *w,
                              efx_phys_character handle) {
    efxjs_character *c = calloc(1, sizeof(*c));
    if (!c) {
        efx_physics_destroy_character(w, handle);
        return generic_error(ctx, "out of memory");
    }
    c->w = w;
    c->handle = handle;
    c->alive = 1;
    c->host = host_state(ctx);
    JSValue obj = JS_NewObjectClass(ctx, character_class_id);
    if (JS_IsException(obj)) {
        efx_physics_destroy_character(w, handle);
        free(c);
        return obj;
    }
    JS_SetOpaque(obj, c);
    c->self = JS_DupValue(ctx, obj);
    c->pinned = 1;
    c->next = (efxjs_character *)c->host->physics_characters;
    c->host->physics_characters = c;
    return obj;
}

/* ---- Body methods / properties ---- */

static JSValue body_destroy(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_body *b = JS_GetOpaque2(ctx, this_val, body_class_id);
    if (!b) return type_error(ctx, "expected a Body");
    if (b->alive) {
        b->alive = 0;
        if (b->w) efx_physics_destroy_body(b->w, b->handle);
    }
    body_unpin(ctx, b);
    return JS_UNDEFINED;
}

static JSValue body_get_position(JSContext *ctx, JSValueConst this_val) {
    efxjs_body *b = get_live_body(ctx, this_val);
    if (!b) return JS_EXCEPTION;
    efx_vec3 p;
    efx_physics_body_position(b->w, b->handle, &p);
    return vec3_to_js(ctx, p);
}

static JSValue body_get_velocity(JSContext *ctx, JSValueConst this_val) {
    efxjs_body *b = get_live_body(ctx, this_val);
    if (!b) return JS_EXCEPTION;
    efx_vec3 v;
    efx_physics_body_velocity(b->w, b->handle, &v);
    return vec3_to_js(ctx, v);
}

static JSValue body_set_velocity(JSContext *ctx, JSValueConst this_val,
                                 JSValueConst val) {
    efxjs_body *b = get_live_body(ctx, this_val);
    if (!b) return JS_EXCEPTION;
    float f[3];
    if (get_float_array(ctx, val, f, 3) != 0) return JS_EXCEPTION;
    efx_physics_body_set_velocity(b->w, b->handle, efx_v3(f[0], f[1], f[2]));
    return JS_UNDEFINED;
}

static JSValue body_applyImpulse(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    efxjs_body *b = get_live_body(ctx, this_val);
    if (!b) return JS_EXCEPTION;
    if (argc < 1) return type_error(ctx, "applyImpulse requires a vector");
    float f[3];
    if (get_float_array(ctx, argv[0], f, 3) != 0) return JS_EXCEPTION;
    if (!efx_physics_body_apply_impulse(b->w, b->handle,
                                        efx_v3(f[0], f[1], f[2]))) {
        return type_error(ctx, "applyImpulse requires a dynamic body");
    }
    return JS_UNDEFINED;
}

static JSValue body_applyForce(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    efxjs_body *b = get_live_body(ctx, this_val);
    if (!b) return JS_EXCEPTION;
    if (argc < 1) return type_error(ctx, "applyForce requires a vector");
    float f[3];
    if (get_float_array(ctx, argv[0], f, 3) != 0) return JS_EXCEPTION;
    if (!efx_physics_body_apply_force(b->w, b->handle,
                                      efx_v3(f[0], f[1], f[2]))) {
        return type_error(ctx, "applyForce requires a dynamic body");
    }
    return JS_UNDEFINED;
}

static JSValue body_get_contacts(JSContext *ctx, JSValueConst this_val) {
    efxjs_body *b = get_live_body(ctx, this_val);
    if (!b) return JS_EXCEPTION;
    int n = efx_physics_body_contact_count(b->w, b->handle);
    JSValue arr = JS_NewArray(ctx);
    int out_i = 0;
    for (int i = 0; i < n; i++) {
        efx_contact_info ci;
        if (!efx_physics_body_contact(b->w, b->handle, i, &ci)) continue;
        JSValue o = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, o, "body", find_body_wrapper(ctx, ci.body));
        JS_SetPropertyStr(ctx, o, "sensor", JS_NewBool(ctx, ci.sensor));
        JS_SetPropertyStr(ctx, o, "normal", vec3_to_js(ctx, ci.normal));
        JS_SetPropertyStr(ctx, o, "point", vec3_to_js(ctx, ci.point));
        JS_SetPropertyStr(ctx, o, "depth", JS_NewFloat64(ctx, ci.depth));
        JS_SetPropertyStr(ctx, o, "impulse", JS_NewFloat64(ctx, ci.impulse));
        JS_SetPropertyUint32(ctx, arr, (uint32_t)out_i++, o);
    }
    return arr;
}

static JSValue body_get_transform(JSContext *ctx, JSValueConst this_val) {
    efxjs_body *b = get_live_body(ctx, this_val);
    if (!b) return JS_EXCEPTION;
    efx_vec3 p;
    efx_physics_body_position(b->w, b->handle, &p);
    JSValue a = JS_NewArray(ctx);
    const double m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, p.x, p.y, p.z, 1};
    for (int i = 0; i < 16; i++) {
        JS_SetPropertyUint32(ctx, a, (uint32_t)i, JS_NewFloat64(ctx, m[i]));
    }
    return a;
}

static const JSCFunctionListEntry body_proto_funcs[] = {
    JS_CFUNC_DEF("destroy", 0, body_destroy),
    JS_CFUNC_DEF("applyImpulse", 1, body_applyImpulse),
    JS_CFUNC_DEF("applyForce", 1, body_applyForce),
    JS_CGETSET_DEF("position", body_get_position, NULL),
    JS_CGETSET_DEF("velocity", body_get_velocity, body_set_velocity),
    JS_CGETSET_DEF("contacts", body_get_contacts, NULL),
    JS_CGETSET_DEF("transform", body_get_transform, NULL),
};

/* ---- Character methods / properties ---- */

static JSValue character_destroy(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_character *c = JS_GetOpaque2(ctx, this_val, character_class_id);
    if (!c) return type_error(ctx, "expected a Character");
    if (c->alive) {
        c->alive = 0;
        if (c->w) efx_physics_destroy_character(c->w, c->handle);
    }
    character_unpin(ctx, c);
    return JS_UNDEFINED;
}

static JSValue character_get_position(JSContext *ctx, JSValueConst this_val) {
    efxjs_character *c = get_live_character(ctx, this_val);
    if (!c) return JS_EXCEPTION;
    efx_vec3 p;
    efx_physics_character_position(c->w, c->handle, &p);
    return vec3_to_js(ctx, p);
}

static JSValue character_get_velocity(JSContext *ctx, JSValueConst this_val) {
    efxjs_character *c = get_live_character(ctx, this_val);
    if (!c) return JS_EXCEPTION;
    efx_vec3 v;
    efx_physics_character_velocity(c->w, c->handle, &v);
    return vec3_to_js(ctx, v);
}

static JSValue character_set_velocity(JSContext *ctx, JSValueConst this_val,
                                      JSValueConst val) {
    efxjs_character *c = get_live_character(ctx, this_val);
    if (!c) return JS_EXCEPTION;
    float f[3];
    if (get_float_array(ctx, val, f, 3) != 0) return JS_EXCEPTION;
    efx_physics_character_set_velocity(c->w, c->handle, efx_v3(f[0], f[1],
                                                              f[2]));
    return JS_UNDEFINED;
}

static JSValue character_get_onFloor(JSContext *ctx, JSValueConst this_val) {
    efxjs_character *c = get_live_character(ctx, this_val);
    if (!c) return JS_EXCEPTION;
    return JS_NewBool(ctx, efx_physics_character_on_floor(c->w, c->handle));
}

static JSValue character_moveAndSlide(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    efxjs_character *c = get_live_character(ctx, this_val);
    if (!c) return JS_EXCEPTION;
    if (argc < 1) return type_error(ctx, "moveAndSlide requires a motion vector");
    float f[3];
    if (get_float_array(ctx, argv[0], f, 3) != 0) return JS_EXCEPTION;
    efx_move_result mr;
    if (!efx_physics_character_move_and_slide(c->w, c->handle,
                                              efx_v3(f[0], f[1], f[2]), &mr)) {
        return type_error(ctx, "moveAndSlide failed");
    }
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "position", vec3_to_js(ctx, mr.position));
    JS_SetPropertyStr(ctx, o, "onFloor", JS_NewBool(ctx, mr.on_floor));
    JS_SetPropertyStr(ctx, o, "onWall", JS_NewBool(ctx, mr.on_wall));
    JS_SetPropertyStr(ctx, o, "onCeiling", JS_NewBool(ctx, mr.on_ceiling));
    JS_SetPropertyStr(ctx, o, "floorNormal",
                      vec3_to_js(ctx, mr.floor_normal));
    JSValue cols = JS_NewArray(ctx);
    int n = efx_physics_move_collision_count(c->w, c->handle);
    int out_i = 0;
    for (int i = 0; i < n; i++) {
        efx_move_collision mc;
        if (!efx_physics_move_collision(c->w, c->handle, i, &mc)) continue;
        JSValue co = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, co, "body", find_body_wrapper(ctx, mc.body));
        JS_SetPropertyStr(ctx, co, "normal", vec3_to_js(ctx, mc.normal));
        JS_SetPropertyStr(ctx, co, "point", vec3_to_js(ctx, mc.point));
        JS_SetPropertyUint32(ctx, cols, (uint32_t)out_i++, co);
    }
    JS_SetPropertyStr(ctx, o, "collisions", cols);
    return o;
}

static const JSCFunctionListEntry character_proto_funcs[] = {
    JS_CFUNC_DEF("destroy", 0, character_destroy),
    JS_CFUNC_DEF("moveAndSlide", 1, character_moveAndSlide),
    JS_CGETSET_DEF("position", character_get_position, NULL),
    JS_CGETSET_DEF("velocity", character_get_velocity, character_set_velocity),
    JS_CGETSET_DEF("onFloor", character_get_onFloor, NULL),
};

/* read-only query properties (Texture.width / Texture.height), resolved
 * through the render layer's texture registry at read time */
static JSValue efx_js_texture_getWidth(JSContext *ctx, JSValueConst this_val) {
    efxjs_texture *t = JS_GetOpaque2(ctx, this_val, texture_class_id);
    if (!t) {
        return type_error(ctx, "expected a Texture");
    }
    if (!t->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    int w = 0, h = 0;
    efx_render_texture_size(t->handle, &w, &h);
    return JS_NewInt32(ctx, w);
}

static JSValue efx_js_texture_getHeight(JSContext *ctx, JSValueConst this_val) {
    efxjs_texture *t = JS_GetOpaque2(ctx, this_val, texture_class_id);
    if (!t) {
        return type_error(ctx, "expected a Texture");
    }
    if (!t->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    int w = 0, h = 0;
    efx_render_texture_size(t->handle, &w, &h);
    return JS_NewInt32(ctx, h);
}

static const JSCFunctionListEntry texture_proto_funcs[] = {
    JS_CGETSET_DEF("width", efx_js_texture_getWidth, NULL),
    JS_CGETSET_DEF("height", efx_js_texture_getHeight, NULL),
};

/* read-only ImageData dimensions (F6a: loadImage results expose the decoded
 * pixel size; createImageData results expose the built size) */
static JSValue efx_js_imagedata_getWidth(JSContext *ctx, JSValueConst this_val) {
    efxjs_imagedata *d = JS_GetOpaque2(ctx, this_val, imagedata_class_id);
    if (!d) {
        return type_error(ctx, "expected an ImageData");
    }
    if (!d->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    return JS_NewInt32(ctx, d->w);
}

static JSValue efx_js_imagedata_getHeight(JSContext *ctx, JSValueConst this_val) {
    efxjs_imagedata *d = JS_GetOpaque2(ctx, this_val, imagedata_class_id);
    if (!d) {
        return type_error(ctx, "expected an ImageData");
    }
    if (!d->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    return JS_NewInt32(ctx, d->h);
}

static const JSCFunctionListEntry imagedata_proto_funcs[] = {
    JS_CGETSET_DEF("width", efx_js_imagedata_getWidth, NULL),
    JS_CGETSET_DEF("height", efx_js_imagedata_getHeight, NULL),
};

/* read-only query property surfaceCount (MeshData/Mesh, F3) */
static JSValue efx_js_meshdata_getSurfaceCount(JSContext *ctx,
                                               JSValueConst this_val) {
    efxjs_meshdata *m = JS_GetOpaque2(ctx, this_val, meshdata_class_id);
    if (!m) {
        return type_error(ctx, "expected a MeshData");
    }
    if (!m->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    return JS_NewInt32(ctx, m->md->surface_count);
}

static JSValue efx_js_mesh_getSurfaceCount(JSContext *ctx,
                                           JSValueConst this_val) {
    efxjs_mesh *m = JS_GetOpaque2(ctx, this_val, mesh_class_id);
    if (!m) {
        return type_error(ctx, "expected a Mesh");
    }
    if (!m->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    return JS_NewInt32(ctx, efx_render_mesh_surface_count(m->handle));
}

static const JSCFunctionListEntry meshdata_proto_funcs[] = {
    JS_CGETSET_DEF("surfaceCount", efx_js_meshdata_getSurfaceCount, NULL),
};

static const JSCFunctionListEntry mesh_proto_funcs[] = {
    JS_CGETSET_DEF("surfaceCount", efx_js_mesh_getSurfaceCount, NULL),
};

/* read-only query properties width/height (RenderTarget, F5a) */
static JSValue efx_js_target_getWidth(JSContext *ctx, JSValueConst this_val) {
    efxjs_rendertarget *t = JS_GetOpaque2(ctx, this_val, rendertarget_class_id);
    if (!t) {
        return type_error(ctx, "expected a RenderTarget");
    }
    if (!t->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    int w = 0, h = 0;
    efx_render_target_size(t->handle, &w, &h);
    return JS_NewInt32(ctx, w);
}

static JSValue efx_js_target_getHeight(JSContext *ctx, JSValueConst this_val) {
    efxjs_rendertarget *t = JS_GetOpaque2(ctx, this_val, rendertarget_class_id);
    if (!t) {
        return type_error(ctx, "expected a RenderTarget");
    }
    if (!t->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    int w = 0, h = 0;
    efx_render_target_size(t->handle, &w, &h);
    return JS_NewInt32(ctx, h);
}

static const JSCFunctionListEntry rendertarget_proto_funcs[] = {
    JS_CGETSET_DEF("width", efx_js_target_getWidth, NULL),
    JS_CGETSET_DEF("height", efx_js_target_getHeight, NULL),
};

/* read-only font metrics (F8a): Font.size / lineHeight / ascent / descent */
static JSValue efx_js_font_getSize(JSContext *ctx, JSValueConst this_val) {
    efxjs_font *f = JS_GetOpaque2(ctx, this_val, font_class_id);
    if (!f) {
        return type_error(ctx, "expected a Font");
    }
    if (!f->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    return JS_NewFloat64(ctx, (double)efx_text_font_size(f->font));
}

static JSValue efx_js_font_getLineHeight(JSContext *ctx, JSValueConst this_val) {
    efxjs_font *f = JS_GetOpaque2(ctx, this_val, font_class_id);
    if (!f) {
        return type_error(ctx, "expected a Font");
    }
    if (!f->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    return JS_NewFloat64(ctx, (double)efx_text_font_line_height(f->font));
}

static JSValue efx_js_font_getAscent(JSContext *ctx, JSValueConst this_val) {
    efxjs_font *f = JS_GetOpaque2(ctx, this_val, font_class_id);
    if (!f) {
        return type_error(ctx, "expected a Font");
    }
    if (!f->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    return JS_NewFloat64(ctx, (double)efx_text_font_ascent(f->font));
}

static JSValue efx_js_font_getDescent(JSContext *ctx, JSValueConst this_val) {
    efxjs_font *f = JS_GetOpaque2(ctx, this_val, font_class_id);
    if (!f) {
        return type_error(ctx, "expected a Font");
    }
    if (!f->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    return JS_NewFloat64(ctx, (double)efx_text_font_descent(f->font));
}

static const JSCFunctionListEntry font_proto_funcs[] = {
    JS_CGETSET_DEF("size", efx_js_font_getSize, NULL),
    JS_CGETSET_DEF("lineHeight", efx_js_font_getLineHeight, NULL),
    JS_CGETSET_DEF("ascent", efx_js_font_getAscent, NULL),
    JS_CGETSET_DEF("descent", efx_js_font_getDescent, NULL),
};

/* ------------------------------------------ F11 particle system bindings */

static efxjs_particlesystem *get_live_ps(JSContext *ctx, JSValueConst v) {
    efxjs_particlesystem *p = JS_GetOpaque2(ctx, v, particlesystem_class_id);
    if (!p) {
        type_error(ctx, "expected a ParticleSystem");
        return NULL;
    }
    if (!p->alive) {
        type_error(ctx, "using a destroyed resource");
        return NULL;
    }
    return p;
}

/* read a value as an [x,y] or [x,y,z] float vector; 0 absent, 1 set, -1 err */
static int vec_from_value(JSContext *ctx, JSValueConst v, float out[3],
                          int allow2) {
    float tmp[3] = {0, 0, 0};
    int n = 3;
    if (!JS_IsArray(v)) {
        type_error(ctx, "expected an array");
        return -1;
    }
    JSValue lv = JS_GetPropertyStr(ctx, v, "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lv);
    JS_FreeValue(ctx, lv);
    if (allow2 && len == 2) {
        n = 2;
    } else if (len != 3) {
        type_error(ctx, "expected a [x,y] or [x,y,z] array");
        return -1;
    }
    if (get_float_array(ctx, v, tmp, n) != 0) {
        return -1;
    }
    for (int i = 0; i < n; i++) out[i] = tmp[i];
    return 1;
}

/* optional vector field: 0 absent, 1 set, -1 error */
static int pcfg_vec(JSContext *ctx, JSValueConst o, const char *k, float out[3],
                    int allow2) {
    JSValue v = JS_GetPropertyStr(ctx, o, k);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    int r = vec_from_value(ctx, v, out, allow2);
    JS_FreeValue(ctx, v);
    return r;
}

/* scalar field: 0 absent, 1 set, -1 error */
static int pcfg_num(JSContext *ctx, JSValueConst o, const char *k, float *out) {
    JSValue v = JS_GetPropertyStr(ctx, o, k);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    double d;
    if (JS_ToFloat64(ctx, &d, v) < 0 || !isfinite(d)) {
        JS_FreeValue(ctx, v);
        type_error(ctx, "option must be a finite number");
        return -1;
    }
    JS_FreeValue(ctx, v);
    *out = (float)d;
    return 1;
}

/* number or [min,max]: 0 absent, 1 set, -1 error */
static int pcfg_range(JSContext *ctx, JSValueConst o, const char *k, float *lo,
                      float *hi) {
    JSValue v = JS_GetPropertyStr(ctx, o, k);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (JS_IsArray(v)) {
        float t[2];
        if (get_float_array(ctx, v, t, 2) != 0) {
            JS_FreeValue(ctx, v);
            return -1;
        }
        *lo = t[0];
        *hi = t[1];
    } else {
        double d;
        if (JS_ToFloat64(ctx, &d, v) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, v);
            type_error(ctx, "expected a number or [min,max]");
            return -1;
        }
        *lo = *hi = (float)d;
    }
    JS_FreeValue(ctx, v);
    return 1;
}

static int pcfg_sizes(JSContext *ctx, JSValueConst o,
                      efx_particle_config *c) {
    JSValue v = JS_GetPropertyStr(ctx, o, "sizes");
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (JS_IsArray(v)) {
        JSValue lv = JS_GetPropertyStr(ctx, v, "length");
        int32_t n = -1;
        JS_ToInt32(ctx, &n, lv);
        JS_FreeValue(ctx, lv);
        if (n < 1 || n > 8) {
            JS_FreeValue(ctx, v);
            range_error(ctx, "sizes must hold 1..8 entries");
            return -1;
        }
        for (int i = 0; i < n; i++) {
            JSValue e = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
            double d;
            if (JS_ToFloat64(ctx, &d, e) < 0 || !isfinite(d) || d <= 0) {
                JS_FreeValue(ctx, e);
                JS_FreeValue(ctx, v);
                range_error(ctx, "sizes must be finite and > 0");
                return -1;
            }
            JS_FreeValue(ctx, e);
            c->sizes[i] = (float)d;
        }
        c->size_count = n;
    } else {
        double d;
        if (JS_ToFloat64(ctx, &d, v) < 0 || !isfinite(d) || d <= 0) {
            JS_FreeValue(ctx, v);
            range_error(ctx, "size must be finite and > 0");
            return -1;
        }
        c->sizes[0] = (float)d;
        c->size_count = 1;
    }
    JS_FreeValue(ctx, v);
    return 1;
}

static int pcfg_colors(JSContext *ctx, JSValueConst o,
                       efx_particle_config *c) {
    JSValue v = JS_GetPropertyStr(ctx, o, "colors");
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (!JS_IsArray(v)) {
        JS_FreeValue(ctx, v);
        type_error(ctx, "colors must be a color or an array of colors");
        return -1;
    }
    JSValue first = JS_GetPropertyUint32(ctx, v, 0);
    int is_list = JS_IsArray(first);
    JS_FreeValue(ctx, first);
    if (is_list) {
        JSValue lv = JS_GetPropertyStr(ctx, v, "length");
        int32_t n = -1;
        JS_ToInt32(ctx, &n, lv);
        JS_FreeValue(ctx, lv);
        if (n < 1 || n > 8) {
            JS_FreeValue(ctx, v);
            range_error(ctx, "colors must hold 1..8 entries");
            return -1;
        }
        for (int i = 0; i < n; i++) {
            JSValue e = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
            if (get_float_array(ctx, e, c->colors[i], 4) != 0) {
                JS_FreeValue(ctx, e);
                JS_FreeValue(ctx, v);
                return -1;
            }
            JS_FreeValue(ctx, e);
        }
        c->color_count = n;
    } else {
        if (get_float_array(ctx, v, c->colors[0], 4) != 0) {
            JS_FreeValue(ctx, v);
            return -1;
        }
        c->color_count = 1;
    }
    JS_FreeValue(ctx, v);
    return 1;
}

static int pcfg_quads(JSContext *ctx, JSValueConst o,
                      efx_particle_config *c) {
    JSValue v = JS_GetPropertyStr(ctx, o, "quads");
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (!JS_IsArray(v)) {
        JS_FreeValue(ctx, v);
        type_error(ctx, "quads must be an array");
        return -1;
    }
    JSValue lv = JS_GetPropertyStr(ctx, v, "length");
    int32_t n = -1;
    JS_ToInt32(ctx, &n, lv);
    JS_FreeValue(ctx, lv);
    if (n < 0 || n > 64) {
        JS_FreeValue(ctx, v);
        range_error(ctx, "quads must hold at most 64 entries");
        return -1;
    }
    for (int i = 0; i < n; i++) {
        JSValue e = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
        float rect[4];
        if (JS_IsArray(e)) {
            if (get_float_array(ctx, e, rect, 4) != 0) {
                JS_FreeValue(ctx, e);
                JS_FreeValue(ctx, v);
                return -1;
            }
        } else if (JS_IsObject(e)) {
            static const char *rk[] = {"x", "y", "w", "h"};
            for (int k = 0; k < 4; k++) {
                JSValue f = JS_GetPropertyStr(ctx, e, rk[k]);
                double d;
                if (JS_ToFloat64(ctx, &d, f) < 0 || !isfinite(d)) {
                    JS_FreeValue(ctx, f);
                    JS_FreeValue(ctx, e);
                    JS_FreeValue(ctx, v);
                    type_error(ctx, "quad rect fields must be finite numbers");
                    return -1;
                }
                JS_FreeValue(ctx, f);
                rect[k] = (float)d;
            }
        } else {
            JS_FreeValue(ctx, e);
            JS_FreeValue(ctx, v);
            type_error(ctx, "each quad must be an object or [x,y,w,h]");
            return -1;
        }
        JS_FreeValue(ctx, e);
        for (int k = 0; k < 4; k++) c->quads[i][k] = rect[k];
    }
    c->quad_count = n;
    JS_FreeValue(ctx, v);
    return 1;
}

static int pcfg_shape(JSContext *ctx, JSValueConst o,
                      efx_particle_config *c) {
    JSValue v = JS_GetPropertyStr(ctx, o, "emissionShape");
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (!JS_IsObject(v)) {
        JS_FreeValue(ctx, v);
        type_error(ctx, "emissionShape must be an object");
        return -1;
    }
    static const char *known[] = {"shape", "size"};
    if (check_known_fields(ctx, v, known, 2, "emissionShape") != 0) {
        JS_FreeValue(ctx, v);
        return -1;
    }
    JSValue sv = JS_GetPropertyStr(ctx, v, "shape");
    if (!JS_IsUndefined(sv)) {
        const char *s = JS_ToCString(ctx, sv);
        int ok = 0;
        if (s) {
            if (!strcmp(s, "point")) { c->shape = EFX_SHAPE_POINT; ok = 1; }
            else if (!strcmp(s, "box")) { c->shape = EFX_PHYS_SHAPE_BOX; ok = 1; }
            else if (!strcmp(s, "sphere")) { c->shape = EFX_PHYS_SHAPE_SPHERE; ok = 1; }
            else if (!strcmp(s, "sphereSurface")) {
                c->shape = EFX_SHAPE_SPHERE_SURFACE; ok = 1;
            } else if (!strcmp(s, "disc")) { c->shape = EFX_SHAPE_DISC; ok = 1; }
            JS_FreeCString(ctx, s);
        }
        if (!ok) {
            JS_FreeValue(ctx, sv);
            JS_FreeValue(ctx, v);
            type_error(ctx, "unknown emission shape");
            return -1;
        }
    }
    JS_FreeValue(ctx, sv);
    int r = pcfg_vec(ctx, v, "size", c->shape_size, 0);
    JS_FreeValue(ctx, v);
    return r < 0 ? -1 : 1;
}

/* parse the particle options over `c` (which the caller pre-fills). Unknown
 * fields throw; absent fields keep their current value. Returns 0/-1. */
static int read_particle_config(JSContext *ctx, JSValueConst opts,
                                efx_particle_config *c) {
    static const char *known[] = {
        "texture", "max", "space", "facing", "normal", "blend", "lifetime",
        "emissionRate", "emitterLifetime", "position", "direction", "spread",
        "speed", "gravity", "linearAcceleration", "radialAcceleration",
        "tangentialAcceleration", "linearDamping", "sizes", "sizeVariation",
        "colors", "rotation", "spin", "spinVariation", "relativeRotation",
        "emissionShape", "quads", "insertMode", "speedScale"};
    if (check_known_fields(ctx, opts, known,
                           (int)(sizeof(known) / sizeof(known[0])),
                           "createParticleSystem") != 0) {
        return -1;
    }

    JSValue tv = JS_GetPropertyStr(ctx, opts, "texture");
    if (!JS_IsUndefined(tv)) {
        if (get_live_sample(ctx, tv, &c->texture) != 0) {
            JS_FreeValue(ctx, tv);
            return -1;
        }
    }
    JS_FreeValue(ctx, tv);

    JSValue mv = JS_GetPropertyStr(ctx, opts, "max");
    if (!JS_IsUndefined(mv)) {
        int32_t n = 0;
        if (JS_ToInt32(ctx, &n, mv) < 0) {
            JS_FreeValue(ctx, mv);
            type_error(ctx, "max must be an integer");
            return -1;
        }
        c->max = n;
    }
    JS_FreeValue(ctx, mv);

    JSValue sp = JS_GetPropertyStr(ctx, opts, "space");
    if (!JS_IsUndefined(sp)) {
        const char *s = JS_ToCString(ctx, sp);
        if (s && !strcmp(s, "screen")) c->space = EFX_SPACE_SCREEN;
        else if (s && !strcmp(s, "world")) c->space = EFX_SPACE_WORLD;
        else {
            if (s) JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, sp);
            type_error(ctx, "space must be 'world' or 'screen'");
            return -1;
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, sp);

    JSValue fv = JS_GetPropertyStr(ctx, opts, "facing");
    if (!JS_IsUndefined(fv)) {
        const char *s = JS_ToCString(ctx, fv);
        if (s && !strcmp(s, "view")) c->facing = EFX_FACING_VIEW;
        else if (s && !strcmp(s, "y")) c->facing = EFX_FACING_Y;
        else if (s && !strcmp(s, "plane")) c->facing = EFX_FACING_PLANE;
        else {
            if (s) JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, fv);
            type_error(ctx, "facing must be 'view', 'y', or 'plane'");
            return -1;
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, fv);

    if (c->space == EFX_SPACE_SCREEN && c->facing != EFX_FACING_VIEW) {
        type_error(ctx, "facing must be 'view' for screen space");
        return -1;
    }

    if (pcfg_vec(ctx, opts, "normal", c->normal, 0) < 0) return -1;

    JSValue bv = JS_GetPropertyStr(ctx, opts, "blend");
    if (!JS_IsUndefined(bv)) {
        const char *s = JS_ToCString(ctx, bv);
        if (s && !strcmp(s, "alpha")) c->blend = EFX_BLEND_ALPHA;
        else if (s && !strcmp(s, "additive")) c->blend = EFX_BLEND_ADDITIVE;
        else if (s && !strcmp(s, "subtractive")) c->blend = EFX_BLEND_SUBTRACTIVE;
        else {
            if (s) JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, bv);
            type_error(ctx, "blend must be 'alpha', 'additive', or 'subtractive'");
            return -1;
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, bv);

    if (pcfg_range(ctx, opts, "lifetime", &c->life_min, &c->life_max) < 0)
        return -1;
    float f;
    int r;
    if ((r = pcfg_num(ctx, opts, "emissionRate", &f)) < 0) return -1;
    if (r > 0) c->emission_rate = f;
    if ((r = pcfg_num(ctx, opts, "emitterLifetime", &f)) < 0) return -1;
    if (r > 0) c->emitter_lifetime = f;
    if (pcfg_vec(ctx, opts, "position", c->position, 1) < 0) return -1;
    if (pcfg_vec(ctx, opts, "direction", c->direction, 1) < 0) return -1;
    if ((r = pcfg_num(ctx, opts, "spread", &f)) < 0) return -1;
    if (r > 0) c->spread = f;
    if (pcfg_range(ctx, opts, "speed", &c->speed_min, &c->speed_max) < 0)
        return -1;
    if (pcfg_vec(ctx, opts, "gravity", c->gravity, 1) < 0) return -1;
    if (pcfg_range(ctx, opts, "radialAcceleration", &c->radial_acc_min,
                   &c->radial_acc_max) < 0)
        return -1;
    if (pcfg_range(ctx, opts, "tangentialAcceleration", &c->tangential_acc_min,
                   &c->tangential_acc_max) < 0)
        return -1;
    if (pcfg_range(ctx, opts, "linearDamping", &c->damping_min,
                   &c->damping_max) < 0)
        return -1;
    if (pcfg_sizes(ctx, opts, c) < 0) return -1;
    if ((r = pcfg_num(ctx, opts, "sizeVariation", &f)) < 0) return -1;
    if (r > 0) c->size_variation = f;
    if (pcfg_colors(ctx, opts, c) < 0) return -1;
    if (pcfg_range(ctx, opts, "rotation", &c->rotation_min, &c->rotation_max) < 0)
        return -1;
    if (pcfg_range(ctx, opts, "spin", &c->spin_start, &c->spin_end) < 0)
        return -1;
    if ((r = pcfg_num(ctx, opts, "spinVariation", &f)) < 0) return -1;
    if (r > 0) c->spin_variation = f;
    JSValue rrv = JS_GetPropertyStr(ctx, opts, "relativeRotation");
    if (!JS_IsUndefined(rrv)) {
        if (!JS_IsBool(rrv)) {
            JS_FreeValue(ctx, rrv);
            type_error(ctx, "relativeRotation must be a boolean");
            return -1;
        }
        c->relative_rotation = JS_ToBool(ctx, rrv);
    }
    JS_FreeValue(ctx, rrv);
    if (pcfg_shape(ctx, opts, c) < 0) return -1;
    if (pcfg_quads(ctx, opts, c) < 0) return -1;

    JSValue im = JS_GetPropertyStr(ctx, opts, "insertMode");
    if (!JS_IsUndefined(im)) {
        const char *s = JS_ToCString(ctx, im);
        if (s && !strcmp(s, "top")) c->insert_mode = EFX_INSERT_TOP;
        else if (s && !strcmp(s, "bottom")) c->insert_mode = EFX_INSERT_BOTTOM;
        else if (s && !strcmp(s, "random")) c->insert_mode = EFX_INSERT_RANDOM;
        else {
            if (s) JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, im);
            type_error(ctx, "insertMode must be 'top', 'bottom', or 'random'");
            return -1;
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, im);

    if ((r = pcfg_num(ctx, opts, "speedScale", &f)) < 0) return -1;
    if (r > 0) c->speed_scale = f;

    /* gravity/linear acceleration are the same concept for a particle */
    r = pcfg_vec(ctx, opts, "linearAcceleration", c->lin_acc_min, 0);
    if (r < 0) return -1;
    if (r > 0) {
        for (int i = 0; i < 3; i++) c->lin_acc_max[i] = c->lin_acc_min[i];
    }
    return 0;
}

static JSValue efx_js_ps_emit(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv) {
    efxjs_particlesystem *p = get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    if (argc < 1) return type_error(ctx, "emit requires a count");
    int32_t n = 0;
    if (JS_ToInt32(ctx, &n, argv[0]) < 0 || n < 0) {
        return range_error(ctx, "emit count must be a non-negative integer");
    }
    int rc = efx_render_particles_emit(p->handle, n);
    if (rc != EFX_RENDER_OK) return generic_error(ctx, "emit failed");
    return JS_UNDEFINED;
}

static JSValue efx_js_ps_start(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    (void)argc; (void)argv;
    efxjs_particlesystem *p = get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    efx_render_particles_start(p->handle);
    return JS_UNDEFINED;
}

static JSValue efx_js_ps_stop(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv) {
    (void)argc; (void)argv;
    efxjs_particlesystem *p = get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    efx_render_particles_stop(p->handle);
    return JS_UNDEFINED;
}

static JSValue efx_js_ps_pause(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    (void)argc; (void)argv;
    efxjs_particlesystem *p = get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    efx_render_particles_pause(p->handle);
    return JS_UNDEFINED;
}

static JSValue efx_js_ps_reset(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    (void)argc; (void)argv;
    efxjs_particlesystem *p = get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    efx_render_particles_reset(p->handle);
    return JS_UNDEFINED;
}

static JSValue efx_js_ps_set(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    efxjs_particlesystem *p = get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return type_error(ctx, "set requires an options object");
    }
    efx_particle_config cfg;
    efx_render_particles_config(p->handle, &cfg);
    if (read_particle_config(ctx, argv[0], &cfg) != 0) {
        return JS_EXCEPTION;
    }
    int rc = efx_render_particles_set(p->handle, &cfg);
    if (rc == EFX_RENDER_ERR_HANDLE) return type_error(ctx, "expected a live ParticleSystem");
    if (rc == EFX_RENDER_ERR_SIZE) return range_error(ctx, "invalid particle configuration");
    if (rc != EFX_RENDER_OK) return generic_error(ctx, "set failed");
    return JS_UNDEFINED;
}

static JSValue efx_js_ps_getCount(JSContext *ctx, JSValueConst this_val) {
    efxjs_particlesystem *p = get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    return JS_NewInt32(ctx, efx_render_particles_count(p->handle));
}

static JSValue efx_js_ps_getSpeedScale(JSContext *ctx, JSValueConst this_val) {
    efxjs_particlesystem *p = get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    return JS_NewFloat64(ctx, (double)efx_render_particles_speed_scale(p->handle));
}

static JSValue efx_js_ps_setSpeedScale(JSContext *ctx, JSValueConst this_val,
                                       JSValueConst val) {
    efxjs_particlesystem *p = get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    double d;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d) || d <= 0) {
        return range_error(ctx, "speedScale must be a finite number > 0");
    }
    efx_render_particles_set_speed_scale(p->handle, (float)d);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry particlesystem_proto_funcs[] = {
    JS_CFUNC_DEF("emit", 1, efx_js_ps_emit),
    JS_CFUNC_DEF("start", 0, efx_js_ps_start),
    JS_CFUNC_DEF("stop", 0, efx_js_ps_stop),
    JS_CFUNC_DEF("pause", 0, efx_js_ps_pause),
    JS_CFUNC_DEF("reset", 0, efx_js_ps_reset),
    JS_CFUNC_DEF("set", 1, efx_js_ps_set),
    JS_CGETSET_DEF("count", efx_js_ps_getCount, NULL),
    JS_CGETSET_DEF("speedScale", efx_js_ps_getSpeedScale,
                   efx_js_ps_setSpeedScale),
};

/* -------------------------------------------------------- F6a resource loading */

static const char *resource_err_text(int err) {
    switch (err) {
    case EFX_RESOURCE_ERR_OPEN:
        return "resource root could not be opened";
    case EFX_RESOURCE_ERR_NOTFOUND:
        return "resource not found";
    case EFX_RESOURCE_ERR_PATH:
        return "invalid resource path";
    case EFX_RESOURCE_ERR_IO:
        return "resource read failed";
    case EFX_RESOURCE_ERR_NOMEM:
        return "out of memory";
    default:
        return "resource error";
    }
}

static JSValue plain_error(JSContext *ctx, const char *msg) {
    return JS_ThrowPlainError(ctx, "%s", msg);
}

JSValue efx_js_loadText(JSContext *ctx, JSValueConst this_val, int argc,
                        JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) {
        return type_error(ctx, "loadText requires a path string");
    }
    struct efx_host_state *h = host_state(ctx);
    if (!h->resource) {
        return plain_error(ctx, "no resource root");
    }
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) {
        return JS_EXCEPTION;
    }
    int err = EFX_RESOURCE_OK;
    char *text = efx_resource_read_text(h->resource, path, &err);
    JS_FreeCString(ctx, path);
    if (!text) {
        return plain_error(ctx, resource_err_text(err));
    }
    JSValue out = JS_NewString(ctx, text);
    efx_resource_free(text);
    return out;
}

JSValue efx_js_loadImage(JSContext *ctx, JSValueConst this_val, int argc,
                         JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) {
        return type_error(ctx, "loadImage requires a path string");
    }
    struct efx_host_state *h = host_state(ctx);
    if (!h->resource) {
        return plain_error(ctx, "no resource root");
    }
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) {
        return JS_EXCEPTION;
    }
    size_t size = 0;
    int err = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(h->resource, path, &size, &err);
    JS_FreeCString(ctx, path);
    if (!bytes) {
        return plain_error(ctx, resource_err_text(err));
    }
    int ierr = EFX_IMAGE_OK;
    efx_image *img = efx_image_decode(bytes, size, &ierr);
    efx_resource_free(bytes);
    if (!img) {
        return plain_error(ctx, ierr == EFX_IMAGE_ERR_NOMEM ? "out of memory"
                                                            : "image decode failed");
    }
    size_t n = (size_t)img->width * (size_t)img->height * 4u;
    uint8_t *px = malloc(n ? n : 1);
    if (!px) {
        efx_image_free(img);
        return generic_error(ctx, "out of memory");
    }
    memcpy(px, img->pixels, n);
    int w = img->width;
    int hh = img->height;
    efx_image_free(img);
    efxjs_imagedata *d = calloc(1, sizeof(efxjs_imagedata));
    if (!d) {
        free(px);
        return generic_error(ctx, "out of memory");
    }
    d->pixels = px;
    d->w = w;
    d->h = hh;
    d->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, imagedata_class_id);
    JS_SetOpaque(obj, d);
    return obj;
}

static const char *gltf_err_text(int err) {
    switch (err) {
    case EFX_GLTF_ERR_UNSUPPORTED:
        return "glTF asset requires an unsupported extension";
    case EFX_GLTF_ERR_SELECTION:
        return "glTF mesh selection matched no mesh";
    case EFX_GLTF_ERR_CAP:
        return "glTF mesh exceeds the surface count limit";
    case EFX_GLTF_ERR_IMAGE:
        return "glTF image decode failed";
    case EFX_GLTF_ERR_NOMEM:
        return "out of memory";
    case EFX_GLTF_ERR_IO:
        return "glTF resource could not be read";
    default:
        return "invalid or malformed glTF asset";
    }
}

JSValue efx_js_loadMeshData(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) {
        return type_error(ctx, "loadMeshData requires a path string");
    }
    struct efx_host_state *h = host_state(ctx);
    if (!h->resource) {
        return plain_error(ctx, "no resource root");
    }
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) {
        return JS_EXCEPTION;
    }
    efx_gltf_mesh_opts opts;
    memset(&opts, 0, sizeof(opts));
    char *name = NULL;
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        if (!JS_IsObject(argv[1])) {
            JS_FreeCString(ctx, path);
            return type_error(ctx, "loadMeshData options must be an object");
        }
        static const char *known[] = {"mesh"};
        if (check_known_fields(ctx, argv[1], known, 1, "loadMeshData") != 0) {
            JS_FreeCString(ctx, path);
            return JS_EXCEPTION;
        }
        JSValue mv = JS_GetPropertyStr(ctx, argv[1], "mesh");
        if (!JS_IsUndefined(mv)) {
            opts.has_mesh = 1;
            if (JS_IsString(mv)) {
                opts.is_name = 1;
                name = (char *)JS_ToCString(ctx, mv);
                if (!name) {
                    JS_FreeValue(ctx, mv);
                    JS_FreeCString(ctx, path);
                    return JS_EXCEPTION;
                }
                opts.mesh_name = name;
            } else if (JS_IsNumber(mv)) {
                double d = 0;
                if (JS_ToFloat64(ctx, &d, mv) < 0 || !isfinite(d) ||
                    d != floor(d) || d < 0) {
                    JS_FreeValue(ctx, mv);
                    JS_FreeCString(ctx, path);
                    return type_error(ctx,
                                      "mesh must be a non-negative integer or a name");
                }
                opts.mesh_index = (int)d;
            } else {
                JS_FreeValue(ctx, mv);
                JS_FreeCString(ctx, path);
                return type_error(ctx,
                                  "mesh must be a non-negative integer or a name");
            }
        }
        JS_FreeValue(ctx, mv);
    }
    int err = EFX_GLTF_OK;
    efx_meshdata *md = efx_gltf_load_meshdata(h->resource, path, &opts, &err);
    if (name) {
        JS_FreeCString(ctx, name);
    }
    JS_FreeCString(ctx, path);
    if (!md) {
        return plain_error(ctx, gltf_err_text(err));
    }
    efxjs_meshdata *wrap = calloc(1, sizeof(efxjs_meshdata));
    if (!wrap) {
        efx_meshdata_destroy(md);
        return generic_error(ctx, "out of memory");
    }
    wrap->md = md;
    wrap->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, meshdata_class_id);
    JS_SetOpaque(obj, wrap);
    return obj;
}

/* ------------------------------------------------------ F8a font/text */

static JSValue text_error(JSContext *ctx, int code, const char *msg) {
    if (code == EFX_TEXT_ERR_RANGE) {
        return range_error(ctx, msg);
    }
    return generic_error(ctx, msg);
}

static int get_opt_number(JSContext *ctx, JSValueConst obj, const char *key,
                          int *present, double *out) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    *present = 0;
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (JS_ToFloat64(ctx, out, v) < 0 || !isfinite(*out)) {
        JS_FreeValue(ctx, v);
        return -1;
    }
    JS_FreeValue(ctx, v);
    *present = 1;
    return 0;
}

static int parse_align(JSContext *ctx, JSValueConst obj, const char *key,
                       int *out) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    const char *s = JS_ToCString(ctx, v);
    JS_FreeValue(ctx, v);
    if (!s) return -1;
    int rc = 0;
    if (strcmp(s, "left") == 0) *out = EFX_TEXT_ALIGN_LEFT;
    else if (strcmp(s, "center") == 0) *out = EFX_TEXT_ALIGN_CENTER;
    else if (strcmp(s, "right") == 0) *out = EFX_TEXT_ALIGN_RIGHT;
    else if (strcmp(s, "justify") == 0) *out = EFX_TEXT_ALIGN_JUSTIFY;
    else rc = -1;
    JS_FreeCString(ctx, s);
    if (rc != 0) {
        type_error(ctx, "align must be 'left', 'center', 'right' or 'justify'");
        return -1;
    }
    return 0;
}

static int parse_valign(JSContext *ctx, JSValueConst obj, const char *key,
                        int *out) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    const char *s = JS_ToCString(ctx, v);
    JS_FreeValue(ctx, v);
    if (!s) return -1;
    int rc = 0;
    if (strcmp(s, "top") == 0) *out = EFX_TEXT_VALIGN_TOP;
    else if (strcmp(s, "middle") == 0) *out = EFX_TEXT_VALIGN_MIDDLE;
    else if (strcmp(s, "bottom") == 0) *out = EFX_TEXT_VALIGN_BOTTOM;
    else rc = -1;
    JS_FreeCString(ctx, s);
    if (rc != 0) {
        type_error(ctx, "valign must be 'top', 'middle' or 'bottom'");
        return -1;
    }
    return 0;
}

/* Parses the shared layout options; returns 0 or -1 (exception set). Colors
 * are accepted here (draw reads them separately) so measure and draw share
 * one known-field set. */
static int parse_layout_opts(JSContext *ctx, JSValueConst opts,
                             efx_text_layout_opts *lo) {
    memset(lo, 0, sizeof(*lo));
    lo->align = EFX_TEXT_ALIGN_LEFT;
    lo->valign = EFX_TEXT_VALIGN_TOP;
    lo->scale = 1.0f;
    if (JS_IsUndefined(opts) || JS_IsNull(opts)) return 0;
    if (!JS_IsObject(opts)) {
        type_error(ctx, "text options must be an object");
        return -1;
    }
    static const char *known[] = {"align", "valign", "width", "lineHeight",
                                  "color", "outlineColor", "shadowColor",
                                  "rotation", "scale"};
    if (check_known_fields(ctx, opts, known, 9, "drawText") != 0) return -1;
    if (parse_align(ctx, opts, "align", &lo->align) != 0) return -1;
    if (parse_valign(ctx, opts, "valign", &lo->valign) != 0) return -1;
    double d;
    int present = 0;
    if (get_opt_number(ctx, opts, "width", &present, &d) != 0) {
        type_error(ctx, "width must be a finite number");
        return -1;
    }
    if (present) {
        if (!(d > 0)) {
            range_error(ctx, "width must be > 0");
            return -1;
        }
        lo->has_width = 1;
        lo->width = (float)d;
    }
    if (get_opt_number(ctx, opts, "lineHeight", &present, &d) != 0) {
        type_error(ctx, "lineHeight must be a finite number");
        return -1;
    }
    if (present) {
        if (!(d > 0)) {
            range_error(ctx, "lineHeight must be > 0");
            return -1;
        }
        lo->has_line_height = 1;
        lo->line_height = (float)d;
    }
    if (get_opt_number(ctx, opts, "rotation", &present, &d) != 0) {
        type_error(ctx, "rotation must be a finite number");
        return -1;
    }
    if (present) lo->rotation = (float)d;
    if (get_opt_number(ctx, opts, "scale", &present, &d) != 0) {
        type_error(ctx, "scale must be a finite number");
        return -1;
    }
    if (present) {
        if (!(d > 0)) {
            range_error(ctx, "scale must be > 0");
            return -1;
        }
        lo->scale = (float)d;
    }
    if (lo->align == EFX_TEXT_ALIGN_JUSTIFY && !lo->has_width) {
        type_error(ctx, "justify alignment requires a width");
        return -1;
    }
    return 0;
}

static JSValue bounds_object(JSContext *ctx, const efx_text_bounds *b) {
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "width", JS_NewFloat64(ctx, (double)b->width));
    JS_SetPropertyStr(ctx, o, "height", JS_NewFloat64(ctx, (double)b->height));
    JS_SetPropertyStr(ctx, o, "lines", JS_NewInt32(ctx, b->lines));
    return o;
}

JSValue efx_js_loadFontData(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) {
        return type_error(ctx, "loadFontData requires a path string");
    }
    struct efx_host_state *h = host_state(ctx);
    if (!h->resource) {
        return plain_error(ctx, "no resource root");
    }
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;
    int err = EFX_TEXT_OK;
    efx_text_fontdata *fd = efx_text_fontdata_load(h->resource, path, &err);
    JS_FreeCString(ctx, path);
    if (!fd) {
        return generic_error(ctx, err == EFX_TEXT_ERR_NOMEM
                                      ? "out of memory"
                                      : "font could not be loaded");
    }
    efxjs_fontdata *wrap = calloc(1, sizeof(*wrap));
    if (!wrap) {
        efx_text_fontdata_destroy(fd);
        return generic_error(ctx, "out of memory");
    }
    wrap->fd = fd;
    wrap->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, fontdata_class_id);
    JS_SetOpaque(obj, wrap);
    return obj;
}

JSValue efx_js_createFont(JSContext *ctx, JSValueConst this_val, int argc,
                          JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "createFont requires a FontData");
    }
    efxjs_fontdata *fdw = JS_GetOpaque2(ctx, argv[0], fontdata_class_id);
    if (!fdw) {
        return type_error(ctx, "createFont requires a FontData");
    }
    if (!fdw->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    if (argc < 2 || !JS_IsObject(argv[1])) {
        return type_error(ctx, "createFont requires an options object");
    }
    JSValueConst opts = argv[1];
    static const char *known[] = {"size",   "glyphs", "padding",
                                  "filter", "outline", "shadow"};
    if (check_known_fields(ctx, opts, known, 6, "createFont") != 0) {
        return JS_EXCEPTION;
    }
    efx_font_opts fo;
    memset(&fo, 0, sizeof(fo));
    fo.padding = 1;
    fo.filter = EFX_FILTER_LINEAR;
    double d = 0;
    int present = 0;
    if (get_opt_number(ctx, opts, "size", &present, &d) != 0) {
        return type_error(ctx, "size must be a finite number");
    }
    if (!present) {
        return type_error(ctx, "createFont requires size");
    }
    if (!(d > 0)) {
        return range_error(ctx, "size must be > 0");
    }
    fo.size = (float)d;

    uint32_t *cps = NULL;
    int ncp = 0;
    JSValue glyphs = JS_GetPropertyStr(ctx, opts, "glyphs");
    if (!JS_IsUndefined(glyphs)) {
        if (!JS_IsString(glyphs)) {
            JS_FreeValue(ctx, glyphs);
            return type_error(ctx, "glyphs must be a string");
        }
        const char *gs = JS_ToCString(ctx, glyphs);
        JS_FreeValue(ctx, glyphs);
        if (!gs) return JS_EXCEPTION;
        ncp = efx_text_codepoints(gs, &cps);
        JS_FreeCString(ctx, gs);
        if (ncp < 0) return generic_error(ctx, "out of memory");
        if (ncp == 0) {
            free(cps);
            return range_error(ctx, "glyphs must not be empty");
        }
        fo.codepoints = cps;
        fo.codepoint_count = ncp;
    } else {
        JS_FreeValue(ctx, glyphs);
    }

    if (get_opt_number(ctx, opts, "padding", &present, &d) != 0) {
        free(cps);
        return type_error(ctx, "padding must be a finite number");
    }
    if (present) {
        if (d < 0 || d != floor(d)) {
            free(cps);
            return range_error(ctx, "padding must be a non-negative integer");
        }
        fo.padding = (int)d;
    }

    JSValue filter = JS_GetPropertyStr(ctx, opts, "filter");
    if (!JS_IsUndefined(filter)) {
        const char *fs = JS_ToCString(ctx, filter);
        JS_FreeValue(ctx, filter);
        if (!fs) {
            free(cps);
            return JS_EXCEPTION;
        }
        if (strcmp(fs, "linear") == 0) fo.filter = EFX_FILTER_LINEAR;
        else if (strcmp(fs, "nearest") == 0) fo.filter = EFX_FILTER_NEAREST;
        else {
            JS_FreeCString(ctx, fs);
            free(cps);
            return type_error(ctx, "filter must be 'linear' or 'nearest'");
        }
        JS_FreeCString(ctx, fs);
    } else {
        JS_FreeValue(ctx, filter);
    }

    JSValue outline = JS_GetPropertyStr(ctx, opts, "outline");
    if (!JS_IsUndefined(outline) && !JS_IsNull(outline)) {
        if (!JS_IsObject(outline)) {
            JS_FreeValue(ctx, outline);
            free(cps);
            return type_error(ctx, "outline must be an object or null");
        }
        static const char *ok[] = {"width"};
        if (check_known_fields(ctx, outline, ok, 1, "outline") != 0) {
            JS_FreeValue(ctx, outline);
            free(cps);
            return JS_EXCEPTION;
        }
        if (get_opt_number(ctx, outline, "width", &present, &d) != 0 ||
            !present) {
            JS_FreeValue(ctx, outline);
            free(cps);
            return type_error(ctx, "outline requires a numeric width");
        }
        if (!(d > 0)) {
            JS_FreeValue(ctx, outline);
            free(cps);
            return range_error(ctx, "outline width must be > 0");
        }
        fo.effects.has_outline = 1;
        fo.effects.outline_width = (float)d;
    }
    JS_FreeValue(ctx, outline);

    JSValue shadow = JS_GetPropertyStr(ctx, opts, "shadow");
    if (!JS_IsUndefined(shadow) && !JS_IsNull(shadow)) {
        if (!JS_IsObject(shadow)) {
            JS_FreeValue(ctx, shadow);
            free(cps);
            return type_error(ctx, "shadow must be an object or null");
        }
        static const char *sk[] = {"blur", "offset"};
        if (check_known_fields(ctx, shadow, sk, 2, "shadow") != 0) {
            JS_FreeValue(ctx, shadow);
            free(cps);
            return JS_EXCEPTION;
        }
        if (get_opt_number(ctx, shadow, "blur", &present, &d) != 0 ||
            !present) {
            JS_FreeValue(ctx, shadow);
            free(cps);
            return type_error(ctx, "shadow requires a numeric blur");
        }
        if (!(d > 0)) {
            JS_FreeValue(ctx, shadow);
            free(cps);
            return range_error(ctx, "shadow blur must be > 0");
        }
        fo.effects.has_shadow = 1;
        fo.effects.shadow_blur = (float)d;
        JSValue off = JS_GetPropertyStr(ctx, shadow, "offset");
        if (!JS_IsUndefined(off)) {
            float o[2];
            if (get_float_array(ctx, off, o, 2) != 0) {
                JS_FreeValue(ctx, off);
                JS_FreeValue(ctx, shadow);
                free(cps);
                return JS_EXCEPTION;
            }
            fo.effects.shadow_offset[0] = o[0];
            fo.effects.shadow_offset[1] = o[1];
        }
        JS_FreeValue(ctx, off);
    }
    JS_FreeValue(ctx, shadow);

    int err = EFX_TEXT_OK;
    efx_text_font *font = efx_text_font_create(fdw->fd, &fo, &err);
    free(cps);
    if (!font) {
        return text_error(ctx, err, "font could not be baked");
    }
    efxjs_font *wrap = calloc(1, sizeof(*wrap));
    if (!wrap) {
        efx_text_font_destroy(font);
        return generic_error(ctx, "out of memory");
    }
    wrap->font = font;
    wrap->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, font_class_id);
    JS_SetOpaque(obj, wrap);
    return obj;
}

static efxjs_font *live_font(JSContext *ctx, JSValueConst v) {
    efxjs_font *f = JS_GetOpaque2(ctx, v, font_class_id);
    if (!f || !f->alive) return NULL;
    return f;
}

JSValue efx_js_measureText(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv) {
    (void)this_val;
    if (argc < 2 || !JS_IsString(argv[0])) {
        return type_error(ctx, "measureText requires (text, font, opts?)");
    }
    efxjs_font *f = live_font(ctx, argv[1]);
    if (!f) {
        return type_error(ctx, "measureText requires a live Font");
    }
    efx_text_layout_opts lo;
    if (parse_layout_opts(ctx, argc >= 3 ? argv[2] : JS_UNDEFINED, &lo) != 0) {
        return JS_EXCEPTION;
    }
    const char *text = JS_ToCString(ctx, argv[0]);
    if (!text) return JS_EXCEPTION;
    efx_text_bounds b;
    int rc = efx_text_measure(f->font, text, &lo, &b);
    JS_FreeCString(ctx, text);
    if (rc != EFX_TEXT_OK) {
        return text_error(ctx, rc, "text measurement failed");
    }
    return bounds_object(ctx, &b);
}

JSValue efx_js_drawText(JSContext *ctx, JSValueConst this_val, int argc,
                        JSValueConst *argv) {
    (void)this_val;
    if (argc < 4 || !JS_IsString(argv[0])) {
        return type_error(ctx, "drawText requires (text, font, x, y, opts?)");
    }
    efxjs_font *f = live_font(ctx, argv[1]);
    if (!f) {
        return type_error(ctx, "drawText requires a live Font");
    }
    double x = 0, y = 0;
    if (JS_ToFloat64(ctx, &x, argv[2]) < 0 || !isfinite(x) ||
        JS_ToFloat64(ctx, &y, argv[3]) < 0 || !isfinite(y)) {
        return type_error(ctx, "drawText requires finite x and y");
    }
    JSValueConst opts = argc >= 5 ? argv[4] : JS_UNDEFINED;
    efx_text_layout_opts lo;
    if (parse_layout_opts(ctx, opts, &lo) != 0) return JS_EXCEPTION;

    float color[4] = {1, 1, 1, 1};
    float outline_color[4] = {0, 0, 0, 1};
    float shadow_color[4] = {0, 0, 0, 1};
    if (JS_IsObject(opts)) {
        JSValue cv = JS_GetPropertyStr(ctx, opts, "color");
        if (!JS_IsUndefined(cv)) {
            if (get_float_array(ctx, cv, color, 4) != 0) {
                JS_FreeValue(ctx, cv);
                return JS_EXCEPTION;
            }
        }
        JS_FreeValue(ctx, cv);
        JSValue ov = JS_GetPropertyStr(ctx, opts, "outlineColor");
        if (!JS_IsUndefined(ov)) {
            if (get_float_array(ctx, ov, outline_color, 4) != 0) {
                JS_FreeValue(ctx, ov);
                return JS_EXCEPTION;
            }
        }
        JS_FreeValue(ctx, ov);
        JSValue sv = JS_GetPropertyStr(ctx, opts, "shadowColor");
        if (!JS_IsUndefined(sv)) {
            if (get_float_array(ctx, sv, shadow_color, 4) != 0) {
                JS_FreeValue(ctx, sv);
                return JS_EXCEPTION;
            }
        }
        JS_FreeValue(ctx, sv);
    }

    const char *text = JS_ToCString(ctx, argv[0]);
    if (!text) return JS_EXCEPTION;
    efx_text_bounds b;
    int rc = efx_text_draw(f->font, text, (float)x, (float)y, &lo, color,
                           outline_color, shadow_color, &b);
    JS_FreeCString(ctx, text);
    if (rc != EFX_TEXT_OK) {
        return text_error(ctx, rc, "text drawing failed");
    }
    return bounds_object(ctx, &b);
}

/* ================================================= F14 audio bindings */

static int audio_opt_number(JSContext *ctx, JSValueConst opts, const char *key,
                            double *out) {
    JSValue v = JS_GetPropertyStr(ctx, opts, key);
    if (JS_IsUndefined(v) || JS_IsNull(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    double d = 0.0;
    int bad = !JS_IsNumber(v) || JS_ToFloat64(ctx, &d, v) < 0 || !isfinite(d);
    JS_FreeValue(ctx, v);
    if (bad) {
        JS_ThrowTypeError(ctx, "%s must be a finite number", key);
        return -1;
    }
    *out = d;
    return 1;
}

static int audio_opt_bool(JSContext *ctx, JSValueConst opts, const char *key,
                          int *out) {
    JSValue v = JS_GetPropertyStr(ctx, opts, key);
    if (JS_IsUndefined(v) || JS_IsNull(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (!JS_IsBool(v)) {
        JS_FreeValue(ctx, v);
        JS_ThrowTypeError(ctx, "%s must be a boolean", key);
        return -1;
    }
    *out = JS_ToBool(ctx, v) ? 1 : 0;
    JS_FreeValue(ctx, v);
    return 1;
}

/* reads a resource for a loader; on failure throws and returns NULL */
static uint8_t *audio_read_resource(JSContext *ctx, JSValueConst pathv,
                                    const char *fn, size_t *out_size) {
    if (!JS_IsString(pathv)) {
        JS_ThrowTypeError(ctx, "%s requires a path string", fn);
        return NULL;
    }
    const char *path = JS_ToCString(ctx, pathv);
    if (!path) {
        return NULL;
    }
    struct efx_host_state *h = host_state(ctx);
    if (!h->resource) {
        JS_FreeCString(ctx, path);
        JS_ThrowInternalError(ctx, "%s requires a resource root", fn);
        return NULL;
    }
    size_t size = 0;
    int rerr = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(h->resource, path, &size, &rerr);
    if (!bytes) {
        JS_ThrowInternalError(ctx, "cannot read audio: %s", path);
    }
    JS_FreeCString(ctx, path);
    *out_size = size;
    return bytes;
}

/* ---- playback handle ---- */

static efxjs_audio *audio_handle(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    if (!a || !a->alive) {
        return NULL;
    }
    return a;
}

static int audio_handle_live(const efxjs_audio *a) {
    return a->voice >= 0 && efx_audio_voice_serial(a->voice) == a->serial;
}

static JSValue audio_handle_stop(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_audio *a = audio_handle(ctx, this_val);
    if (!a) {
        return type_error(ctx, "not an Audio handle");
    }
    if (audio_handle_live(a)) {
        efx_audio_stop_voice(a->voice);
    }
    a->voice = -1;
    return JS_UNDEFINED;
}

static JSValue audio_handle_pause(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_audio *a = audio_handle(ctx, this_val);
    if (!a) {
        return type_error(ctx, "not an Audio handle");
    }
    if (audio_handle_live(a)) {
        efx_audio_set_voice_paused(a->voice, 1);
    }
    return JS_UNDEFINED;
}

static JSValue audio_handle_resume(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_audio *a = audio_handle(ctx, this_val);
    if (!a) {
        return type_error(ctx, "not an Audio handle");
    }
    if (audio_handle_live(a)) {
        efx_audio_set_voice_paused(a->voice, 0);
    }
    return JS_UNDEFINED;
}

static JSValue audio_handle_get_playing(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = audio_handle(ctx, this_val);
    if (!a) {
        return JS_FALSE;
    }
    return JS_NewBool(ctx, audio_handle_live(a) &&
                               efx_audio_voice_playing(a->voice));
}

static JSValue audio_handle_get_paused(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = audio_handle(ctx, this_val);
    if (!a) {
        return JS_FALSE;
    }
    return JS_NewBool(ctx, audio_handle_live(a) &&
                               efx_audio_voice_paused(a->voice));
}

static JSValue audio_handle_get_volume(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    return JS_NewFloat64(ctx, a ? (double)a->volume : 0.0);
}

static JSValue audio_handle_set_volume(JSContext *ctx, JSValueConst this_val,
                                       JSValueConst val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    if (!a) {
        return type_error(ctx, "not an Audio handle");
    }
    double d = 0.0;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d) || d < 0.0) {
        return range_error(ctx, "volume must be a non-negative number");
    }
    a->volume = (float)d;
    if (audio_handle_live(a)) {
        efx_audio_set_voice_volume(a->voice, (float)d);
    }
    return JS_UNDEFINED;
}

static JSValue audio_handle_get_pan(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    return JS_NewFloat64(ctx, a ? (double)a->pan : 0.0);
}

static JSValue audio_handle_set_pan(JSContext *ctx, JSValueConst this_val,
                                    JSValueConst val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    if (!a) {
        return type_error(ctx, "not an Audio handle");
    }
    double d = 0.0;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d)) {
        return range_error(ctx, "pan must be a finite number");
    }
    a->pan = (float)d;
    if (audio_handle_live(a)) {
        efx_audio_set_voice_pan(a->voice, (float)d);
    }
    return JS_UNDEFINED;
}

static JSValue audio_handle_get_pitch(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    return JS_NewFloat64(ctx, a ? (double)a->pitch : 1.0);
}

static JSValue audio_handle_set_pitch(JSContext *ctx, JSValueConst this_val,
                                      JSValueConst val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    if (!a) {
        return type_error(ctx, "not an Audio handle");
    }
    double d = 0.0;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d) || d <= 0.0) {
        return range_error(ctx, "pitch must be a positive number");
    }
    a->pitch = (float)d;
    if (audio_handle_live(a)) {
        efx_audio_set_voice_pitch(a->voice, (float)d);
    }
    return JS_UNDEFINED;
}

static JSValue audio_handle_get_loop(JSContext *ctx, JSValueConst this_val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    return JS_NewBool(ctx, a ? a->loop : 0);
}

static JSValue audio_handle_set_loop(JSContext *ctx, JSValueConst this_val,
                                     JSValueConst val) {
    efxjs_audio *a = JS_GetOpaque2(ctx, this_val, audio_class_id);
    if (!a) {
        return type_error(ctx, "not an Audio handle");
    }
    int loop = JS_ToBool(ctx, val) ? 1 : 0;
    a->loop = loop;
    if (audio_handle_live(a)) {
        efx_audio_set_voice_loop(a->voice, loop);
    }
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry audio_proto_funcs[] = {
    JS_CFUNC_DEF("stop", 0, audio_handle_stop),
    JS_CFUNC_DEF("pause", 0, audio_handle_pause),
    JS_CFUNC_DEF("resume", 0, audio_handle_resume),
    JS_CGETSET_DEF("playing", audio_handle_get_playing, NULL),
    JS_CGETSET_DEF("paused", audio_handle_get_paused, NULL),
    JS_CGETSET_DEF("volume", audio_handle_get_volume, audio_handle_set_volume),
    JS_CGETSET_DEF("pan", audio_handle_get_pan, audio_handle_set_pan),
    JS_CGETSET_DEF("pitch", audio_handle_get_pitch, audio_handle_set_pitch),
    JS_CGETSET_DEF("loop", audio_handle_get_loop, audio_handle_set_loop),
};

/* ---- namespace entry points ---- */

JSValue efx_js_audio_loadAudioData(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "loadAudioData requires a path string");
    }
    size_t size = 0;
    uint8_t *bytes = audio_read_resource(ctx, argv[0], "loadAudioData", &size);
    if (!bytes) {
        return JS_EXCEPTION;
    }
    int derr = 0;
    efx_audio_data *data = efx_audio_data_load(bytes, size, &derr);
    efx_resource_free(bytes);
    if (!data) {
        return generic_error(ctx, "cannot decode audio");
    }
    efxjs_audiodata *o = calloc(1, sizeof(*o));
    if (!o) {
        efx_audio_data_release(data);
        return generic_error(ctx, "out of memory");
    }
    o->data = data;
    o->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, audiodata_class_id);
    JS_SetOpaque(obj, o);
    return obj;
}

JSValue efx_js_audio_loadAudioStream(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "loadAudioStream requires a path string");
    }
    size_t size = 0;
    uint8_t *bytes =
        audio_read_resource(ctx, argv[0], "loadAudioStream", &size);
    if (!bytes) {
        return JS_EXCEPTION;
    }
    int derr = 0;
    efx_audio_stream *stream = efx_audio_stream_load(bytes, size, &derr);
    efx_resource_free(bytes);
    if (!stream) {
        return generic_error(ctx, "cannot decode audio");
    }
    efxjs_audiostream *o = calloc(1, sizeof(*o));
    if (!o) {
        efx_audio_stream_release(stream);
        return generic_error(ctx, "out of memory");
    }
    o->stream = stream;
    o->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, audiostream_class_id);
    JS_SetOpaque(obj, o);
    return obj;
}

JSValue efx_js_audio_playAudio(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "playAudio requires an AudioData or AudioStream");
    }
    efxjs_audiodata *ad = JS_GetOpaque(argv[0], audiodata_class_id);
    efxjs_audiostream *as = JS_GetOpaque(argv[0], audiostream_class_id);
    if (!ad && !as) {
        return type_error(ctx, "playAudio requires an AudioData or AudioStream");
    }
    float volume = 1.0f;
    float pan = 0.0f;
    float pitch = 1.0f;
    int loop = 0;
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        if (!JS_IsObject(argv[1])) {
            return type_error(ctx, "playAudio options must be an object");
        }
        static const char *known[] = {"volume", "pan", "pitch", "loop"};
        if (check_known_fields(ctx, argv[1], known, 4, "playAudio") != 0) {
            return JS_EXCEPTION;
        }
        double n = 0.0;
        int r;
        if ((r = audio_opt_number(ctx, argv[1], "volume", &n)) < 0) {
            return JS_EXCEPTION;
        }
        if (r) {
            if (n < 0.0) {
                return range_error(ctx, "volume must be a non-negative number");
            }
            volume = (float)n;
        }
        if ((r = audio_opt_number(ctx, argv[1], "pan", &n)) < 0) {
            return JS_EXCEPTION;
        }
        if (r) {
            pan = (float)n;
        }
        if ((r = audio_opt_number(ctx, argv[1], "pitch", &n)) < 0) {
            return JS_EXCEPTION;
        }
        if (r) {
            pitch = (n > 0.0) ? (float)n : 1.0f;
        }
        if ((r = audio_opt_bool(ctx, argv[1], "loop", &loop)) < 0) {
            return JS_EXCEPTION;
        }
    }
    int voice;
    if (ad) {
        if (!ad->alive || !ad->data) {
            return generic_error(ctx, "AudioData was destroyed");
        }
        voice = efx_audio_play_data(ad->data, volume, pan, pitch, loop);
    } else {
        if (!as->alive || !as->stream) {
            return generic_error(ctx, "AudioStream was destroyed");
        }
        voice = efx_audio_play_stream(as->stream, volume, pan, pitch, loop);
    }
    if (voice < 0) {
        return JS_NULL;
    }
    efxjs_audio *a = calloc(1, sizeof(*a));
    if (!a) {
        efx_audio_stop_voice(voice);
        return generic_error(ctx, "out of memory");
    }
    a->voice = voice;
    a->serial = efx_audio_voice_serial(voice);
    a->alive = 1;
    a->volume = volume;
    a->pan = pan;
    a->pitch = pitch;
    a->loop = loop;
    JSValue obj = JS_NewObjectClass(ctx, audio_class_id);
    JS_SetOpaque(obj, a);
    return obj;
}

static JSValue efx_js_audio_get_master(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    return JS_NewFloat64(ctx, (double)efx_audio_master_volume());
}

static JSValue efx_js_audio_set_master(JSContext *ctx, JSValueConst this_val,
                                       JSValueConst val) {
    (void)this_val;
    double d = 0.0;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d) || d < 0.0) {
        return range_error(ctx, "volume must be a non-negative number");
    }
    efx_audio_set_master_volume((float)d);
    return JS_UNDEFINED;
}

JSValue efx_js_audio_resume(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    efx_audio_request_resume();
    return JS_UNDEFINED;
}

int efx_api_register_audio(JSContext *ctx, JSValueConst efx) {
    static const JSCFunctionListEntry audio_funcs[] = {
        JS_CFUNC_DEF("loadAudioData", 1, efx_js_audio_loadAudioData),
        JS_CFUNC_DEF("loadAudioStream", 1, efx_js_audio_loadAudioStream),
        JS_CFUNC_DEF("playAudio", 2, efx_js_audio_playAudio),
        JS_CGETSET_DEF("volume", efx_js_audio_get_master,
                       efx_js_audio_set_master),
        JS_CFUNC_DEF("resume", 0, efx_js_audio_resume),
    };
    JSValue audio = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, audio, audio_funcs,
                               (int)(sizeof(audio_funcs) /
                                     sizeof(audio_funcs[0])));
    /* JS_SetPropertyStr consumes the value reference */
    JS_SetPropertyStr(ctx, efx, "audio", audio);
    return 0;
}


int efx_api_init(JSContext *ctx) {
    static int registered;
    if (registered) {
        return 0;
    }
    JSRuntime *rt = JS_GetRuntime(ctx);
    if (JS_NewClassID(rt, &texture_class_id) != texture_class_id ||
        JS_NewClassID(rt, &imagedata_class_id) != imagedata_class_id ||
        JS_NewClassID(rt, &meshdata_class_id) != meshdata_class_id ||
        JS_NewClassID(rt, &mesh_class_id) != mesh_class_id ||
        JS_NewClassID(rt, &rendertarget_class_id) != rendertarget_class_id ||
        JS_NewClassID(rt, &fontdata_class_id) != fontdata_class_id ||
        JS_NewClassID(rt, &font_class_id) != font_class_id ||
        JS_NewClassID(rt, &particlesystem_class_id) != particlesystem_class_id ||
        JS_NewClassID(rt, &body_class_id) != body_class_id ||
        JS_NewClassID(rt, &character_class_id) != character_class_id ||
        JS_NewClassID(rt, &audiodata_class_id) != audiodata_class_id ||
        JS_NewClassID(rt, &audiostream_class_id) != audiostream_class_id ||
        JS_NewClassID(rt, &audio_class_id) != audio_class_id) {
        return -1;
    }
    if (JS_NewClass(rt, texture_class_id, &texture_class_def) < 0 ||
        JS_NewClass(rt, imagedata_class_id, &imagedata_class_def) < 0 ||
        JS_NewClass(rt, meshdata_class_id, &meshdata_class_def) < 0 ||
        JS_NewClass(rt, mesh_class_id, &mesh_class_def) < 0 ||
        JS_NewClass(rt, rendertarget_class_id, &rendertarget_class_def) < 0 ||
        JS_NewClass(rt, fontdata_class_id, &fontdata_class_def) < 0 ||
        JS_NewClass(rt, font_class_id, &font_class_def) < 0 ||
        JS_NewClass(rt, particlesystem_class_id,
                    &particlesystem_class_def) < 0 ||
        JS_NewClass(rt, body_class_id, &body_class_def) < 0 ||
        JS_NewClass(rt, character_class_id, &character_class_def) < 0 ||
        JS_NewClass(rt, audiodata_class_id, &audiodata_class_def) < 0 ||
        JS_NewClass(rt, audiostream_class_id, &audiostream_class_def) < 0 ||
        JS_NewClass(rt, audio_class_id, &audio_class_def) < 0) {
        return -1;
    }
    JSValue tex_proto = JS_NewObject(ctx);
    JSValue img_proto = JS_NewObject(ctx);
    JSValue md_proto = JS_NewObject(ctx);
    JSValue mesh_proto = JS_NewObject(ctx);
    JSValue rt_proto = JS_NewObject(ctx);
    JSValue fd_proto = JS_NewObject(ctx);
    JSValue font_proto = JS_NewObject(ctx);
    JSValue ps_proto = JS_NewObject(ctx);
    JSValue body_proto = JS_NewObject(ctx);
    JSValue character_proto = JS_NewObject(ctx);
    JSValue ad_proto = JS_NewObject(ctx);
    JSValue as_proto = JS_NewObject(ctx);
    JSValue audio_proto = JS_NewObject(ctx);
    JSValue m = JS_NewCFunction(ctx, js_destroy_resource, "destroy", 0);
    JS_SetPropertyStr(ctx, tex_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, img_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, md_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, mesh_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, rt_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, fd_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, font_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, ps_proto, "destroy", m);
    JS_SetPropertyStr(ctx, ad_proto, "destroy",
                      JS_NewCFunction(ctx, js_destroy_resource, "destroy", 0));
    JS_SetPropertyStr(ctx, as_proto, "destroy",
                      JS_NewCFunction(ctx, js_destroy_resource, "destroy", 0));
    JS_SetPropertyStr(ctx, audio_proto, "destroy",
                      JS_NewCFunction(ctx, js_destroy_resource, "destroy", 0));
    JS_SetPropertyFunctionList(ctx, tex_proto, texture_proto_funcs,
                               (int)(sizeof(texture_proto_funcs) /
                                     sizeof(texture_proto_funcs[0])));
    JS_SetPropertyFunctionList(ctx, img_proto, imagedata_proto_funcs,
                               (int)(sizeof(imagedata_proto_funcs) /
                                     sizeof(imagedata_proto_funcs[0])));
    JS_SetPropertyFunctionList(ctx, md_proto, meshdata_proto_funcs,
                               (int)(sizeof(meshdata_proto_funcs) /
                                     sizeof(meshdata_proto_funcs[0])));
    JS_SetPropertyFunctionList(ctx, mesh_proto, mesh_proto_funcs,
                               (int)(sizeof(mesh_proto_funcs) /
                                     sizeof(mesh_proto_funcs[0])));
    JS_SetPropertyFunctionList(ctx, rt_proto, rendertarget_proto_funcs,
                               (int)(sizeof(rendertarget_proto_funcs) /
                                     sizeof(rendertarget_proto_funcs[0])));
    JS_SetPropertyFunctionList(ctx, font_proto, font_proto_funcs,
                               (int)(sizeof(font_proto_funcs) /
                                     sizeof(font_proto_funcs[0])));
    JS_SetPropertyFunctionList(ctx, ps_proto, particlesystem_proto_funcs,
                               (int)(sizeof(particlesystem_proto_funcs) /
                                     sizeof(particlesystem_proto_funcs[0])));
    JS_SetPropertyFunctionList(ctx, body_proto, body_proto_funcs,
                               (int)(sizeof(body_proto_funcs) /
                                     sizeof(body_proto_funcs[0])));
    JS_SetPropertyFunctionList(ctx, character_proto, character_proto_funcs,
                               (int)(sizeof(character_proto_funcs) /
                                     sizeof(character_proto_funcs[0])));
    JS_SetPropertyFunctionList(ctx, audio_proto, audio_proto_funcs,
                               (int)(sizeof(audio_proto_funcs) /
                                     sizeof(audio_proto_funcs[0])));
    JS_SetClassProto(ctx, texture_class_id, tex_proto);
    JS_SetClassProto(ctx, imagedata_class_id, img_proto);
    JS_SetClassProto(ctx, meshdata_class_id, md_proto);
    JS_SetClassProto(ctx, mesh_class_id, mesh_proto);
    JS_SetClassProto(ctx, rendertarget_class_id, rt_proto);
    JS_SetClassProto(ctx, fontdata_class_id, fd_proto);
    JS_SetClassProto(ctx, font_class_id, font_proto);
    JS_SetClassProto(ctx, particlesystem_class_id, ps_proto);
    JS_SetClassProto(ctx, body_class_id, body_proto);
    JS_SetClassProto(ctx, character_class_id, character_proto);
    JS_SetClassProto(ctx, audiodata_class_id, ad_proto);
    JS_SetClassProto(ctx, audiostream_class_id, as_proto);
    JS_SetClassProto(ctx, audio_class_id, audio_proto);
    registered = 1;
    return 0;
}

/* -------------------------------------------------------- F2 bindings */

JSValue efx_js_whiteTexture(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    struct efx_host_state *h = host_state(ctx);
    if (!h->has_white_texture) {
        uint64_t handle = efx_render_white_texture();
        if (!handle) {
            return generic_error(ctx, "white texture unavailable");
        }
        efxjs_texture *t = calloc(1, sizeof(efxjs_texture));
        t->handle = handle;
        t->alive = 1;
        t->permanent = 1;
        JSValue obj = JS_NewObjectClass(ctx, texture_class_id);
        JS_SetOpaque(obj, t);
        h->white_texture = obj;
        h->has_white_texture = 1;
    }
    return JS_DupValue(ctx, h->white_texture);
}

JSValue efx_js_setClearColor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "setClearColor requires a [r,g,b,a] array");
    }
    float c[4];
    int rc = get_float_array(ctx, argv[0], c, 4);
    if (rc != 0) {
        return JS_EXCEPTION;
    }
    efx_render_set_clear_color(c);
    return JS_UNDEFINED;
}

JSValue efx_js_setCamera2D(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return type_error(ctx, "setCamera2D requires an options object");
    }
    JSValueConst opts = argv[0];
    efx_camera2d cam;
    memset(&cam, 0, sizeof(cam));
    cam.zoom = 1.0f;
    cam.x = NAN; /* NaN = resolve to frame center at record time */
    cam.y = NAN;

    JSValue frame = JS_GetPropertyStr(ctx, opts, "frame");
    if (!JS_IsUndefined(frame)) {
        float f[2];
        if (get_float_array(ctx, frame, f, 2) != 0) {
            JS_FreeValue(ctx, frame);
            return JS_EXCEPTION;
        }
        if (!(f[0] > 0 && f[1] > 0)) {
            JS_FreeValue(ctx, frame);
            return range_error(ctx, "frame must be positive");
        }
        cam.frame_w = f[0];
        cam.frame_h = f[1];
        if (isnan(cam.x)) {
            cam.x = f[0] * 0.5f;
        }
        if (isnan(cam.y)) {
            cam.y = f[1] * 0.5f;
        }
    }
    JS_FreeValue(ctx, frame);

    static const char *keys[] = {"x", "y", "zoom", "rotation"};
    float *targets[] = {&cam.x, &cam.y, &cam.zoom, &cam.rotation};
    for (int i = 0; i < 4; i++) {
        JSValue v = JS_GetPropertyStr(ctx, opts, keys[i]);
        if (!JS_IsUndefined(v)) {
            double d;
            if (JS_ToFloat64(ctx, &d, v) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, v);
                return type_error(ctx, "camera fields must be finite numbers");
            }
            *targets[i] = (float)d;
        }
        JS_FreeValue(ctx, v);
    }
    if (!(cam.zoom > 0)) {
        return range_error(ctx, "zoom must be > 0");
    }
    efx_render_set_camera(&cam);
    return JS_UNDEFINED;
}

JSValue efx_js_createImageData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return type_error(ctx, "createImageData requires an options object");
    }
    JSValueConst opts = argv[0];
    int32_t w = 0, hgt = 0;
    JSValue wv = JS_GetPropertyStr(ctx, opts, "width");
    JSValue hv = JS_GetPropertyStr(ctx, opts, "height");
    int bad = 0;
    if (JS_ToInt32(ctx, &w, wv) < 0 || JS_ToInt32(ctx, &hgt, hv) < 0) {
        bad = 1;
    }
    JS_FreeValue(ctx, wv);
    JS_FreeValue(ctx, hv);
    if (bad || w <= 0 || hgt <= 0) {
        return range_error(ctx, "width and height must be positive");
    }
    double pw = (double)w * (double)hgt * 4.0;
    if (pw > (double)0x7fffffff) {
        return range_error(ctx, "image too large");
    }

    JSValue pixels = JS_GetPropertyStr(ctx, opts, "pixels");
    if (JS_IsUndefined(pixels)) {
        JS_FreeValue(ctx, pixels);
        return type_error(ctx, "createImageData requires pixels");
    }
    size_t n = (size_t)pw;
    uint8_t *buf = malloc(n);
    if (!buf) {
        JS_FreeValue(ctx, pixels);
        return generic_error(ctx, "out of memory");
    }
    int rc;
    if (JS_IsArray(pixels)) {
        rc = 0;
        for (size_t i = 0; i < n && rc == 0; i++) {
            double d;
            JSValue ev = JS_GetPropertyUint32(ctx, pixels, (uint32_t)i);
            if (JS_ToFloat64(ctx, &d, ev) < 0 || d < 0 || d > 255 || d != (int)d) {
                rc = -2;
            } else {
                buf[i] = (uint8_t)d;
            }
            JS_FreeValue(ctx, ev);
        }
        if (rc != 0) {
            range_error(ctx, "pixel bytes must be integers 0..255");
        }
    } else {
        size_t blen = 0;
        uint8_t *ptr = JS_GetUint8Array(ctx, &blen, pixels);
        if (!ptr) {
            type_error(ctx, "pixels must be an array or typed array");
            rc = -1;
        } else if (blen != n) {
            range_error(ctx, "pixels length must be width*height*4");
            rc = -2;
        } else {
            memcpy(buf, ptr, n);
            rc = 0;
        }
    }
    JS_FreeValue(ctx, pixels);
    if (rc != 0) {
        free(buf);
        return JS_EXCEPTION;
    }

    /* format field: only 'rgba8' (the default) exists in F2 */
    JSValue fmt = JS_GetPropertyStr(ctx, opts, "format");
    int fmt_bad = 0;
    if (!JS_IsUndefined(fmt)) {
        const char *fs = JS_ToCString(ctx, fmt);
        if (!fs || strcmp(fs, "rgba8") != 0) {
            fmt_bad = 1;
        }
        if (fs) {
            JS_FreeCString(ctx, fs);
        }
    }
    JS_FreeValue(ctx, fmt);
    if (fmt_bad) {
        free(buf);
        return range_error(ctx, "unsupported image format (only 'rgba8')");
    }

    /* unknown-field check (typo protection) */
    static const char *known[] = {"width", "height", "pixels", "format"};
    JSPropertyEnum *props = NULL;
    uint32_t nprops = 0;
    if (JS_GetOwnPropertyNames(ctx, &props, &nprops, opts,
                               JS_GPN_STRING_MASK) == 0) {
        int unknown = 0;
        for (uint32_t i = 0; i < nprops; i++) {
            const char *k = JS_AtomToCString(ctx, props[i].atom);
            int ok = 0;
            for (int j = 0; j < 4; j++) {
                if (k && strcmp(k, known[j]) == 0) {
                    ok = 1;
                    break;
                }
            }
            if (!ok) {
                JS_ThrowTypeError(ctx, "unknown option '%s'", k ? k : "?");
                unknown = 1;
            }
            if (k) {
                JS_FreeCString(ctx, k);
            }
            JS_FreeAtom(ctx, props[i].atom);
        }
        js_free(ctx, props);
        if (unknown) {
            free(buf);
            return JS_EXCEPTION;
        }
    }

    efxjs_imagedata *d = calloc(1, sizeof(efxjs_imagedata));
    d->pixels = buf;
    d->w = w;
    d->h = hgt;
    d->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, imagedata_class_id);
    JS_SetOpaque(obj, d);
    return obj;
}

static efxjs_imagedata *get_live_imagedata(JSContext *ctx, JSValueConst v) {
    efxjs_imagedata *d = JS_GetOpaque2(ctx, v, imagedata_class_id);
    if (!d) {
        type_error(ctx, "expected an ImageData");
        return NULL;
    }
    if (!d->alive) {
        type_error(ctx, "using a destroyed resource");
        return NULL;
    }
    return d;
}

static efxjs_rendertarget *get_live_render_target(JSContext *ctx,
                                                  JSValueConst v) {
    efxjs_rendertarget *t = JS_GetOpaque2(ctx, v, rendertarget_class_id);
    if (!t) {
        type_error(ctx, "expected a RenderTarget");
        return NULL;
    }
    if (!t->alive) {
        type_error(ctx, "using a destroyed resource");
        return NULL;
    }
    return t;
}

/* F5a texture coercion: a live Texture or a live RenderTarget — either is
 * accepted wherever a sampling source is required (drawQuad, material
 * maps, alphaMask). Returns 0 and sets *out_handle on success. */
static int get_live_sample(JSContext *ctx, JSValueConst v, uint64_t *out_handle) {
    efxjs_texture *t = JS_GetOpaque(v, texture_class_id);
    if (t) {
        if (!t->alive) {
            type_error(ctx, "using a destroyed resource");
            return -1;
        }
        *out_handle = t->handle;
        return 0;
    }
    efxjs_rendertarget *rt = JS_GetOpaque(v, rendertarget_class_id);
    if (rt) {
        if (!rt->alive) {
            type_error(ctx, "using a destroyed resource");
            return -1;
        }
        *out_handle = rt->handle;
        return 0;
    }
    type_error(ctx, "expected a Texture or RenderTarget");
    return -1;
}

/* map render-module errors from the F5a redirection calls to JS
 * exceptions; returns a JS value to return from the binding */
static JSValue target_call_error(JSContext *ctx, int rc) {
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return type_error(ctx, "expected a live RenderTarget");
    }
    if (rc == EFX_RENDER_ERR_NESTED) {
        return type_error(ctx, "a render target is already active");
    }
    if (rc == EFX_RENDER_ERR_STATE) {
        return type_error(ctx, "no render target is active");
    }
    if (rc == EFX_RENDER_ERR_BUDGET) {
        return range_error(ctx, "display list budget exceeded");
    }
    if (rc == EFX_RENDER_ERR_NOMEM) {
        return generic_error(ctx, "out of memory");
    }
    return generic_error(ctx, "render target call failed");
}

JSValue efx_js_createTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "createTexture requires an ImageData");
    }
    efxjs_imagedata *d = get_live_imagedata(ctx, argv[0]);
    if (!d) {
        return JS_EXCEPTION;
    }
    int wrap = EFX_TEX_WRAP_REPEAT;
    int filter = EFX_FILTER_LINEAR;
    int mipmaps = 0;
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        if (!JS_IsObject(argv[1])) {
            return type_error(ctx, "createTexture options must be an object");
        }
        static const char *known[] = {"wrap", "filter", "mipmaps"};
        if (check_known_fields(ctx, argv[1], known, 3, "createTexture") != 0) {
            return JS_EXCEPTION;
        }
        JSValue wv = JS_GetPropertyStr(ctx, argv[1], "wrap");
        if (!JS_IsUndefined(wv)) {
            const char *s = JS_ToCString(ctx, wv);
            if (!s) {
                JS_FreeValue(ctx, wv);
                return type_error(ctx, "wrap must be a string");
            }
            if (strcmp(s, "repeat") == 0) {
                wrap = EFX_TEX_WRAP_REPEAT;
            } else if (strcmp(s, "clamp") == 0) {
                wrap = EFX_TEX_WRAP_CLAMP;
            } else if (strcmp(s, "mirror") == 0) {
                wrap = EFX_TEX_WRAP_MIRROR;
            } else {
                JS_FreeCString(ctx, s);
                JS_FreeValue(ctx, wv);
                return type_error(ctx, "unknown wrap mode");
            }
            JS_FreeCString(ctx, s);
        }
        JS_FreeValue(ctx, wv);
        JSValue fv = JS_GetPropertyStr(ctx, argv[1], "filter");
        if (!JS_IsUndefined(fv)) {
            const char *s = JS_ToCString(ctx, fv);
            if (!s) {
                JS_FreeValue(ctx, fv);
                return type_error(ctx, "filter must be a string");
            }
            if (strcmp(s, "nearest") == 0) {
                filter = EFX_FILTER_NEAREST;
            } else if (strcmp(s, "linear") == 0) {
                filter = EFX_FILTER_LINEAR;
            } else {
                JS_FreeCString(ctx, s);
                JS_FreeValue(ctx, fv);
                return type_error(ctx, "unknown filter");
            }
            JS_FreeCString(ctx, s);
        }
        JS_FreeValue(ctx, fv);
        JSValue mv = JS_GetPropertyStr(ctx, argv[1], "mipmaps");
        if (!JS_IsUndefined(mv)) {
            if (!JS_IsBool(mv)) {
                JS_FreeValue(ctx, mv);
                return type_error(ctx, "mipmaps must be a boolean");
            }
            mipmaps = JS_ToBool(ctx, mv) ? 1 : 0;
        }
        JS_FreeValue(ctx, mv);
    }
    uint64_t handle = efx_render_texture_create(d->w, d->h, d->pixels, wrap,
                                                filter, mipmaps);
    if (!handle) {
        return generic_error(ctx, "texture upload failed (no GPU context?)");
    }
    efxjs_texture *t = calloc(1, sizeof(efxjs_texture));
    t->handle = handle;
    t->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, texture_class_id);
    JS_SetOpaque(obj, t);
    return obj;
}

/* -------------------------------------------------------- F5a bindings */

JSValue efx_js_createRenderTarget(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return type_error(ctx, "createRenderTarget requires an options object");
    }
    JSValueConst opts = argv[0];
    static const char *known[] = {"width", "height"};
    if (check_known_fields(ctx, opts, known, 2, "createRenderTarget") != 0) {
        return JS_EXCEPTION;
    }
    double w = 0, h = 0;
    static const char *keys[] = {"width", "height"};
    double *outs[] = {&w, &h};
    for (int i = 0; i < 2; i++) {
        JSValue v = JS_GetPropertyStr(ctx, opts, keys[i]);
        if (JS_IsUndefined(v)) {
            JS_FreeValue(ctx, v);
            return type_error(ctx, "createRenderTarget requires width and height");
        }
        int bad = !JS_IsNumber(v) || JS_ToFloat64(ctx, outs[i], v) < 0;
        JS_FreeValue(ctx, v);
        if (bad) {
            return type_error(ctx, "width and height must be numbers");
        }
        if (!isfinite(*outs[i]) || *outs[i] <= 0 ||
            *outs[i] != floor(*outs[i]) ||
            *outs[i] > (double)EFX_RENDER_MAX_TARGET_SIZE) {
            return range_error(
                ctx, "width and height must be integers in 1..4096");
        }
    }
    uint64_t handle = efx_render_target_create((int)w, (int)h);
    if (!handle) {
        return generic_error(ctx, "render target creation failed (no GPU context?)");
    }
    efxjs_rendertarget *t = calloc(1, sizeof(efxjs_rendertarget));
    if (!t) {
        efx_render_target_destroy(handle);
        return generic_error(ctx, "out of memory");
    }
    t->handle = handle;
    t->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, rendertarget_class_id);
    JS_SetOpaque(obj, t);
    return obj;
}

JSValue efx_js_beginRenderTarget(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "beginRenderTarget requires a RenderTarget");
    }
    efxjs_rendertarget *t = get_live_render_target(ctx, argv[0]);
    if (!t) {
        return JS_EXCEPTION;
    }
    int rc = efx_render_begin_target(t->handle);
    if (rc != EFX_RENDER_OK) {
        return target_call_error(ctx, rc);
    }
    return JS_UNDEFINED;
}

JSValue efx_js_endRenderTarget(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    int rc = efx_render_end_target();
    if (rc != EFX_RENDER_OK) {
        return target_call_error(ctx, rc);
    }
    return JS_UNDEFINED;
}

JSValue efx_js_drawQuad(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) {
        return type_error(ctx, "drawQuad requires (x, y, texture, opts?)");
    }
    double x, y;
    if (JS_ToFloat64(ctx, &x, argv[0]) < 0 || JS_ToFloat64(ctx, &y, argv[1]) < 0) {
        return type_error(ctx, "x and y must be numbers");
    }
    if (!isfinite(x) || !isfinite(y)) {
        return range_error(ctx, "x and y must be finite");
    }
    /* F5a texture coercion: a live Texture or a live RenderTarget */
    uint64_t tex_handle = 0;
    if (get_live_sample(ctx, argv[2], &tex_handle) != 0) {
        return JS_EXCEPTION;
    }

    float color[4] = {1, 1, 1, 1};
    float rotation = 0, scale = 1;
    float src[4] = {0, 0, 0, 0};
    int has_src = 0;
    float size[2] = {0, 0};
    int has_size = 0;
    float origin[2] = {0, 0};
    int has_origin = 0;

    if (argc >= 4 && !JS_IsUndefined(argv[3])) {
        if (!JS_IsObject(argv[3])) {
            return type_error(ctx, "opts must be an object");
        }
        JSValueConst opts = argv[3];
        static const char *known[] = {"color", "rotation", "scale", "sourceRect", "size", "origin"};
        JSPropertyEnum *props = NULL;
        uint32_t nprops = 0;
        if (JS_GetOwnPropertyNames(ctx, &props, &nprops, opts,
                                   JS_GPN_STRING_MASK) == 0) {
            for (uint32_t i = 0; i < nprops; i++) {
                const char *k = JS_AtomToCString(ctx, props[i].atom);
                int ok = 0;
                for (int j = 0; j < 6; j++) {
                    if (k && strcmp(k, known[j]) == 0) {
                        ok = 1;
                        break;
                    }
                }
                if (k) {
                    JS_FreeCString(ctx, k);
                }
                JS_FreeAtom(ctx, props[i].atom);
                if (!ok) {
                    for (uint32_t j = i + 1; j < nprops; j++) {
                        JS_FreeAtom(ctx, props[j].atom);
                    }
                    js_free(ctx, props);
                    return JS_ThrowTypeError(ctx, "unknown drawQuad option");
                }
            }
            js_free(ctx, props);
        }

        JSValue cv = JS_GetPropertyStr(ctx, opts, "color");
        if (!JS_IsUndefined(cv)) {
            if (get_float_array(ctx, cv, color, 4) != 0) {
                JS_FreeValue(ctx, cv);
                return JS_EXCEPTION;
            }
        }
        JS_FreeValue(ctx, cv);

        JSValue rv = JS_GetPropertyStr(ctx, opts, "rotation");
        if (!JS_IsUndefined(rv)) {
            double d;
            if (JS_ToFloat64(ctx, &d, rv) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, rv);
                return type_error(ctx, "rotation must be a finite number");
            }
            rotation = (float)d;
        }
        JS_FreeValue(ctx, rv);

        JSValue sv = JS_GetPropertyStr(ctx, opts, "scale");
        if (!JS_IsUndefined(sv)) {
            double d;
            if (JS_ToFloat64(ctx, &d, sv) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, sv);
                return type_error(ctx, "scale must be a finite number");
            }
            if (d <= 0) {
                JS_FreeValue(ctx, sv);
                return range_error(ctx, "scale must be > 0");
            }
            scale = (float)d;
        }
        JS_FreeValue(ctx, sv);

        JSValue zv = JS_GetPropertyStr(ctx, opts, "size");
        if (!JS_IsUndefined(zv)) {
            if (get_float_array(ctx, zv, size, 2) != 0) {
                JS_FreeValue(ctx, zv);
                return JS_EXCEPTION;
            }
            if (size[0] <= 0 || size[1] <= 0) {
                JS_FreeValue(ctx, zv);
                return range_error(ctx, "size entries must be > 0");
            }
            has_size = 1;
        }
        JS_FreeValue(ctx, zv);

        JSValue ov = JS_GetPropertyStr(ctx, opts, "origin");
        if (!JS_IsUndefined(ov)) {
            if (get_float_array(ctx, ov, origin, 2) != 0) {
                JS_FreeValue(ctx, ov);
                return JS_EXCEPTION;
            }
            has_origin = 1;
        }
        JS_FreeValue(ctx, ov);

        JSValue srcv = JS_GetPropertyStr(ctx, opts, "sourceRect");
        if (!JS_IsUndefined(srcv)) {
            if (!JS_IsObject(srcv)) {
                JS_FreeValue(ctx, srcv);
                return type_error(ctx, "sourceRect must be an object");
            }
            static const char *skeys[] = {"x", "y", "w", "h"};
            for (int i = 0; i < 4; i++) {
                JSValue f = JS_GetPropertyStr(ctx, srcv, skeys[i]);
                double d;
                if (JS_ToFloat64(ctx, &d, f) < 0 || !isfinite(d)) {
                    JS_FreeValue(ctx, f);
                    JS_FreeValue(ctx, srcv);
                    return type_error(ctx, "sourceRect fields must be finite numbers");
                }
                JS_FreeValue(ctx, f);
                src[i] = (float)d;
            }
            JS_FreeValue(ctx, srcv);
            if (src[2] <= 0 || src[3] <= 0) {
                return range_error(ctx, "sourceRect extent must be > 0");
            }
            int tw = 0, th = 0;
            efx_render_sample_size(tex_handle, &tw, &th);
            if (src[0] < 0 || src[1] < 0 ||
                src[0] + src[2] > (float)tw || src[1] + src[3] > (float)th) {
                return range_error(ctx, "sourceRect outside texture bounds");
            }
            has_src = 1;
        }
    }

    /* size derivation: explicit size -> sourceRect extent -> texture
     * pixels (a render target's extent plays the texture's role, F5a) */
    float w, h;
    if (has_size) {
        w = size[0];
        h = size[1];
    } else if (has_src) {
        w = src[2];
        h = src[3];
    } else {
        int tw = 0, th = 0;
        efx_render_sample_size(tex_handle, &tw, &th);
        w = (float)tw;
        h = (float)th;
    }
    float origin_x = has_origin ? origin[0] : w * 0.5f;
    float origin_y = has_origin ? origin[1] : h * 0.5f;

    int rc = efx_render_quad((float)x, (float)y, w, h,
                             tex_handle, color, rotation, scale, src, has_src,
                             origin_x, origin_y);
    if (rc == EFX_RENDER_ERR_BUDGET) {
        return range_error(ctx, "display list budget exceeded");
    }
    if (rc == EFX_RENDER_ERR_SINK) {
        return generic_error(ctx, "no render surface (draw calls need a window)");
    }
    if (rc == EFX_RENDER_ERR_FEEDBACK) {
        return type_error(ctx,
                          "cannot sample the render target being drawn into");
    }
    if (rc != EFX_RENDER_OK) {
        return generic_error(ctx, "drawQuad failed");
    }
    return JS_UNDEFINED;
}

JSValue efx_js_setBlendMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "setBlendMode requires a mode string");
    }
    const char *s = JS_ToCString(ctx, argv[0]);
    if (!s) {
        return type_error(ctx, "setBlendMode requires a mode string");
    }
    int mode;
    if (strcmp(s, "alpha") == 0) {
        mode = EFX_BLEND_ALPHA;
    } else if (strcmp(s, "additive") == 0) {
        mode = EFX_BLEND_ADDITIVE;
    } else if (strcmp(s, "subtractive") == 0) {
        mode = EFX_BLEND_SUBTRACTIVE;
    } else {
        JS_FreeCString(ctx, s);
        return type_error(ctx, "unknown blend mode");
    }
    JS_FreeCString(ctx, s);
    efx_render_set_blend(mode);
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------ F3 bindings */

/* read a flat number array (JS array or typed array; elements extracted by
 * index so any typed array kind works). Element rules: non-number →
 * TypeError, non-finite → RangeError (F2 precedent). Returns 0 on success. */
static int read_number_array(JSContext *ctx, JSValueConst v, float **out,
                             int *out_len, const char *what) {
    int is_ta = JS_GetTypedArrayType(v);
    if (!JS_IsArray(v) && is_ta < 0) {
        type_error(ctx, what);
        return -1;
    }
    JSValue lenv = JS_GetPropertyStr(ctx, v, "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    if (len < 0) {
        range_error(ctx, what);
        return -2;
    }
    float *buf = len ? malloc((size_t)len * sizeof(float)) : NULL;
    if (len && !buf) {
        generic_error(ctx, "out of memory");
        return -3;
    }
    for (int32_t i = 0; i < len; i++) {
        JSValue ev = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
        if (!JS_IsNumber(ev)) {
            JS_FreeValue(ctx, ev);
            free(buf);
            type_error(ctx, "array elements must be numbers");
            return -1;
        }
        double d = 0;
        if (JS_ToFloat64(ctx, &d, ev) < 0) {
            /* exception already pending on the context */
            JS_FreeValue(ctx, ev);
            free(buf);
            return -4;
        }
        JS_FreeValue(ctx, ev);
        if (!isfinite(d)) {
            free(buf);
            range_error(ctx, "array elements must be finite numbers");
            return -2;
        }
        buf[i] = (float)d;
    }
    *out = buf;
    *out_len = (int)len;
    return 0;
}

/* indices: non-negative integers in uint32 range; non-integer → RangeError
 * (the F2 pixel-bytes precedent). `what` names the field for messages. */
static int read_index_array(JSContext *ctx, JSValueConst v, uint32_t **out,
                            int *out_len, const char *what) {
    int is_ta = JS_GetTypedArrayType(v);
    if (!JS_IsArray(v) && is_ta < 0) {
        type_error(ctx, what);
        return -1;
    }
    JSValue lenv = JS_GetPropertyStr(ctx, v, "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    if (len < 0) {
        range_error(ctx, what);
        return -2;
    }
    uint32_t *buf = len ? malloc((size_t)len * sizeof(uint32_t)) : NULL;
    if (len && !buf) {
        generic_error(ctx, "out of memory");
        return -3;
    }
    for (int32_t i = 0; i < len; i++) {
        JSValue ev = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
        double d = 0;
        int bad = !JS_IsNumber(ev) || JS_ToFloat64(ctx, &d, ev) < 0;
        JS_FreeValue(ctx, ev);
        if (bad) {
            free(buf);
            type_error(ctx, "array elements must be numbers");
            return -1;
        }
        if (!isfinite(d) || d < 0 || d > 4294967295.0 || d != floor(d)) {
            free(buf);
            range_error(ctx, "array elements must be integers in [0, 2^32-1]");
            return -2;
        }
        buf[i] = (uint32_t)d;
    }
    *out = buf;
    *out_len = (int)len;
    return 0;
}

/* reject unknown fields on an object with a TypeError naming the field */
static int check_known_fields(JSContext *ctx, JSValueConst obj,
                              const char **known, int nknown,
                              const char *where) {
    JSPropertyEnum *props = NULL;
    uint32_t nprops = 0;
    if (JS_GetOwnPropertyNames(ctx, &props, &nprops, obj,
                               JS_GPN_STRING_MASK) != 0) {
        return -1;
    }
    int rc = 0;
    for (uint32_t i = 0; i < nprops; i++) {
        const char *k = JS_AtomToCString(ctx, props[i].atom);
        int ok = 0;
        for (int j = 0; j < nknown; j++) {
            if (k && strcmp(k, known[j]) == 0) {
                ok = 1;
                break;
            }
        }
        if (!ok) {
            JS_ThrowTypeError(ctx, "unknown %s option '%s'", where,
                              k ? k : "?");
            rc = -1;
        }
        if (k) {
            JS_FreeCString(ctx, k);
        }
        JS_FreeAtom(ctx, props[i].atom);
        if (rc != 0) {
            for (uint32_t j = i + 1; j < nprops; j++) {
                JS_FreeAtom(ctx, props[j].atom);
            }
            break;
        }
    }
    js_free(ctx, props);
    return rc;
}

static const char *MD_KEYS[] = {"positions", "normals", "uvs",
                                "colors", "joints", "weights", "indices"};
static const char *MD_KEYS_MAT[] = {"positions", "normals", "uvs",
                                    "colors", "joints", "weights",
                                    "indices", "materials"};

/* read a [x,y,z] array (array or typed array) */
static int read_vec3(JSContext *ctx, JSValueConst v, float out[3],
                     const char *what) {
    float *buf = NULL;
    int len = 0;
    if (read_number_array(ctx, v, &buf, &len, what) != 0) {
        return -1;
    }
    if (len != 3) {
        free(buf);
        range_error(ctx, "expected 3 numbers");
        return -1;
    }
    memcpy(out, buf, sizeof(float) * 3);
    free(buf);
    return 0;
}

/* parse one Phong channel color: required 4-element array */
static int read_channel_color(JSContext *ctx, JSValueConst channel,
                              const char *name, float out[4]) {
    JSValue cv = JS_GetPropertyStr(ctx, channel, "color");
    if (JS_IsUndefined(cv)) {
        JS_FreeValue(ctx, cv);
        JS_ThrowTypeError(ctx, "%s channel requires color", name);
        return -1;
    }
    int rc = get_float_array(ctx, cv, out, 4);
    JS_FreeValue(ctx, cv);
    return rc == 0 ? 0 : -1;
}

/* parse a material map field (present = live Texture or RenderTarget, F5a;
 * null/omitted = none) */
static int read_material_map(JSContext *ctx, JSValueConst ch, const char *name,
                             uint64_t *out) {
    JSValue mv = JS_GetPropertyStr(ctx, ch, "map");
    if (JS_IsUndefined(mv) || JS_IsNull(mv)) {
        JS_FreeValue(ctx, mv);
        *out = 0;
        return 0;
    }
    int rc = get_live_sample(ctx, mv, out);
    JS_FreeValue(ctx, mv);
    if (rc != 0) {
        (void)name;
        return -1;
    }
    return 0;
}

/* parse a material object into the engine snapshot (F4a/F4b spec: channel
 * defaults, specular.shininess, per-channel maps, alphaMask, unknown-field
 * rejection) */
static int read_material(JSContext *ctx, JSValueConst v, efx_material *out) {
    if (!JS_IsObject(v)) {
        type_error(ctx, "material must be an object");
        return -1;
    }
    efx_material_default(out);
    static const char *known[] = {"ambient", "diffuse", "specular", "emissive",
                                  "alphaMask"};
    if (check_known_fields(ctx, v, known, 5, "material") != 0) {
        return -1;
    }
    static const char *chan_keys[] = {"ambient", "diffuse", "specular", "emissive"};
    float *outs[] = {out->ambient, out->diffuse, out->specular, out->emissive};
    uint64_t *mouts[] = {&out->ambient_map, &out->diffuse_map,
                         &out->specular_map, &out->emissive_map};
    for (int i = 0; i < 4; i++) {
        JSValue ch = JS_GetPropertyStr(ctx, v, chan_keys[i]);
        if (JS_IsUndefined(ch) || JS_IsNull(ch)) {
            JS_FreeValue(ctx, ch);
            continue;
        }
        if (!JS_IsObject(ch)) {
            JS_FreeValue(ctx, ch);
            JS_ThrowTypeError(ctx, "%s channel must be an object", chan_keys[i]);
            return -1;
        }
        static const char *spec_keys[] = {"color", "shininess", "map"};
        static const char *color_map_keys[] = {"color", "map"};
        const char **ck;
        int nk;
        if (i == 2) {
            ck = spec_keys;
            nk = 3;
        } else {
            ck = color_map_keys;
            nk = 2;
        }
        if (check_known_fields(ctx, ch, ck, nk, chan_keys[i]) != 0) {
            JS_FreeValue(ctx, ch);
            return -1;
        }
        if (read_channel_color(ctx, ch, chan_keys[i], outs[i]) != 0) {
            JS_FreeValue(ctx, ch);
            return -1;
        }
        if (read_material_map(ctx, ch, chan_keys[i], mouts[i]) != 0) {
            JS_FreeValue(ctx, ch);
            return -1;
        }
        if (i == 2) {
            JSValue sv = JS_GetPropertyStr(ctx, ch, "shininess");
            if (!JS_IsUndefined(sv)) {
                double d = 0;
                int bad = !JS_IsNumber(sv) || JS_ToFloat64(ctx, &d, sv) < 0;
                JS_FreeValue(ctx, sv);
                if (bad) {
                    JS_FreeValue(ctx, ch);
                    type_error(ctx, "shininess must be a number");
                    return -1;
                }
                if (!isfinite(d) || d <= 0) {
                    JS_FreeValue(ctx, ch);
                    range_error(ctx, "shininess must be Finite and > 0");
                    return -1;
                }
                out->shininess = (float)d;
            } else {
                JS_FreeValue(ctx, sv);
            }
        }
        JS_FreeValue(ctx, ch);
    }
    JSValue am = JS_GetPropertyStr(ctx, v, "alphaMask");
    if (!JS_IsUndefined(am) && !JS_IsNull(am)) {
        int rc = get_live_sample(ctx, am, &out->alpha_mask);
        JS_FreeValue(ctx, am);
        if (rc != 0) {
            return -1;
        }
    } else {
        JS_FreeValue(ctx, am);
    }
    return 0;
}

/* buffers extracted from JS for one createMeshData call; every allocation
 * is registered here the moment it exists so a single release path frees
 * each exactly once on success and on every error exit */
typedef struct {
    float *f[EFX_MESH_MAX_SURFACES * 5];
    uint32_t *i[EFX_MESH_MAX_SURFACES];
    uint32_t *j[EFX_MESH_MAX_SURFACES];
    int nf, ni, nj;
} md_owned;

static void md_owned_free(md_owned *o) {
    for (int k = 0; k < o->nf; k++) {
        free(o->f[k]);
    }
    for (int k = 0; k < o->ni; k++) {
        free(o->i[k]);
    }
    for (int k = 0; k < o->nj; k++) {
        free(o->j[k]);
    }
    o->nf = 0;
    o->ni = 0;
    o->nj = 0;
}

/* extract one surface object into an efx_surface_src; buffers are owned
 * by *own (the caller releases them, on success and on failure alike) */
static int read_surface(JSContext *ctx, JSValueConst obj, efx_surface_src *s,
                        md_owned *own, int allow_materials) {
    memset(s, 0, sizeof(*s));
    if (!JS_IsObject(obj)) {
        type_error(ctx, "surfaces must be objects");
        return -1;
    }
    if (allow_materials
            ? check_known_fields(ctx, obj, MD_KEYS_MAT, 8, "surface") != 0
            : check_known_fields(ctx, obj, MD_KEYS, 7, "surface") != 0) {
        return -1;
    }
    static const char *keys[] = {"positions", "normals", "uvs", "colors"};
    float *bufs[4] = {NULL, NULL, NULL, NULL};
    int lens[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
        JSValue v = JS_GetPropertyStr(ctx, obj, keys[i]);
        if (JS_IsUndefined(v)) {
            JS_FreeValue(ctx, v);
            continue;
        }
        int rc = read_number_array(ctx, v, &bufs[i], &lens[i], keys[i]);
        JS_FreeValue(ctx, v);
        if (rc != 0) {
            return -1;
        }
        own->f[own->nf++] = bufs[i];
    }
    s->positions = bufs[0];
    s->positions_len = lens[0];
    s->normals = bufs[1];
    s->normals_len = lens[1];
    s->uvs = bufs[2];
    s->uvs_len = lens[2];
    s->colors = bufs[3];
    s->colors_len = lens[3];
    /* F6c skinned attributes: joints are integer indices, weights finite
     * floats; pairing/count validation happens in efx_meshdata_create */
    JSValue jv = JS_GetPropertyStr(ctx, obj, "joints");
    if (!JS_IsUndefined(jv)) {
        uint32_t *jb = NULL;
        int jl = 0;
        int rc = read_index_array(ctx, jv, &jb, &jl, "joints");
        JS_FreeValue(ctx, jv);
        if (rc != 0) {
            return -1;
        }
        s->joints = jb;
        s->joints_len = jl;
        own->j[own->nj++] = jb;
    } else {
        JS_FreeValue(ctx, jv);
    }
    JSValue wv = JS_GetPropertyStr(ctx, obj, "weights");
    if (!JS_IsUndefined(wv)) {
        float *wb = NULL;
        int wl = 0;
        int rc = read_number_array(ctx, wv, &wb, &wl, "weights");
        JS_FreeValue(ctx, wv);
        if (rc != 0) {
            return -1;
        }
        s->weights = wb;
        s->weights_len = wl;
        own->f[own->nf++] = wb;
    } else {
        JS_FreeValue(ctx, wv);
    }
    JSValue iv = JS_GetPropertyStr(ctx, obj, "indices");
    if (!JS_IsUndefined(iv)) {
        uint32_t *ibuf = NULL;
        int ilen = 0;
        int rc = read_index_array(ctx, iv, &ibuf, &ilen, "indices");
        JS_FreeValue(ctx, iv);
        if (rc != 0) {
            return -1;
        }
        s->indices = ibuf;
        s->indices_len = ilen;
        own->i[own->ni++] = ibuf;
    }
    if (!s->positions) {
        type_error(ctx, "surface requires positions");
        return -1;
    }
    return 0;
}

JSValue efx_js_createMeshData(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return type_error(ctx, "createMeshData requires an options object");
    }
    JSValueConst opts = argv[0];
    static const char *bag_keys[] = {"surfaces", "positions", "normals",
                                     "uvs", "colors", "joints", "weights",
                                     "indices", "materials"};
    if (check_known_fields(ctx, opts, bag_keys, 9, "createMeshData") != 0) {
        return JS_EXCEPTION;
    }

    JSValue surfaces = JS_GetPropertyStr(ctx, opts, "surfaces");
    JSValue positions = JS_GetPropertyStr(ctx, opts, "positions");
    int has_surfaces = !JS_IsUndefined(surfaces);
    int has_positions = !JS_IsUndefined(positions);
    if (has_surfaces && has_positions) {
        JS_FreeValue(ctx, surfaces);
        JS_FreeValue(ctx, positions);
        return type_error(ctx,
                          "pass either surfaces or single-surface fields");
    }
    if (!has_surfaces && !has_positions) {
        JS_FreeValue(ctx, surfaces);
        JS_FreeValue(ctx, positions);
        return type_error(ctx, "createMeshData requires surfaces");
    }

    efx_surface_src src[EFX_MESH_MAX_SURFACES];
    md_owned own;
    memset(&own, 0, sizeof(own));
    int count = 0;

    if (has_surfaces) {
        if (!JS_IsArray(surfaces)) {
            JS_FreeValue(ctx, surfaces);
            JS_FreeValue(ctx, positions);
            return type_error(ctx, "surfaces must be an array");
        }
        JSValue lenv = JS_GetPropertyStr(ctx, surfaces, "length");
        int32_t len = -1;
        JS_ToInt32(ctx, &len, lenv);
        JS_FreeValue(ctx, lenv);
        JS_FreeValue(ctx, positions);
        if (len < 1 || len > EFX_MESH_MAX_SURFACES) {
            JS_FreeValue(ctx, surfaces);
            return range_error(ctx, "surfaces must hold 1..16 entries");
        }
        for (int32_t i = 0; i < len; i++) {
            JSValue sv = JS_GetPropertyUint32(ctx, surfaces, (uint32_t)i);
            int rc = read_surface(ctx, sv, &src[i], &own, 0);
            JS_FreeValue(ctx, sv);
            if (rc != 0) {
                JS_FreeValue(ctx, surfaces);
                goto fail;
            }
            count++;
        }
        JS_FreeValue(ctx, surfaces);
    } else {
        JS_FreeValue(ctx, surfaces);
        /* the bag itself is the single surface */
        int rc = read_surface(ctx, opts, &src[0], &own, 1);
        JS_FreeValue(ctx, positions);
        if (rc != 0) {
            goto fail;
        }
        count = 1;
    }

    /* F4a: optional parallel materials array (one entry per surface) */
    efx_material mats[EFX_MESH_MAX_SURFACES];
    uint8_t mat_has[EFX_MESH_MAX_SURFACES];
    memset(mat_has, 0, sizeof(mat_has));
    JSValue materials = JS_GetPropertyStr(ctx, opts, "materials");
    if (!JS_IsUndefined(materials)) {
        if (!JS_IsArray(materials)) {
            JS_FreeValue(ctx, materials);
            type_error(ctx, "materials must be an array");
            goto fail;
        }
        JSValue mlenv = JS_GetPropertyStr(ctx, materials, "length");
        int32_t mlen = -1;
        JS_ToInt32(ctx, &mlen, mlenv);
        JS_FreeValue(ctx, mlenv);
        if (mlen != count) {
            JS_FreeValue(ctx, materials);
            range_error(ctx, "materials must have one entry per surface");
            goto fail;
        }
        for (int32_t i = 0; i < count; i++) {
            JSValue mv = JS_GetPropertyUint32(ctx, materials, (uint32_t)i);
            if (JS_IsNull(mv) || JS_IsUndefined(mv)) {
                JS_FreeValue(ctx, mv);
                continue;
            }
            if (read_material(ctx, mv, &mats[i]) != 0) {
                JS_FreeValue(ctx, mv);
                JS_FreeValue(ctx, materials);
                goto fail;
            }
            mat_has[i] = 1;
            JS_FreeValue(ctx, mv);
        }
    }
    JS_FreeValue(ctx, materials);

    {
        int err = 0;
        efx_meshdata *md = efx_meshdata_create(src, count, &err);
        md_owned_free(&own);
        if (!md) {
            if (err == EFX_MESHERR_COUNT || err == EFX_MESHERR_LEN ||
                err == EFX_MESHERR_INDEX) {
                return range_error(ctx, "invalid mesh data");
            }
            return generic_error(ctx, "out of memory");
        }
        for (int i = 0; i < count; i++) {
            efx_meshdata_set_material(md, i, mat_has[i] ? &mats[i] : NULL,
                                      mat_has[i]);
        }
        efxjs_meshdata *wrap = calloc(1, sizeof(efxjs_meshdata));
        if (!wrap) {
            efx_meshdata_destroy(md);
            return generic_error(ctx, "out of memory");
        }
        wrap->md = md;
        wrap->alive = 1;
        JSValue obj = JS_NewObjectClass(ctx, meshdata_class_id);
        JS_SetOpaque(obj, wrap);
        return obj;
    }

fail:
    md_owned_free(&own);
    return JS_EXCEPTION;
}

static efxjs_meshdata *get_live_meshdata(JSContext *ctx, JSValueConst v) {
    efxjs_meshdata *m = JS_GetOpaque2(ctx, v, meshdata_class_id);
    if (!m) {
        type_error(ctx, "expected a MeshData");
        return NULL;
    }
    if (!m->alive) {
        type_error(ctx, "using a destroyed resource");
        return NULL;
    }
    return m;
}

JSValue efx_js_createMesh(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "createMesh requires a MeshData");
    }
    efxjs_meshdata *md = get_live_meshdata(ctx, argv[0]);
    if (!md) {
        return JS_EXCEPTION;
    }
    uint64_t handle = efx_render_mesh_create(md->md);
    if (!handle) {
        return generic_error(ctx, "mesh upload failed (no GPU context?)");
    }
    efxjs_mesh *m = calloc(1, sizeof(efxjs_mesh));
    if (!m) {
        efx_render_mesh_destroy(handle);
        return generic_error(ctx, "out of memory");
    }
    m->handle = handle;
    m->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, mesh_class_id);
    JS_SetOpaque(obj, m);
    return obj;
}

static efxjs_mesh *get_live_mesh(JSContext *ctx, JSValueConst v) {
    efxjs_mesh *m = JS_GetOpaque2(ctx, v, mesh_class_id);
    if (!m) {
        type_error(ctx, "expected a Mesh");
        return NULL;
    }
    if (!m->alive) {
        type_error(ctx, "using a destroyed resource");
        return NULL;
    }
    return m;
}

JSValue efx_js_drawMesh(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "drawMesh requires a mesh");
    }
    efxjs_mesh *mesh = get_live_mesh(ctx, argv[0]);
    if (!mesh) {
        return JS_EXCEPTION;
    }

    float transform[16];
    int has_transform = 0;
    float color[4] = {1, 1, 1, 1};
    int skinned = 0;

    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        JSValueConst opts = argv[1];
        if (!JS_IsObject(opts)) {
            return type_error(ctx, "drawMesh options must be an object");
        }
        static const char *known[] = {"transform", "color", "skinned"};
        if (check_known_fields(ctx, opts, known, 3, "drawMesh") != 0) {
            return JS_EXCEPTION;
        }
        JSValue tv = JS_GetPropertyStr(ctx, opts, "transform");
        if (JS_IsUndefined(tv)) {
            JS_FreeValue(ctx, tv);
        } else {
            float *buf = NULL;
            int len = 0;
            int rc = read_number_array(ctx, tv, &buf, &len, "transform");
            JS_FreeValue(ctx, tv);
            if (rc != 0) {
                return JS_EXCEPTION;
            }
            if (len != 16) {
                free(buf);
                return range_error(ctx, "transform must hold 16 numbers");
            }
            memcpy(transform, buf, sizeof(transform));
            free(buf);
            has_transform = 1;
        }

        JSValue cv = JS_GetPropertyStr(ctx, opts, "color");
        if (JS_IsUndefined(cv)) {
            JS_FreeValue(ctx, cv);
        } else {
            float *buf = NULL;
            int len = 0;
            int rc = read_number_array(ctx, cv, &buf, &len, "color");
            JS_FreeValue(ctx, cv);
            if (rc != 0) {
                return JS_EXCEPTION;
            }
            if (len != 4) {
                free(buf);
                return range_error(ctx, "color must hold 4 numbers");
            }
            memcpy(color, buf, sizeof(color));
            free(buf);
        }

        JSValue sv = JS_GetPropertyStr(ctx, opts, "skinned");
        if (!JS_IsUndefined(sv)) {
            if (!JS_IsBool(sv)) {
                JS_FreeValue(ctx, sv);
                return type_error(ctx, "skinned must be a boolean");
            }
            skinned = JS_ToBool(ctx, sv) ? 1 : 0;
        }
        JS_FreeValue(ctx, sv);
    }

    int rc = efx_render_mesh(mesh->handle, has_transform ? transform : NULL,
                             color, skinned);
    if (rc == EFX_RENDER_ERR_BUDGET) {
        return range_error(ctx, "display list budget exceeded");
    }
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return type_error(ctx, "expected a live Mesh");
    }
    if (rc == EFX_RENDER_ERR_RIG) {
        return type_error(ctx, "mesh has no rig to draw skinned");
    }
    if (rc == EFX_RENDER_ERR_FEEDBACK) {
        return type_error(ctx,
                          "cannot sample the render target being drawn into");
    }
    if (rc != EFX_RENDER_OK) {
        return generic_error(ctx, "drawMesh failed");
    }
    return JS_UNDEFINED;
}

/* --------------------------------------------------- F11 particle/billboard bindings */

JSValue efx_js_createParticleSystem(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return type_error(ctx, "createParticleSystem requires an options object");
    }
    /* required fields throw TypeError when absent (web-binding parity);
       range problems are raised by the engine validation below */
    JSValue rq = JS_GetPropertyStr(ctx, argv[0], "max");
    int has_max = !JS_IsUndefined(rq);
    JS_FreeValue(ctx, rq);
    if (!has_max) {
        return type_error(ctx, "createParticleSystem requires max");
    }
    rq = JS_GetPropertyStr(ctx, argv[0], "lifetime");
    int has_life = !JS_IsUndefined(rq);
    JS_FreeValue(ctx, rq);
    if (!has_life) {
        return type_error(ctx, "createParticleSystem requires lifetime");
    }
    efx_particle_config c;
    memset(&c, 0, sizeof(c));
    c.space = EFX_SPACE_WORLD;
    c.facing = EFX_FACING_VIEW;
    c.blend = EFX_BLEND_ALPHA;
    c.normal[1] = 1.0f;
    c.life_min = c.life_max = 1.0f;
    c.emitter_lifetime = -1.0f;
    c.direction[1] = 1.0f;
    c.size_count = 1;
    c.sizes[0] = 1.0f;
    c.color_count = 1;
    c.colors[0][0] = c.colors[0][1] = c.colors[0][2] = c.colors[0][3] = 1.0f;
    c.shape = EFX_SHAPE_POINT;
    c.insert_mode = EFX_INSERT_TOP;
    c.speed_scale = 1.0f;
    if (read_particle_config(ctx, argv[0], &c) != 0) {
        return JS_EXCEPTION;
    }
    if (!c.texture) {
        return type_error(ctx, "createParticleSystem requires a texture");
    }
    if (c.max <= 0) {
        return range_error(ctx, "createParticleSystem requires a positive max");
    }
    int err = 0;
    uint64_t h = efx_render_particles_create(&c, &err);
    if (!h) {
        if (err == EFX_RENDER_ERR_SIZE) {
            return range_error(ctx, "invalid particle configuration");
        }
        return generic_error(ctx, "createParticleSystem failed");
    }
    efxjs_particlesystem *p = calloc(1, sizeof(*p));
    if (!p) {
        efx_render_particles_destroy(h);
        return generic_error(ctx, "out of memory");
    }
    p->handle = h;
    p->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, particlesystem_class_id);
    JS_SetOpaque(obj, p);
    return obj;
}

JSValue efx_js_drawParticles(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "drawParticles requires a ParticleSystem");
    }
    efxjs_particlesystem *p = get_live_ps(ctx, argv[0]);
    if (!p) {
        return JS_EXCEPTION;
    }
    int rc = efx_render_particles_draw(p->handle);
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return type_error(ctx, "expected a live ParticleSystem");
    }
    if (rc == EFX_RENDER_ERR_BUDGET) {
        return range_error(ctx, "display list budget exceeded");
    }
    if (rc == EFX_RENDER_ERR_FEEDBACK) {
        return type_error(ctx, "cannot sample the render target being drawn into");
    }
    if (rc != EFX_RENDER_OK) {
        return generic_error(ctx, "drawParticles failed");
    }
    return JS_UNDEFINED;
}

JSValue efx_js_drawBillboard(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return type_error(ctx, "drawBillboard requires (pos, opts)");
    }
    float pos[3];
    if (!JS_IsArray(argv[0])) {
        return type_error(ctx, "drawBillboard pos must be [x,y,z]");
    }
    {
        JSValue lv = JS_GetPropertyStr(ctx, argv[0], "length");
        int32_t ln = -1;
        JS_ToInt32(ctx, &ln, lv);
        JS_FreeValue(ctx, lv);
        if (ln != 3) {
            return type_error(ctx, "drawBillboard pos must be [x,y,z]");
        }
    }
    if (get_float_array(ctx, argv[0], pos, 3) != 0) {
        return JS_EXCEPTION;
    }
    if (!JS_IsObject(argv[1])) {
        return type_error(ctx, "drawBillboard options must be an object");
    }
    JSValueConst opts = argv[1];
    static const char *known[] = {"texture", "size",     "color",     "sourceRect",
                                  "rotation", "facing",  "depthTest", "normal"};
    if (check_known_fields(ctx, opts, known, 8, "drawBillboard") != 0) {
        return JS_EXCEPTION;
    }
    JSValue tv = JS_GetPropertyStr(ctx, opts, "texture");
    if (JS_IsUndefined(tv)) {
        JS_FreeValue(ctx, tv);
        return type_error(ctx, "drawBillboard requires a texture");
    }
    uint64_t tex = 0;
    if (get_live_sample(ctx, tv, &tex) != 0) {
        JS_FreeValue(ctx, tv);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, tv);

    float w = 1.0f, h = 1.0f;
    float color[4] = {1, 1, 1, 1};
    float rotation = 0.0f;
    float normal[3] = {0, 1, 0};
    int facing = EFX_FACING_VIEW;
    int depth_test = 1;
    float src[4] = {0, 0, 0, 0};
    int has_src = 0;

    JSValue zv = JS_GetPropertyStr(ctx, opts, "size");
    if (!JS_IsUndefined(zv)) {
        if (JS_IsArray(zv)) {
            float sz[2];
            if (get_float_array(ctx, zv, sz, 2) != 0) {
                JS_FreeValue(ctx, zv);
                return JS_EXCEPTION;
            }
            w = sz[0];
            h = sz[1];
        } else {
            double d;
            if (JS_ToFloat64(ctx, &d, zv) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, zv);
                return type_error(ctx, "size must be a number or [w,h]");
            }
            w = h = (float)d;
        }
    }
    JS_FreeValue(ctx, zv);
    if (!(w > 0) || !(h > 0)) {
        return range_error(ctx, "size entries must be > 0");
    }

    JSValue cv = JS_GetPropertyStr(ctx, opts, "color");
    if (!JS_IsUndefined(cv)) {
        if (get_float_array(ctx, cv, color, 4) != 0) {
            JS_FreeValue(ctx, cv);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, cv);

    JSValue rv = JS_GetPropertyStr(ctx, opts, "rotation");
    if (!JS_IsUndefined(rv)) {
        double d;
        if (JS_ToFloat64(ctx, &d, rv) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, rv);
            return type_error(ctx, "rotation must be a finite number");
        }
        rotation = (float)d;
    }
    JS_FreeValue(ctx, rv);

    JSValue fv = JS_GetPropertyStr(ctx, opts, "facing");
    if (!JS_IsUndefined(fv)) {
        const char *s = JS_ToCString(ctx, fv);
        if (s && !strcmp(s, "view")) facing = EFX_FACING_VIEW;
        else if (s && !strcmp(s, "y")) facing = EFX_FACING_Y;
        else if (s && !strcmp(s, "plane")) facing = EFX_FACING_PLANE;
        else {
            if (s) JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, fv);
            return type_error(ctx, "facing must be 'view', 'y', or 'plane'");
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, fv);

    JSValue nv = JS_GetPropertyStr(ctx, opts, "normal");
    if (!JS_IsUndefined(nv)) {
        if (get_float_array(ctx, nv, normal, 3) != 0) {
            JS_FreeValue(ctx, nv);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, nv);

    JSValue dv = JS_GetPropertyStr(ctx, opts, "depthTest");
    if (!JS_IsUndefined(dv)) {
        if (!JS_IsBool(dv)) {
            JS_FreeValue(ctx, dv);
            return type_error(ctx, "depthTest must be a boolean");
        }
        depth_test = JS_ToBool(ctx, dv);
    }
    JS_FreeValue(ctx, dv);

    JSValue sv = JS_GetPropertyStr(ctx, opts, "sourceRect");
    if (!JS_IsUndefined(sv)) {
        if (!JS_IsObject(sv)) {
            JS_FreeValue(ctx, sv);
            return type_error(ctx, "sourceRect must be an object");
        }
        static const char *skeys[] = {"x", "y", "w", "h"};
        for (int i = 0; i < 4; i++) {
            JSValue f = JS_GetPropertyStr(ctx, sv, skeys[i]);
            double d;
            if (JS_ToFloat64(ctx, &d, f) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, f);
                JS_FreeValue(ctx, sv);
                return type_error(ctx, "sourceRect fields must be finite numbers");
            }
            JS_FreeValue(ctx, f);
            src[i] = (float)d;
        }
        JS_FreeValue(ctx, sv);
        if (src[2] <= 0 || src[3] <= 0) {
            return range_error(ctx, "sourceRect extent must be > 0");
        }
        int tw = 0, th = 0;
        efx_render_sample_size(tex, &tw, &th);
        if (src[0] < 0 || src[1] < 0 || src[0] + src[2] > (float)tw ||
            src[1] + src[3] > (float)th) {
            return range_error(ctx, "sourceRect outside texture bounds");
        }
        has_src = 1;
    }

    int rc = efx_render_billboard(tex, pos, w, h, color, rotation, facing, normal,
                                  depth_test, src, has_src);
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return type_error(ctx, "expected a live Texture or RenderTarget");
    }
    if (rc == EFX_RENDER_ERR_SIZE) {
        return range_error(ctx, "invalid billboard size or facing");
    }
    if (rc == EFX_RENDER_ERR_BUDGET) {
        return range_error(ctx, "display list budget exceeded");
    }
    if (rc != EFX_RENDER_OK) {
        return generic_error(ctx, "drawBillboard failed");
    }
    return JS_UNDEFINED;
}

/* one parsed sprite for the atomic drawSprites loop */
typedef struct {
    float x, y, w, h;
    float color[4];
    float rotation, scale;
    float src[4];
    int has_src;
    float origin[2];
    int has_origin;
} sprite_params;

/* validate + parse one sprite entry exactly as drawQuad parses its options */
static int parse_sprite(JSContext *ctx, uint64_t tex, JSValueConst e,
                        sprite_params *s) {
    if (!JS_IsObject(e)) {
        type_error(ctx, "each sprite must be an object");
        return -1;
    }
    static const char *known[] = {"x",     "y",      "size", "color",
                                  "rotation", "scale", "sourceRect",
                                  "origin"};
    if (check_known_fields(ctx, e, known, 8, "drawSprites") != 0) {
        return -1;
    }
    s->color[0] = s->color[1] = s->color[2] = s->color[3] = 1.0f;
    s->rotation = 0.0f;
    s->scale = 1.0f;
    s->has_src = 0;
    s->has_origin = 0;

    JSValue xv = JS_GetPropertyStr(ctx, e, "x");
    JSValue yv = JS_GetPropertyStr(ctx, e, "y");
    double x = 0, y = 0;
    int bad = JS_ToFloat64(ctx, &x, xv) < 0 || JS_ToFloat64(ctx, &y, yv) < 0 ||
              !isfinite(x) || !isfinite(y);
    JS_FreeValue(ctx, xv);
    JS_FreeValue(ctx, yv);
    if (bad) {
        type_error(ctx, "sprite x and y must be finite numbers");
        return -1;
    }
    s->x = (float)x;
    s->y = (float)y;

    JSValue cv = JS_GetPropertyStr(ctx, e, "color");
    if (!JS_IsUndefined(cv)) {
        if (get_float_array(ctx, cv, s->color, 4) != 0) {
            JS_FreeValue(ctx, cv);
            return -1;
        }
    }
    JS_FreeValue(ctx, cv);

    JSValue rv = JS_GetPropertyStr(ctx, e, "rotation");
    if (!JS_IsUndefined(rv)) {
        double d;
        if (JS_ToFloat64(ctx, &d, rv) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, rv);
            type_error(ctx, "rotation must be a finite number");
            return -1;
        }
        s->rotation = (float)d;
    }
    JS_FreeValue(ctx, rv);

    JSValue scv = JS_GetPropertyStr(ctx, e, "scale");
    if (!JS_IsUndefined(scv)) {
        double d;
        if (JS_ToFloat64(ctx, &d, scv) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, scv);
            type_error(ctx, "scale must be a finite number");
            return -1;
        }
        if (d <= 0) {
            JS_FreeValue(ctx, scv);
            range_error(ctx, "scale must be > 0");
            return -1;
        }
        s->scale = (float)d;
    }
    JS_FreeValue(ctx, scv);

    float size[2] = {0, 0};
    int has_size = 0;
    JSValue zv = JS_GetPropertyStr(ctx, e, "size");
    if (!JS_IsUndefined(zv)) {
        if (get_float_array(ctx, zv, size, 2) != 0) {
            JS_FreeValue(ctx, zv);
            return -1;
        }
        if (size[0] <= 0 || size[1] <= 0) {
            JS_FreeValue(ctx, zv);
            range_error(ctx, "size entries must be > 0");
            return -1;
        }
        has_size = 1;
    }
    JS_FreeValue(ctx, zv);

    JSValue ov = JS_GetPropertyStr(ctx, e, "origin");
    if (!JS_IsUndefined(ov)) {
        if (get_float_array(ctx, ov, s->origin, 2) != 0) {
            JS_FreeValue(ctx, ov);
            return -1;
        }
        s->has_origin = 1;
    }
    JS_FreeValue(ctx, ov);

    JSValue srcv = JS_GetPropertyStr(ctx, e, "sourceRect");
    if (!JS_IsUndefined(srcv)) {
        if (!JS_IsObject(srcv)) {
            JS_FreeValue(ctx, srcv);
            type_error(ctx, "sourceRect must be an object");
            return -1;
        }
        static const char *skeys[] = {"x", "y", "w", "h"};
        for (int i = 0; i < 4; i++) {
            JSValue f = JS_GetPropertyStr(ctx, srcv, skeys[i]);
            double d;
            if (JS_ToFloat64(ctx, &d, f) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, f);
                JS_FreeValue(ctx, srcv);
                type_error(ctx, "sourceRect fields must be finite numbers");
                return -1;
            }
            JS_FreeValue(ctx, f);
            s->src[i] = (float)d;
        }
        JS_FreeValue(ctx, srcv);
        if (s->src[2] <= 0 || s->src[3] <= 0) {
            range_error(ctx, "sourceRect extent must be > 0");
            return -1;
        }
        int tw = 0, th = 0;
        efx_render_sample_size(tex, &tw, &th);
        if (s->src[0] < 0 || s->src[1] < 0 ||
            s->src[0] + s->src[2] > (float)tw ||
            s->src[1] + s->src[3] > (float)th) {
            range_error(ctx, "sourceRect outside texture bounds");
            return -1;
        }
        s->has_src = 1;
    }

    if (has_size) {
        s->w = size[0];
        s->h = size[1];
    } else if (s->has_src) {
        s->w = s->src[2];
        s->h = s->src[3];
    } else {
        int tw = 0, th = 0;
        efx_render_sample_size(tex, &tw, &th);
        s->w = (float)tw;
        s->h = (float)th;
    }
    return 0;
}

JSValue efx_js_drawSprites(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return type_error(ctx, "drawSprites requires (texture, sprites)");
    }
    uint64_t tex = 0;
    if (get_live_sample(ctx, argv[0], &tex) != 0) {
        return JS_EXCEPTION;
    }
    if (!JS_IsArray(argv[1])) {
        return type_error(ctx, "sprites must be an array");
    }
    JSValue lv = JS_GetPropertyStr(ctx, argv[1], "length");
    int32_t n = 0;
    JS_ToInt32(ctx, &n, lv);
    JS_FreeValue(ctx, lv);
    if (n <= 0) {
        return JS_UNDEFINED;
    }
    sprite_params *items = calloc((size_t)n, sizeof(*items));
    if (!items) {
        return generic_error(ctx, "out of memory");
    }
    /* validate every entry before recording any (atomic) */
    for (int i = 0; i < n; i++) {
        JSValue e = JS_GetPropertyUint32(ctx, argv[1], (uint32_t)i);
        int rc = parse_sprite(ctx, tex, e, &items[i]);
        JS_FreeValue(ctx, e);
        if (rc != 0) {
            free(items);
            return JS_EXCEPTION;
        }
    }
    for (int i = 0; i < n; i++) {
        sprite_params *s = &items[i];
        float ox = s->has_origin ? s->origin[0] : s->w * 0.5f;
        float oy = s->has_origin ? s->origin[1] : s->h * 0.5f;
        int rc = efx_render_quad(s->x, s->y, s->w, s->h, tex, s->color,
                                 s->rotation, s->scale, s->src, s->has_src, ox,
                                 oy);
        if (rc == EFX_RENDER_ERR_BUDGET) {
            free(items);
            return range_error(ctx, "display list budget exceeded");
        }
        if (rc != EFX_RENDER_OK) {
            free(items);
            return generic_error(ctx, "drawSprites failed");
        }
    }
    free(items);
    return JS_UNDEFINED;
}

/* F7: parse one pose sample ({ clip, time, weight? }); clip names resolve
 * through the live Mesh's rig. Returns 0 ok (exception pending otherwise). */
static int read_pose_sample(JSContext *ctx, JSValueConst v, efxjs_mesh *mesh,
                            efx_pose_sample *out) {
    if (!JS_IsObject(v)) {
        type_error(ctx, "pose samples must be objects");
        return -1;
    }
    static const char *known[] = {"clip", "time", "weight"};
    if (check_known_fields(ctx, v, known, 3, "pose sample") != 0) {
        return -1;
    }
    JSValue cv = JS_GetPropertyStr(ctx, v, "clip");
    if (JS_IsUndefined(cv)) {
        JS_FreeValue(ctx, cv);
        type_error(ctx, "pose sample requires clip");
        return -1;
    }
    if (JS_IsString(cv)) {
        const char *name = JS_ToCString(ctx, cv);
        int idx = efx_render_mesh_find_clip(mesh->handle, name);
        JS_FreeCString(ctx, name);
        JS_FreeValue(ctx, cv);
        if (idx < 0) {
            generic_error(ctx, "unknown clip name");
            return -1;
        }
        out->clip = idx;
    } else if (JS_IsNumber(cv)) {
        double d = 0;
        JS_ToFloat64(ctx, &d, cv);
        JS_FreeValue(ctx, cv);
        if (!isfinite(d) || d != floor(d) || d < 0) {
            range_error(ctx, "clip index out of range");
            return -1;
        }
        out->clip = (int)d;
    } else {
        JS_FreeValue(ctx, cv);
        type_error(ctx, "clip must be a name or index");
        return -1;
    }

    JSValue tv = JS_GetPropertyStr(ctx, v, "time");
    if (!JS_IsNumber(tv)) {
        JS_FreeValue(ctx, tv);
        type_error(ctx, "pose sample requires a numeric time");
        return -1;
    }
    double t = 0;
    JS_ToFloat64(ctx, &t, tv);
    JS_FreeValue(ctx, tv);
    if (!isfinite(t)) {
        range_error(ctx, "time must be finite");
        return -1;
    }
    out->time = (float)t;

    out->weight = 1.0f;
    JSValue wv = JS_GetPropertyStr(ctx, v, "weight");
    if (!JS_IsUndefined(wv)) {
        if (!JS_IsNumber(wv)) {
            JS_FreeValue(ctx, wv);
            type_error(ctx, "weight must be a number");
            return -1;
        }
        double w = 0;
        JS_ToFloat64(ctx, &w, wv);
        JS_FreeValue(ctx, wv);
        if (!isfinite(w) || w < 0) {
            range_error(ctx, "weight must be finite and >= 0");
            return -1;
        }
        out->weight = (float)w;
    } else {
        JS_FreeValue(ctx, wv);
    }
    return 0;
}

JSValue efx_js_poseMesh(JSContext *ctx, JSValueConst this_val, int argc,
                        JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return type_error(ctx, "poseMesh requires (mesh, pose)");
    }
    efxjs_mesh *mesh = get_live_mesh(ctx, argv[0]);
    if (!mesh) {
        return JS_EXCEPTION;
    }
    if (!efx_render_mesh_skinned(mesh->handle)) {
        return type_error(ctx, "poseMesh requires a Mesh with a rig");
    }
    JSValueConst pose = argv[1];
    efx_pose_sample *samples = NULL;
    int count = 0;
    int rc = 0;
    if (JS_IsArray(pose)) {
        JSValue lenv = JS_GetPropertyStr(ctx, pose, "length");
        int32_t len = -1;
        JS_ToInt32(ctx, &len, lenv);
        JS_FreeValue(ctx, lenv);
        if (len < 0) {
            return range_error(ctx, "pose array length is invalid");
        }
        if (len > 0) {
            samples = malloc((size_t)len * sizeof(*samples));
            if (!samples) {
                return generic_error(ctx, "out of memory");
            }
        }
        for (int32_t i = 0; i < len; i++) {
            JSValue sv = JS_GetPropertyUint32(ctx, pose, (uint32_t)i);
            int sr = read_pose_sample(ctx, sv, mesh, &samples[i]);
            JS_FreeValue(ctx, sv);
            if (sr != 0) {
                free(samples);
                return JS_EXCEPTION;
            }
        }
        count = (int)len;
    } else if (JS_IsObject(pose)) {
        samples = malloc(sizeof(*samples));
        if (!samples) {
            return generic_error(ctx, "out of memory");
        }
        if (read_pose_sample(ctx, pose, mesh, &samples[0]) != 0) {
            free(samples);
            return JS_EXCEPTION;
        }
        count = 1;
    } else {
        return type_error(ctx, "pose must be a sample or an array of samples");
    }

    rc = efx_render_mesh_pose(mesh->handle, samples, count);
    free(samples);
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return type_error(ctx, "poseMesh requires a Mesh with a rig");
    }
    if (rc == EFX_RENDER_ERR_INDEX) {
        return range_error(ctx, "clip index out of range");
    }
    if (rc != EFX_RENDER_OK) {
        return generic_error(ctx, "poseMesh failed");
    }
    return JS_UNDEFINED;
}

JSValue efx_js_setCamera3D(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return type_error(ctx, "setCamera3D requires an options object");
    }
    JSValueConst opts = argv[0];
    static const char *known[] = {"pos", "target", "fov", "near", "far"};
    if (check_known_fields(ctx, opts, known, 5, "setCamera3D") != 0) {
        return JS_EXCEPTION;
    }
    efx_camera3d cam;
    memset(&cam, 0, sizeof(cam));
    cam.near_z = 0.1f;
    cam.far_z = 100.0f;

    static const char *vec_keys[] = {"pos", "target"};
    float *vec_outs[] = {cam.pos, cam.target};
    for (int i = 0; i < 2; i++) {
        JSValue v = JS_GetPropertyStr(ctx, opts, vec_keys[i]);
        if (JS_IsUndefined(v)) {
            JS_FreeValue(ctx, v);
            return type_error(ctx, "setCamera3D requires pos and target");
        }
        float *buf = NULL;
        int len = 0;
        int rc = read_number_array(ctx, v, &buf, &len, vec_keys[i]);
        JS_FreeValue(ctx, v);
        if (rc != 0) {
            return JS_EXCEPTION;
        }
        if (len != 3) {
            free(buf);
            return range_error(ctx, "pos and target must hold 3 numbers");
        }
        memcpy(vec_outs[i], buf, sizeof(float) * 3);
        free(buf);
    }
    JSValue fv = JS_GetPropertyStr(ctx, opts, "fov");
    if (JS_IsUndefined(fv)) {
        JS_FreeValue(ctx, fv);
        return type_error(ctx, "setCamera3D requires fov");
    }
    double d = 0;
    if (!JS_IsNumber(fv) || JS_ToFloat64(ctx, &d, fv) < 0) {
        JS_FreeValue(ctx, fv);
        return type_error(ctx, "fov must be a number");
    }
    JS_FreeValue(ctx, fv);
    if (!isfinite(d)) {
        return range_error(ctx, "fov must be finite");
    }
    cam.fov = (float)d;

    static const char *opt_keys[] = {"near", "far"};
    float *opt_outs[] = {&cam.near_z, &cam.far_z};
    for (int i = 0; i < 2; i++) {
        JSValue v = JS_GetPropertyStr(ctx, opts, opt_keys[i]);
        if (!JS_IsUndefined(v)) {
            if (!JS_IsNumber(v) || JS_ToFloat64(ctx, &d, v) < 0) {
                JS_FreeValue(ctx, v);
                return type_error(ctx, "near and far must be numbers");
            }
            if (!isfinite(d)) {
                JS_FreeValue(ctx, v);
                return range_error(ctx, "near and far must be finite");
            }
            *opt_outs[i] = (float)d;
        }
        JS_FreeValue(ctx, v);
    }
    efx_render_set_camera3d(&cam);
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------ F4a bindings */

JSValue efx_js_setLight(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return type_error(ctx, "setLight requires (slot, opts)");
    }
    double slot_d = 0;
    if (!JS_IsNumber(argv[0]) || JS_ToFloat64(ctx, &slot_d, argv[0]) < 0 ||
        !isfinite(slot_d) || slot_d != floor(slot_d) || slot_d < 0 ||
        slot_d > (double)(EFX_MAX_POINT_LIGHTS - 1)) {
        return range_error(ctx, "light slot must be an integer 0..3");
    }
    int slot = (int)slot_d;
    if (JS_IsNull(argv[1]) || JS_IsUndefined(argv[1])) {
        efx_render_set_point_light(slot, NULL);
        return JS_UNDEFINED;
    }
    if (!JS_IsObject(argv[1])) {
        return type_error(ctx, "setLight options must be an object or null");
    }
    static const char *known[] = {"pos", "color", "range"};
    if (check_known_fields(ctx, argv[1], known, 3, "setLight") != 0) {
        return JS_EXCEPTION;
    }
    efx_point_light l;
    memset(&l, 0, sizeof(l));
    JSValue pv = JS_GetPropertyStr(ctx, argv[1], "pos");
    if (JS_IsUndefined(pv)) {
        JS_FreeValue(ctx, pv);
        return type_error(ctx, "setLight requires pos");
    }
    if (read_vec3(ctx, pv, l.pos, "pos") != 0) {
        JS_FreeValue(ctx, pv);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, pv);
    JSValue cv = JS_GetPropertyStr(ctx, argv[1], "color");
    if (JS_IsUndefined(cv)) {
        JS_FreeValue(ctx, cv);
        return type_error(ctx, "setLight requires color");
    }
    if (get_float_array(ctx, cv, l.color, 4) != 0) {
        JS_FreeValue(ctx, cv);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, cv);
    JSValue rv = JS_GetPropertyStr(ctx, argv[1], "range");
    if (JS_IsUndefined(rv)) {
        JS_FreeValue(ctx, rv);
        l.range = 0.0f;
    } else {
        double d = 0;
        int bad = !JS_IsNumber(rv) || JS_ToFloat64(ctx, &d, rv) < 0;
        JS_FreeValue(ctx, rv);
        if (bad) {
            return type_error(ctx, "range must be a number");
        }
        if (!isfinite(d) || d < 0) {
            return range_error(ctx, "range must be a finite number >= 0");
        }
        l.range = (float)d;
    }
    l.enabled = 1;
    efx_render_set_point_light((int)slot, &l);
    return JS_UNDEFINED;
}

JSValue efx_js_setDirectionalLight(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "setDirectionalLight requires an options object or null");
    }
    if (JS_IsNull(argv[0]) || JS_IsUndefined(argv[0])) {
        efx_render_set_directional_light(NULL);
        return JS_UNDEFINED;
    }
    if (!JS_IsObject(argv[0])) {
        return type_error(ctx, "setDirectionalLight options must be an object or null");
    }
    static const char *known[] = {"dir", "color"};
    if (check_known_fields(ctx, argv[0], known, 2, "setDirectionalLight") != 0) {
        return JS_EXCEPTION;
    }
    efx_dir_light l;
    memset(&l, 0, sizeof(l));
    JSValue dv = JS_GetPropertyStr(ctx, argv[0], "dir");
    if (JS_IsUndefined(dv)) {
        JS_FreeValue(ctx, dv);
        return type_error(ctx, "setDirectionalLight requires dir");
    }
    if (read_vec3(ctx, dv, l.dir, "dir") != 0) {
        JS_FreeValue(ctx, dv);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, dv);
    if (l.dir[0] == 0.0f && l.dir[1] == 0.0f && l.dir[2] == 0.0f) {
        return type_error(ctx, "dir must be non-zero");
    }
    JSValue cv = JS_GetPropertyStr(ctx, argv[0], "color");
    if (JS_IsUndefined(cv)) {
        JS_FreeValue(ctx, cv);
        return type_error(ctx, "setDirectionalLight requires color");
    }
    if (get_float_array(ctx, cv, l.color, 4) != 0) {
        JS_FreeValue(ctx, cv);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, cv);
    l.enabled = 1;
    efx_render_set_directional_light(&l);
    return JS_UNDEFINED;
}

JSValue efx_js_setMeshSurfaceMaterial(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) {
        return type_error(ctx,
                          "setMeshSurfaceMaterial requires (mesh, surfaceIndex, mat)");
    }
    efxjs_mesh *mesh = get_live_mesh(ctx, argv[0]);
    if (!mesh) {
        return JS_EXCEPTION;
    }
    double index_d = 0;
    if (!JS_IsNumber(argv[1]) || JS_ToFloat64(ctx, &index_d, argv[1]) < 0 ||
        !isfinite(index_d) || index_d != floor(index_d)) {
        return range_error(ctx, "surfaceIndex must be an integer");
    }
    int index = (int)index_d;
    int count = efx_render_mesh_surface_count(mesh->handle);
    if (index < 0 || index >= count) {
        return range_error(ctx, "surfaceIndex out of range");
    }
    efx_material mat;
    int has = 0;
    if (JS_IsNull(argv[2]) || JS_IsUndefined(argv[2])) {
        has = 0;
    } else {
        if (read_material(ctx, argv[2], &mat) != 0) {
            return JS_EXCEPTION;
        }
        has = 1;
    }
    int rc = efx_render_mesh_set_material(mesh->handle, (int)index,
                                          has ? &mat : NULL, has);    if (rc == EFX_RENDER_ERR_HANDLE) {
        return type_error(ctx, "expected a live Mesh");
    }
    if (rc == EFX_RENDER_ERR_INDEX) {
        return range_error(ctx, "surfaceIndex out of range");
    }
    if (rc != EFX_RENDER_OK) {
        return generic_error(ctx, "setMeshSurfaceMaterial failed");
    }
    return JS_UNDEFINED;
}

/* -------------------------------------------------------- F5b bindings */

/* optional finite-number field: absent -> *present = 0; non-number ->
 * TypeError; non-finite -> RangeError (the F2 array precedent) */
static int post_number(JSContext *ctx, JSValueConst obj, const char *key,
                       float *out, int *present) {
    *present = 0;
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (!JS_IsNumber(v)) {
        JS_FreeValue(ctx, v);
        JS_ThrowTypeError(ctx, "%s must be a number", key);
        return -1;
    }
    double d = 0;
    if (JS_ToFloat64(ctx, &d, v) < 0) {
        JS_FreeValue(ctx, v);
        return -1;
    }
    JS_FreeValue(ctx, v);
    if (!isfinite(d)) {
        JS_ThrowRangeError(ctx, "%s must be a finite number", key);
        return -1;
    }
    *out = (float)d;
    *present = 1;
    return 0;
}

/* parse one chain entry into the engine snapshot (F5b spec: registered
 * effect name, effect-specific options, mix; unknown field -> TypeError) */
static int read_post_entry(JSContext *ctx, JSValueConst v, efx_post_entry *out) {
    memset(out, 0, sizeof(*out));
    out->mix = 1.0f;
    if (!JS_IsObject(v)) {
        type_error(ctx, "post-effect entry must be an object");
        return -1;
    }
    JSValue ev = JS_GetPropertyStr(ctx, v, "effect");
    if (!JS_IsString(ev)) {
        JS_FreeValue(ctx, ev);
        type_error(ctx, "post-effect entry requires an effect name");
        return -1;
    }
    const char *name = JS_ToCString(ctx, ev);
    JS_FreeValue(ctx, ev);
    if (!name) {
        return -1;
    }
    int known = 0;
    if (strcmp(name, "colorFilter") == 0) {
        out->effect = EFX_POST_COLOR_FILTER;
        out->u.color_filter.brightness = 1.0f;
        out->u.color_filter.contrast = 1.0f;
        out->u.color_filter.saturation = 1.0f;
        out->u.color_filter.tint[0] = 1.0f;
        out->u.color_filter.tint[1] = 1.0f;
        out->u.color_filter.tint[2] = 1.0f;
        out->u.color_filter.tint[3] = 1.0f;
        known = 1;
    } else if (strcmp(name, "blur") == 0) {
        out->effect = EFX_POST_BLUR;
        out->u.blur.radius = 1.0f;
        known = 1;
    } else if (strcmp(name, "bloom") == 0) {
        out->effect = EFX_POST_BLOOM;
        out->u.bloom.threshold = 0.8f;
        out->u.bloom.strength = 0.5f;
        known = 1;
    }
    JS_FreeCString(ctx, name);
    if (!known) {
        type_error(ctx, "unknown post effect");
        return -1;
    }

    static const char *color_keys[] = {"effect", "mix", "brightness",
                                       "contrast", "saturation", "tint"};
    static const char *blur_keys[] = {"effect", "mix", "radius"};
    static const char *bloom_keys[] = {"effect", "mix", "threshold",
                                       "strength"};
    const char **keys;
    int nkeys;
    if (out->effect == EFX_POST_COLOR_FILTER) {
        keys = color_keys;
        nkeys = 6;
    } else if (out->effect == EFX_POST_BLUR) {
        keys = blur_keys;
        nkeys = 3;
    } else {
        keys = bloom_keys;
        nkeys = 4;
    }
    if (check_known_fields(ctx, v, keys, nkeys, "post effect") != 0) {
        return -1;
    }

    float f = 0;
    int present = 0;
    if (post_number(ctx, v, "mix", &f, &present) != 0) {
        return -1;
    }
    if (present) {
        out->mix = f;
    }
    if (out->effect == EFX_POST_COLOR_FILTER) {
        if (post_number(ctx, v, "brightness", &f, &present) != 0) return -1;
        if (present) out->u.color_filter.brightness = f;
        if (post_number(ctx, v, "contrast", &f, &present) != 0) return -1;
        if (present) out->u.color_filter.contrast = f;
        if (post_number(ctx, v, "saturation", &f, &present) != 0) return -1;
        if (present) out->u.color_filter.saturation = f;
        JSValue tv = JS_GetPropertyStr(ctx, v, "tint");
        if (!JS_IsUndefined(tv)) {
            int rc = get_float_array(ctx, tv, out->u.color_filter.tint, 4);
            JS_FreeValue(ctx, tv);
            if (rc != 0) {
                return -1;
            }
        } else {
            JS_FreeValue(ctx, tv);
        }
    } else if (out->effect == EFX_POST_BLUR) {
        if (post_number(ctx, v, "radius", &f, &present) != 0) return -1;
        if (present) out->u.blur.radius = f;
    } else {
        if (post_number(ctx, v, "threshold", &f, &present) != 0) return -1;
        if (present) out->u.bloom.threshold = f;
        if (post_number(ctx, v, "strength", &f, &present) != 0) return -1;
        if (present) out->u.bloom.strength = f;
    }
    return 0;
}

JSValue efx_js_setPostEffects(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "setPostEffects requires an array or null");
    }
    if (JS_IsNull(argv[0]) || JS_IsUndefined(argv[0])) {
        efx_render_set_post_effects(NULL, 0);
        return JS_UNDEFINED;
    }
    if (!JS_IsArray(argv[0])) {
        return type_error(ctx, "setPostEffects requires an array or null");
    }
    JSValue lenv = JS_GetPropertyStr(ctx, argv[0], "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    if (len < 0) {
        return type_error(ctx, "setPostEffects requires an array or null");
    }
    if (len > EFX_POST_MAX_ENTRIES) {
        return range_error(ctx, "post-effect chain is limited to 8 entries");
    }
    efx_post_entry entries[EFX_POST_MAX_ENTRIES];
    for (int i = 0; i < len; i++) {
        JSValue ev = JS_GetPropertyUint32(ctx, argv[0], (uint32_t)i);
        int rc = read_post_entry(ctx, ev, &entries[i]);
        JS_FreeValue(ctx, ev);
        if (rc != 0) {
            return JS_EXCEPTION;
        }
    }
    int rc = efx_render_set_post_effects(entries, (int)len);
    if (rc == EFX_POST_ERR_UNKNOWN) {
        return type_error(ctx, "unknown post effect");
    }
    if (rc == EFX_POST_ERR_COUNT) {
        return range_error(ctx, "post-effect chain is limited to 8 entries");
    }
    if (rc == EFX_POST_ERR_RANGE) {
        return range_error(ctx, "post-effect option out of range");
    }
    if (rc != EFX_POST_OK) {
        return generic_error(ctx, "setPostEffects failed");
    }
    return JS_UNDEFINED;
}

JSValue efx_js_setRenderScale(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "setRenderScale requires a scale number");
    }
    double scale = 0;
    if (!JS_IsNumber(argv[0]) || JS_ToFloat64(ctx, &scale, argv[0]) < 0) {
        return type_error(ctx, "scale must be a number");
    }
    if (!isfinite(scale) || scale <= 0.0 || scale > 2.0) {
        return range_error(ctx, "scale must be in (0, 2]");
    }
    int filter = EFX_FILTER_LINEAR;
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        if (!JS_IsObject(argv[1])) {
            return type_error(ctx, "setRenderScale options must be an object");
        }
        static const char *known[] = {"filter"};
        if (check_known_fields(ctx, argv[1], known, 1, "setRenderScale") != 0) {
            return JS_EXCEPTION;
        }
        JSValue fv = JS_GetPropertyStr(ctx, argv[1], "filter");
        if (!JS_IsUndefined(fv)) {
            const char *fs = JS_ToCString(ctx, fv);
            if (!fs) {
                JS_FreeValue(ctx, fv);
                return type_error(ctx, "filter must be a string");
            }
            if (strcmp(fs, "nearest") == 0) {
                filter = EFX_FILTER_NEAREST;
            } else if (strcmp(fs, "linear") == 0) {
                filter = EFX_FILTER_LINEAR;
            } else {
                JS_FreeCString(ctx, fs);
                JS_FreeValue(ctx, fv);
                return type_error(ctx, "unknown filter");
            }
            JS_FreeCString(ctx, fs);
        }
        JS_FreeValue(ctx, fv);
    }
    int rc = efx_render_set_render_scale((float)scale, filter);
    if (rc == EFX_POST_ERR_RANGE) {
        return range_error(ctx, "scale must be in (0, 2]");
    }
    if (rc == EFX_POST_ERR_FILTER) {
        return type_error(ctx, "unknown filter");
    }
    if (rc != EFX_POST_OK) {
        return generic_error(ctx, "setRenderScale failed");
    }
    return JS_UNDEFINED;
}

/* -------------------------------------------------------- F9 input bindings */

/* keyboard.isDown/isPressed/isReleased — magic 0/1/2 */
static JSValue efx_js_key_query(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int magic) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) {
        return type_error(ctx, "keyboard query requires a key name");
    }
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) {
        return JS_EXCEPTION;
    }
    int key = efx_input_key_id(name);
    JS_FreeCString(ctx, name);
    if (key < 0) {
        return type_error(ctx, "unknown key");
    }
    int v;
    if (magic == 1) {
        v = efx_input_key_is_pressed(key);
    } else if (magic == 2) {
        v = efx_input_key_is_released(key);
    } else {
        v = efx_input_key_is_down(key);
    }
    return JS_NewBool(ctx, v);
}

/* keyboard.onDown/onUp/onChar — magic 0/1/2; returns an unsubscribe fn */
static JSValue efx_js_key_on(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "input callback registration requires a function");
    }
    int which = magic == 1 ? EFX_HOOK_LIST_KB_UP
              : magic == 2 ? EFX_HOOK_LIST_KB_CHAR
                           : EFX_HOOK_LIST_KB_DOWN;
    return register_hook(ctx, argv[0], which);
}

/* mouse.isDown/isPressed/isReleased — magic 0/1/2 */
static JSValue efx_js_mouse_query(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) {
        return type_error(ctx, "mouse query requires a button name");
    }
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) {
        return JS_EXCEPTION;
    }
    int button = efx_input_button_id(name);
    JS_FreeCString(ctx, name);
    if (button < 0) {
        return type_error(ctx, "unknown mouse button");
    }
    int v;
    if (magic == 1) {
        v = efx_input_button_is_pressed(button);
    } else if (magic == 2) {
        v = efx_input_button_is_released(button);
    } else {
        v = efx_input_button_is_down(button);
    }
    return JS_NewBool(ctx, v);
}

/* mouse.onDown/onUp/onMove/onWheel — magic 0/1/2/3 */
static JSValue efx_js_mouse_on(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "input callback registration requires a function");
    }
    int which;
    switch (magic) {
    case 1:
        which = EFX_HOOK_LIST_MOUSE_UP;
        break;
    case 2:
        which = EFX_HOOK_LIST_MOUSE_MOVE;
        break;
    case 3:
        which = EFX_HOOK_LIST_MOUSE_WHEEL;
        break;
    default:
        which = EFX_HOOK_LIST_MOUSE_DOWN;
        break;
    }
    return register_hook(ctx, argv[0], which);
}

static JSValue num_pair(JSContext *ctx, float a, float b) {
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat64(ctx, a));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat64(ctx, b));
    return arr;
}

static JSValue efx_js_mouse_getPosition(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    float x = 0, y = 0;
    efx_input_pointer(&x, &y);
    return num_pair(ctx, x, y);
}

static JSValue efx_js_mouse_getX(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    float x = 0, y = 0;
    efx_input_pointer(&x, &y);
    return JS_NewFloat64(ctx, x);
}

static JSValue efx_js_mouse_getY(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    float x = 0, y = 0;
    efx_input_pointer(&x, &y);
    return JS_NewFloat64(ctx, y);
}

static JSValue efx_js_mouse_getDelta(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    float dx = 0, dy = 0;
    efx_input_delta(&dx, &dy);
    return num_pair(ctx, dx, dy);
}

static JSValue efx_js_mouse_getWheel(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    float dx = 0, dy = 0;
    efx_input_wheel_delta(&dx, &dy);
    return num_pair(ctx, dx, dy);
}

static JSValue efx_js_window_getSize(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewInt32(ctx, w));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewInt32(ctx, h));
    return arr;
}

static JSValue efx_js_window_getWidth(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return JS_NewInt32(ctx, w);
}

static JSValue efx_js_window_getHeight(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return JS_NewInt32(ctx, h);
}

static JSValue efx_js_window_getDpiScale(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return JS_NewFloat64(ctx, dpi);
}

/* F13 gamepad: count is a read-only property; get(index) returns the pad
 * view or null; onConnect/onDisconnect return unsubscribe functions. */
static JSValue efx_js_gamepad_count(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    return JS_NewInt32(ctx, efx_input_gamepad_count());
}

static JSValue efx_js_gamepad_get(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsNumber(argv[0])) {
        return type_error(ctx, "gamepad.get requires an index");
    }
    int index = 0;
    JS_ToInt32(ctx, &index, argv[0]);
    if (index < 0 || index >= EFX_GAMEPAD_MAX ||
        !efx_input_gamepad_connected(index)) {
        return JS_NULL;
    }
    return efx_runtime_gamepad_view((efx_runtime *)host_state(ctx), index);
}

static JSValue efx_js_gamepad_on(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx,
                          "input callback registration requires a function");
    }
    int which = magic == 1 ? EFX_HOOK_LIST_GP_DISCONNECT
                           : EFX_HOOK_LIST_GP_CONNECT;
    return register_hook(ctx, argv[0], which);
}

int efx_api_register_input(JSContext *ctx, JSValueConst efx) {
    static const JSCFunctionListEntry kb_funcs[] = {
        JS_CFUNC_MAGIC_DEF("isDown", 1, efx_js_key_query, 0),
        JS_CFUNC_MAGIC_DEF("isPressed", 1, efx_js_key_query, 1),
        JS_CFUNC_MAGIC_DEF("isReleased", 1, efx_js_key_query, 2),
        JS_CFUNC_MAGIC_DEF("onDown", 1, efx_js_key_on, 0),
        JS_CFUNC_MAGIC_DEF("onUp", 1, efx_js_key_on, 1),
        JS_CFUNC_MAGIC_DEF("onChar", 1, efx_js_key_on, 2),
    };
    static const JSCFunctionListEntry mouse_funcs[] = {
        JS_CFUNC_MAGIC_DEF("isDown", 1, efx_js_mouse_query, 0),
        JS_CFUNC_MAGIC_DEF("isPressed", 1, efx_js_mouse_query, 1),
        JS_CFUNC_MAGIC_DEF("isReleased", 1, efx_js_mouse_query, 2),
        JS_CFUNC_MAGIC_DEF("onDown", 1, efx_js_mouse_on, 0),
        JS_CFUNC_MAGIC_DEF("onUp", 1, efx_js_mouse_on, 1),
        JS_CFUNC_MAGIC_DEF("onMove", 1, efx_js_mouse_on, 2),
        JS_CFUNC_MAGIC_DEF("onWheel", 1, efx_js_mouse_on, 3),
        JS_CGETSET_DEF("position", efx_js_mouse_getPosition, NULL),
        JS_CGETSET_DEF("x", efx_js_mouse_getX, NULL),
        JS_CGETSET_DEF("y", efx_js_mouse_getY, NULL),
        JS_CGETSET_DEF("delta", efx_js_mouse_getDelta, NULL),
        JS_CGETSET_DEF("wheel", efx_js_mouse_getWheel, NULL),
    };
    static const JSCFunctionListEntry window_funcs[] = {
        JS_CGETSET_DEF("size", efx_js_window_getSize, NULL),
        JS_CGETSET_DEF("width", efx_js_window_getWidth, NULL),
        JS_CGETSET_DEF("height", efx_js_window_getHeight, NULL),
        JS_CGETSET_DEF("dpiScale", efx_js_window_getDpiScale, NULL),
    };
    static const JSCFunctionListEntry gamepad_funcs[] = {
        JS_CFUNC_DEF("get", 1, efx_js_gamepad_get),
        JS_CFUNC_MAGIC_DEF("onConnect", 1, efx_js_gamepad_on, 0),
        JS_CFUNC_MAGIC_DEF("onDisconnect", 1, efx_js_gamepad_on, 1),
        JS_CGETSET_DEF("count", efx_js_gamepad_count, NULL),
    };
    JSValue kb = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, kb, kb_funcs,
                               (int)(sizeof(kb_funcs) / sizeof(kb_funcs[0])));
    JSValue mouse = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, mouse, mouse_funcs,
                               (int)(sizeof(mouse_funcs) /
                                     sizeof(mouse_funcs[0])));
    JSValue window = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, window, window_funcs,
                               (int)(sizeof(window_funcs) /
                                     sizeof(window_funcs[0])));
    JSValue gamepad = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, gamepad, gamepad_funcs,
                               (int)(sizeof(gamepad_funcs) /
                                     sizeof(gamepad_funcs[0])));
    /* JS_SetPropertyStr consumes the value reference */
    JS_SetPropertyStr(ctx, efx, "keyboard", kb);
    JS_SetPropertyStr(ctx, efx, "mouse", mouse);
    JS_SetPropertyStr(ctx, efx, "window", window);
    JS_SetPropertyStr(ctx, efx, "gamepad", gamepad);
    return 0;
}

/* ================================================= F12 physics namespace */

static JSValue physics_get_gravity(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    return vec3_to_js(ctx, efx_physics_gravity(physics_world(ctx)));
}

static JSValue physics_set_gravity(JSContext *ctx, JSValueConst this_val,
                                   JSValueConst val) {
    (void)this_val;
    float f[3];
    if (get_float_array(ctx, val, f, 3) != 0) return JS_EXCEPTION;
    efx_physics_set_gravity(physics_world(ctx), efx_v3(f[0], f[1], f[2]));
    return JS_UNDEFINED;
}

static JSValue physics_get_iterations(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    return JS_NewInt32(ctx, efx_physics_iterations(physics_world(ctx)));
}

static JSValue physics_set_iterations(JSContext *ctx, JSValueConst this_val,
                                      JSValueConst val) {
    (void)this_val;
    double d;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d) || floor(d) != d ||
        d < 1) {
        return range_error(ctx, "iterations must be a positive integer");
    }
    efx_physics_set_iterations(physics_world(ctx), (int)d);
    return JS_UNDEFINED;
}

static JSValue physics_step(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return type_error(ctx, "step requires dt");
    double dt;
    if (JS_ToFloat64(ctx, &dt, argv[0]) < 0 || !isfinite(dt)) {
        return type_error(ctx, "dt must be a finite number");
    }
    efx_physics_step(physics_world(ctx), (float)dt);
    return JS_UNDEFINED;
}

static JSValue physics_clear(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    efx_physics_clear(physics_world(ctx));
    physics_release_wrappers(ctx);
    return JS_UNDEFINED;
}

/* build a static mesh collider from a live Mesh + a static mesh descriptor */
static efx_phys_body create_mesh_body(JSContext *ctx, efx_physics_world *w,
                                      efxjs_mesh *m,
                                      const efx_static_mesh_desc *sd) {
    int verts = 0, indices = 0;
    if (!efx_render_mesh_geometry_count(m->handle, &verts, &indices) ||
        verts <= 0 || indices < 3 || indices % 3 != 0) {
        type_error(ctx, "mesh has no triangles");
        return 0;
    }
    float *pos = malloc((size_t)verts * 3 * sizeof(float));
    uint32_t *idx = malloc((size_t)indices * sizeof(uint32_t));
    if (!pos || !idx) {
        free(pos);
        free(idx);
        generic_error(ctx, "out of memory");
        return 0;
    }
    efx_render_mesh_geometry(m->handle, pos, idx);
    efx_phys_body h =
        efx_physics_create_static_mesh(w, pos, verts, idx, indices / 3, sd);
    free(pos);
    free(idx);
    if (!h) generic_error(ctx, "failed to create mesh collider");
    return h;
}

static int read_common_body_opts(JSContext *ctx, JSValueConst opts, int *sensor,
                                 double *friction, double *restitution,
                                 efx_vec3 *position, uint32_t *layer,
                                 uint32_t *mask) {
    *sensor = 0;
    *friction = 0.5;
    *restitution = 0.0;
    *position = efx_v3(0, 0, 0);
    *layer = 0xFFFFFFFFu;
    *mask = 0xFFFFFFFFu;
    if (phys_opt_bool(ctx, opts, "sensor", sensor) < 0) return -1;
    if (phys_opt_number(ctx, opts, "friction", friction) < 0) return -1;
    if (phys_opt_number(ctx, opts, "restitution", restitution) < 0) return -1;
    if (phys_opt_vec3(ctx, opts, "position", position) < 0) return -1;
    if (phys_opt_mask(ctx, opts, "layer", layer) < 0) return -1;
    if (phys_opt_mask(ctx, opts, "mask", mask) < 0) return -1;
    if (*friction < 0) {
        range_error(ctx, "friction must not be negative");
        return -1;
    }
    if (*restitution < 0 || *restitution > 1) {
        range_error(ctx, "restitution must be in [0, 1]");
        return -1;
    }
    return 0;
}

static JSValue physics_createBody(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return type_error(ctx, "createBody requires an options object");
    }
    JSValueConst opts = argv[0];
    static const char *known[] = {"dynamic", "sensor",      "shape",
                                  "position", "mass",       "friction",
                                  "restitution", "layer",    "mask"};
    if (check_known_fields(ctx, opts, known, 9, "createBody") != 0) {
        return JS_EXCEPTION;
    }
    JSValue sv = JS_GetPropertyStr(ctx, opts, "shape");
    parsed_shape ps;
    int prc = parse_shape(ctx, sv, &ps);
    JS_FreeValue(ctx, sv);
    if (prc != 0) return JS_EXCEPTION;

    int dynamic = 0;
    phys_opt_bool(ctx, opts, "dynamic", &dynamic);
    double mass = 1.0;
    if (phys_opt_number(ctx, opts, "mass", &mass) < 0) return JS_EXCEPTION;
    if (dynamic && !(mass > 0)) {
        return range_error(ctx, "dynamic bodies require a positive mass");
    }
    int sensor = 0;
    double friction = 0, rest = 0;
    efx_vec3 position;
    uint32_t layer, mask;
    if (read_common_body_opts(ctx, opts, &sensor, &friction, &rest, &position,
                              &layer, &mask) != 0) {
        return JS_EXCEPTION;
    }

    efx_physics_world *w = physics_world(ctx);
    if (ps.mesh_src) {
        if (dynamic) {
            return type_error(ctx, "mesh colliders are static only");
        }
        efx_static_mesh_desc sd;
        memset(&sd, 0, sizeof(sd));
        sd.position = position;
        sd.sensor = sensor;
        sd.friction = (float)friction;
        sd.restitution = (float)rest;
        sd.layer = layer;
        sd.mask = mask;
        efx_phys_body h = create_mesh_body(ctx, w, ps.mesh_src, &sd);
        if (!h) return JS_EXCEPTION;
        return wrap_body(ctx, w, h);
    }
    efx_body_desc d;
    memset(&d, 0, sizeof(d));
    d.dynamic = dynamic;
    d.sensor = sensor;
    d.shape = ps.shape;
    d.position = position;
    d.mass = (float)mass;
    d.friction = (float)friction;
    d.restitution = (float)rest;
    d.layer = layer;
    d.mask = mask;
    efx_phys_body h = efx_physics_create_body(w, &d);
    if (!h) return generic_error(ctx, "failed to create body");
    return wrap_body(ctx, w, h);
}

static JSValue physics_createStaticMesh(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return type_error(ctx, "createStaticMesh requires a Mesh");
    efxjs_mesh *m = get_live_mesh(ctx, argv[0]);
    if (!m) return JS_EXCEPTION;
    JSValueConst opts = JS_UNDEFINED;
    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        if (!JS_IsObject(argv[1])) {
            return type_error(ctx, "createStaticMesh options must be an object");
        }
        opts = argv[1];
        static const char *known[] = {"position", "sensor", "friction",
                                      "restitution", "layer", "mask"};
        if (check_known_fields(ctx, opts, known, 6, "createStaticMesh") != 0) {
            return JS_EXCEPTION;
        }
    }
    int sensor = 0;
    double friction = 0.5, rest = 0;
    efx_vec3 position = efx_v3(0, 0, 0);
    uint32_t layer = 0xFFFFFFFFu, mask = 0xFFFFFFFFu;
    if (JS_IsObject(opts) &&
        read_common_body_opts(ctx, opts, &sensor, &friction, &rest, &position,
                              &layer, &mask) != 0) {
        return JS_EXCEPTION;
    }
    efx_static_mesh_desc sd;
    memset(&sd, 0, sizeof(sd));
    sd.position = position;
    sd.sensor = sensor;
    sd.friction = (float)friction;
    sd.restitution = (float)rest;
    sd.layer = layer;
    sd.mask = mask;
    efx_physics_world *w = physics_world(ctx);
    efx_phys_body h = create_mesh_body(ctx, w, m, &sd);
    if (!h) return JS_EXCEPTION;
    return wrap_body(ctx, w, h);
}

static JSValue physics_createCharacter(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return type_error(ctx, "createCharacter requires an options object");
    }
    JSValueConst opts = argv[0];
    static const char *known[] = {
        "radius",    "height",       "position",    "up",
        "floorMaxAngle", "floorSnapLength", "stepHeight", "maxSlides",
        "safeMargin", "layer",       "mask"};
    if (check_known_fields(ctx, opts, known, 11, "createCharacter") != 0) {
        return JS_EXCEPTION;
    }
    double radius = 0, height = 0;
    int has_r = phys_opt_number(ctx, opts, "radius", &radius);
    int has_h = phys_opt_number(ctx, opts, "height", &height);
    if (has_r < 0 || has_h < 0) return JS_EXCEPTION;
    if (has_r != 1 || has_h != 1) {
        return type_error(ctx, "createCharacter requires radius and height");
    }
    if (!(radius > 0)) return range_error(ctx, "radius must be positive");
    if (!(height >= 2 * radius)) {
        return range_error(ctx, "height must be at least 2 * radius");
    }
    efx_vec3 position = efx_v3(0, 0, 0), up = efx_v3(0, 1, 0);
    if (phys_opt_vec3(ctx, opts, "position", &position) < 0) return JS_EXCEPTION;
    if (phys_opt_vec3(ctx, opts, "up", &up) < 0) return JS_EXCEPTION;
    if (efx_v3_len_sq(up) <= 0) return range_error(ctx, "up must be non-zero");

    double floor_max_angle = 45, snap = 0.1, step_height = 0.3, safe = 0.001;
    if (phys_opt_number(ctx, opts, "floorMaxAngle", &floor_max_angle) < 0)
        return JS_EXCEPTION;
    if (phys_opt_number(ctx, opts, "floorSnapLength", &snap) < 0)
        return JS_EXCEPTION;
    if (phys_opt_number(ctx, opts, "stepHeight", &step_height) < 0)
        return JS_EXCEPTION;
    if (phys_opt_number(ctx, opts, "safeMargin", &safe) < 0) return JS_EXCEPTION;
    double max_slides = 6;
    if (phys_opt_number(ctx, opts, "maxSlides", &max_slides) < 0)
        return JS_EXCEPTION;
    if (!(max_slides >= 1) || floor(max_slides) != max_slides) {
        return range_error(ctx, "maxSlides must be a positive integer");
    }
    if (snap < 0) return range_error(ctx, "floorSnapLength must not be negative");
    if (step_height < 0) return range_error(ctx, "stepHeight must not be negative");
    if (safe < 0) return range_error(ctx, "safeMargin must not be negative");
    uint32_t layer = 0xFFFFFFFFu, mask = 0xFFFFFFFFu;
    if (phys_opt_mask(ctx, opts, "layer", &layer) < 0) return JS_EXCEPTION;
    if (phys_opt_mask(ctx, opts, "mask", &mask) < 0) return JS_EXCEPTION;

    efx_character_desc d;
    memset(&d, 0, sizeof(d));
    d.radius = (float)radius;
    d.height = (float)height;
    d.position = position;
    d.up = up;
    d.floor_max_angle = (float)floor_max_angle;
    d.floor_snap_length = (float)snap;
    d.step_height = (float)step_height;
    d.safe_margin = (float)safe;
    d.max_slides = (int)max_slides;
    d.layer = layer;
    d.mask = mask;
    efx_physics_world *w = physics_world(ctx);
    efx_phys_character h = efx_physics_create_character(w, &d);
    if (!h) return generic_error(ctx, "failed to create character");
    return wrap_character(ctx, w, h);
}

static JSValue ray_hit_to_js(JSContext *ctx, const efx_ray_hit *h) {
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "point", vec3_to_js(ctx, h->point));
    JS_SetPropertyStr(ctx, o, "normal", vec3_to_js(ctx, h->normal));
    JS_SetPropertyStr(ctx, o, "distance", JS_NewFloat64(ctx, h->distance));
    JSValue body = JS_NULL;
    if (h->character) {
        body = find_character_wrapper(ctx, h->character);
    } else {
        body = find_body_wrapper(ctx, h->body);
    }
    JS_SetPropertyStr(ctx, o, "body", body);
    return o;
}

static JSValue physics_raycast(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return type_error(ctx, "raycast requires origin and direction");
    float o[3], d[3];
    if (get_float_array(ctx, argv[0], o, 3) != 0) return JS_EXCEPTION;
    if (get_float_array(ctx, argv[1], d, 3) != 0) return JS_EXCEPTION;
    JSValueConst opts = JS_UNDEFINED;
    if (argc >= 3 && !JS_IsUndefined(argv[2])) {
        if (!JS_IsObject(argv[2])) {
            return type_error(ctx, "raycast options must be an object");
        }
        opts = argv[2];
        static const char *known[] = {"maxDistance", "mask", "all", "sensors"};
        if (check_known_fields(ctx, opts, known, 4, "raycast") != 0) {
            return JS_EXCEPTION;
        }
    }
    double maxd = 0;
    int has = JS_IsObject(opts) ? phys_opt_number(ctx, opts, "maxDistance", &maxd)
                                : 0;
    if (has < 0) return JS_EXCEPTION;
    if (has != 1 || !(maxd > 0)) {
        return type_error(ctx, "raycast requires a positive maxDistance");
    }
    uint32_t mask = 0xFFFFFFFFu;
    int all = 0, sensors = 0;
    if (JS_IsObject(opts)) {
        if (phys_opt_mask(ctx, opts, "mask", &mask) < 0) return JS_EXCEPTION;
        if (phys_opt_bool(ctx, opts, "all", &all) < 0) return JS_EXCEPTION;
        if (phys_opt_bool(ctx, opts, "sensors", &sensors) < 0) return JS_EXCEPTION;
    }
    efx_ray_hit hits[256];
    int cap = all ? 256 : 1;
    int n = efx_physics_raycast(physics_world(ctx), efx_v3(o[0], o[1], o[2]),
                                efx_v3(d[0], d[1], d[2]), (float)maxd, mask,
                                sensors, all, hits, cap);
    if (!all) {
        if (n == 0) return JS_NULL;
        return ray_hit_to_js(ctx, &hits[0]);
    }
    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < n; i++) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, ray_hit_to_js(ctx, &hits[i]));
    }
    return arr;
}

static JSValue physics_overlap(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return type_error(ctx, "overlap requires a shape");
    parsed_shape ps;
    if (parse_shape(ctx, argv[0], &ps) != 0) return JS_EXCEPTION;
    JSValueConst opts = JS_UNDEFINED;
    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        if (!JS_IsObject(argv[1])) {
            return type_error(ctx, "overlap options must be an object");
        }
        opts = argv[1];
        static const char *known[] = {"position", "mask"};
        if (check_known_fields(ctx, opts, known, 2, "overlap") != 0) {
            return JS_EXCEPTION;
        }
    }
    efx_vec3 position = efx_v3(0, 0, 0);
    uint32_t mask = 0xFFFFFFFFu;
    if (JS_IsObject(opts)) {
        if (phys_opt_vec3(ctx, opts, "position", &position) < 0)
            return JS_EXCEPTION;
        if (phys_opt_mask(ctx, opts, "mask", &mask) < 0) return JS_EXCEPTION;
    }
    efx_phys_mesh *temp = NULL;
    if (ps.mesh_src) {
        temp = build_temp_mesh(ctx, ps.mesh_src);
        if (!temp) return JS_EXCEPTION;
        ps.shape.mesh = temp;
    }
    efx_physics_world *w = physics_world(ctx);
    int count = efx_physics_overlap(w, &ps.shape, position, mask, 1, NULL, 0);
    JSValue arr = JS_NewArray(ctx);
    if (count > 0) {
        efx_overlap_hit *hits = calloc((size_t)count, sizeof(*hits));
        if (!hits) {
            efx_phys_mesh_free(temp);
            return generic_error(ctx, "out of memory");
        }
        int n = efx_physics_overlap(w, &ps.shape, position, mask, 1, hits,
                                    count);
        for (int i = 0; i < n; i++) {
            JSValue h = hits[i].character
                            ? find_character_wrapper(ctx, hits[i].character)
                            : find_body_wrapper(ctx, hits[i].body);
            JS_SetPropertyUint32(ctx, arr, (uint32_t)i, h);
        }
        free(hits);
    }
    efx_phys_mesh_free(temp);
    return arr;
}

static JSValue physics_shapeCast(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) {
        return type_error(ctx, "shapeCast requires shape, from and motion");
    }
    parsed_shape ps;
    if (parse_shape(ctx, argv[0], &ps) != 0) return JS_EXCEPTION;
    float from[3], motion[3];
    if (get_float_array(ctx, argv[1], from, 3) != 0) return JS_EXCEPTION;
    if (get_float_array(ctx, argv[2], motion, 3) != 0) return JS_EXCEPTION;
    JSValueConst opts = JS_UNDEFINED;
    if (argc >= 4 && !JS_IsUndefined(argv[3])) {
        if (!JS_IsObject(argv[3])) {
            return type_error(ctx, "shapeCast options must be an object");
        }
        opts = argv[3];
        static const char *known[] = {"mask", "sensors"};
        if (check_known_fields(ctx, opts, known, 2, "shapeCast") != 0) {
            return JS_EXCEPTION;
        }
    }
    uint32_t mask = 0xFFFFFFFFu;
    int sensors = 0;
    if (JS_IsObject(opts)) {
        if (phys_opt_mask(ctx, opts, "mask", &mask) < 0) return JS_EXCEPTION;
        if (phys_opt_bool(ctx, opts, "sensors", &sensors) < 0)
            return JS_EXCEPTION;
    }
    efx_phys_mesh *temp = NULL;
    if (ps.mesh_src) {
        temp = build_temp_mesh(ctx, ps.mesh_src);
        if (!temp) return JS_EXCEPTION;
        ps.shape.mesh = temp;
    }
    efx_shape_hit hit;
    int rc = efx_physics_shape_cast(physics_world(ctx), &ps.shape,
                                    efx_v3(from[0], from[1], from[2]),
                                    efx_v3(motion[0], motion[1], motion[2]),
                                    mask, sensors, &hit);
    efx_phys_mesh_free(temp);
    if (!rc) return JS_NULL;
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "point", vec3_to_js(ctx, hit.point));
    JS_SetPropertyStr(ctx, o, "normal", vec3_to_js(ctx, hit.normal));
    JS_SetPropertyStr(ctx, o, "fraction", JS_NewFloat64(ctx, hit.fraction));
    JSValue body = hit.character ? find_character_wrapper(ctx, hit.character)
                                 : find_body_wrapper(ctx, hit.body);
    JS_SetPropertyStr(ctx, o, "body", body);
    return o;
}

int efx_api_register_physics(JSContext *ctx, JSValueConst efx) {
    static const JSCFunctionListEntry physics_funcs[] = {
        JS_CFUNC_DEF("step", 1, physics_step),
        JS_CFUNC_DEF("clear", 0, physics_clear),
        JS_CFUNC_DEF("createBody", 1, physics_createBody),
        JS_CFUNC_DEF("createCharacter", 1, physics_createCharacter),
        JS_CFUNC_DEF("createStaticMesh", 2, physics_createStaticMesh),
        JS_CFUNC_DEF("raycast", 2, physics_raycast),
        JS_CFUNC_DEF("overlap", 1, physics_overlap),
        JS_CFUNC_DEF("shapeCast", 3, physics_shapeCast),
        JS_CGETSET_DEF("gravity", physics_get_gravity, physics_set_gravity),
        JS_CGETSET_DEF("iterations", physics_get_iterations,
                       physics_set_iterations),
    };
    JSValue phys = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, phys, physics_funcs,
                               (int)(sizeof(physics_funcs) /
                                     sizeof(physics_funcs[0])));
    /* JS_SetPropertyStr consumes the value reference */
    JS_SetPropertyStr(ctx, efx, "physics", phys);
    return 0;
}
