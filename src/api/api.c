#include "api/api_internal.h"


struct efx_host_state *efx_api_host_state(JSContext *ctx) {
    return (struct efx_host_state *)JS_GetContextOpaque(ctx);
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
    struct efx_host_state *h = efx_api_host_state(ctx);
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
    struct efx_host_state *h = efx_api_host_state(ctx);
    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < h->arg_count; i++) {
        char idx[16];
        snprintf(idx, sizeof(idx), "%d", i);
        JS_SetPropertyStr(ctx, arr, idx, JS_NewString(ctx, h->args[i]));
    }
    return arr;
}


/* ------------------------------------------------------------ helpers */

JSValue efx_api_type_error(JSContext *ctx, const char *msg) {
    return JS_ThrowTypeError(ctx, "%s", msg);
}


JSValue efx_api_range_error(JSContext *ctx, const char *msg) {
    return JS_ThrowRangeError(ctx, "%s", msg);
}


JSValue efx_api_generic_error(JSContext *ctx, const char *msg) {
    return JS_ThrowInternalError(ctx, "%s", msg);
}


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
    struct efx_host_state *h = efx_api_host_state(ctx);
    struct efx_hook_list *list = efx_host_hook_list(h, magic);
    int32_t idx = -1;
    JS_ToInt32(ctx, &idx, func_data[0]);
    if (idx >= 0 && idx < list->count) {
        list->entries[idx].active = 0; /* idempotent: repeated calls are no-ops */
    }
    return JS_UNDEFINED;
}


JSValue efx_api_register_hook(JSContext *ctx, JSValueConst fn, int which) {
    if (!JS_IsFunction(ctx, fn)) {
        return efx_api_type_error(ctx, "hook must be a function");
    }
    struct efx_host_state *h = efx_api_host_state(ctx);
    struct efx_hook_list *list = efx_host_hook_list(h, which);
    int idx = efx_hooks_append(ctx, list, fn);
    if (idx < 0) {
        return efx_api_generic_error(ctx, "out of memory");
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
        return efx_api_type_error(ctx, "registerUpdateHook requires a function");
    }
    return efx_api_register_hook(ctx, argv[0], EFX_HOOK_LIST_UPDATE);
}


JSValue efx_js_registerRenderHook(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "registerRenderHook requires a function");
    }
    return efx_api_register_hook(ctx, argv[0], EFX_HOOK_LIST_RENDER);
}


void efx_api_sink_float(void *ud, int32_t i, double d) {
    ((float *)ud)[i] = (float)d;
}


void efx_api_sink_float_cap3(void *ud, int32_t i, double d) {
    if (i < 3) {
        ((float *)ud)[i] = (float)d;
    }
}


void efx_api_sink_u32(void *ud, int32_t i, double d) {
    ((uint32_t *)ud)[i] = (uint32_t)d;
}


int efx_api_read_elements(JSContext *ctx, JSValueConst v, int32_t len,
                         efx_elem_policy policy, const char *msg_numbers,
                         const char *msg_finite, const char *msg_int,
                         void (*sink)(void *, int32_t, double), void *ud) {
    for (int32_t i = 0; i < len; i++) {
        JSValue ev = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
        double d = 0;
        if (policy == EFX_ELEM_COERCE) {
            if (JS_ToFloat64(ctx, &d, ev) < 0) {
                JS_FreeValue(ctx, ev);
                return -1;
            }
            JS_FreeValue(ctx, ev);
            if (!isfinite(d)) {
                efx_api_range_error(ctx, msg_finite);
                return -1;
            }
        } else if (policy == EFX_ELEM_NUMBER) {
            if (!JS_IsNumber(ev)) {
                JS_FreeValue(ctx, ev);
                efx_api_type_error(ctx, msg_numbers);
                return -1;
            }
            if (JS_ToFloat64(ctx, &d, ev) < 0) {
                JS_FreeValue(ctx, ev);
                return -1;
            }
            JS_FreeValue(ctx, ev);
            if (!isfinite(d)) {
                efx_api_range_error(ctx, msg_finite);
                return -1;
            }
        } else { /* EFX_ELEM_U32 */
            int bad = !JS_IsNumber(ev) || JS_ToFloat64(ctx, &d, ev) < 0;
            JS_FreeValue(ctx, ev);
            if (bad) {
                efx_api_type_error(ctx, msg_numbers);
                return -1;
            }
            if (!isfinite(d) || d < 0 || d > 4294967295.0 || d != floor(d)) {
                efx_api_range_error(ctx, msg_int);
                return -1;
            }
        }
        sink(ud, i, d);
    }
    return 0;
}


/* read a flat array (JS array or typed array) of exactly n floats;
   returns 0 ok, -1 wrong type (TypeError thrown), -2 wrong length or
   non-finite/out-of-range element (RangeError thrown) */
int efx_api_get_float_array(JSContext *ctx, JSValueConst v, float *out, int n) {
    uint8_t *bytes = NULL;
    size_t blen = 0;

    if (JS_IsArray(v)) {
        /* fall through to element loop */
    } else if ((bytes = JS_GetUint8Array(ctx, &blen, v)) != NULL) {
        if ((int)blen != n) {
            efx_api_range_error(ctx, "wrong buffer length");
            return -2;
        }
        for (int i = 0; i < n; i++) {
            out[i] = (float)bytes[i];
        }
        return 0;
    } else {
        efx_api_type_error(ctx, "expected an array");
        return -1;
    }

    JSValue lenv = JS_GetPropertyStr(ctx, v, "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    if (len != n) {
        efx_api_range_error(ctx, "wrong array length");
        return -2;
    }
    if (efx_api_read_elements(ctx, v, n, EFX_ELEM_COERCE, NULL,
                      "array elements must be finite numbers", NULL,
                      efx_api_sink_float, out) != 0) {
        return -2;
    }
    return 0;
}


/* generic optional-field readers: 1 present, 0 absent, -1 error (throws).
 * The wording is passed in per call so every domain keeps its existing
 * message text (the error catalog pins them). */

int efx_api_opt_number(JSContext *ctx, JSValueConst obj, const char *key,
                      double *out, const char *msg) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (JS_ToFloat64(ctx, out, v) < 0 || !isfinite(*out)) {
        JS_FreeValue(ctx, v);
        efx_api_type_error(ctx, msg);
        return -1;
    }
    JS_FreeValue(ctx, v);
    return 1;
}


int efx_api_opt_bool(JSContext *ctx, JSValueConst obj, const char *key,
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


int efx_api_opt_u32(JSContext *ctx, JSValueConst obj, const char *key,
                   uint32_t *out, const char *msg_num, const char *msg_range) {
    double d;
    int r = efx_api_opt_number(ctx, obj, key, &d, msg_num);
    if (r <= 0) {
        return r;
    }
    if (d < 0 || d > 4294967295.0 || floor(d) != d) {
        efx_api_range_error(ctx, msg_range);
        return -1;
    }
    *out = (uint32_t)d;
    return 1;
}


int efx_api_opt_vec3(JSContext *ctx, JSValueConst obj, const char *key,
                    float out[3]) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    int r = efx_api_get_float_array(ctx, v, out, 3);
    JS_FreeValue(ctx, v);
    return r == 0 ? 1 : -1;
}


JSClassID texture_class_id;

JSClassID imagedata_class_id;

JSClassID meshdata_class_id;

JSClassID mesh_class_id;

JSClassID rendertarget_class_id;

JSClassID fontdata_class_id;

JSClassID font_class_id;

JSClassID particlesystem_class_id;

JSClassID audiodata_class_id;

JSClassID audiostream_class_id;

JSClassID audio_class_id;


/* shared finalizer skeleton: unwrap the class's wrapper, run the per-class
 * release step, then free the wrapper. Classes whose finalizer must also
 * unlink host bookkeeping (Body/Character) keep their own finalizer. */
static void finalize_common(JSValue val, JSClassID id,
                            void (*release)(void *)) {
    void *p = JS_GetOpaque(val, id);
    if (p) {
        release(p);
        free(p);
    }
}


static void texture_release(void *p) {
    efxjs_texture *t = (efxjs_texture *)p;
    if (t->alive && !t->permanent) {
        efx_render_texture_destroy(t->handle);
    }
}


static void imagedata_release(void *p) {
    free(((efxjs_imagedata *)p)->pixels);
}


static void meshdata_release(void *p) {
    efx_meshdata_destroy(((efxjs_meshdata *)p)->md);
}


static void mesh_release(void *p) {
    efxjs_mesh *m = (efxjs_mesh *)p;
    if (m->alive) {
        efx_render_mesh_destroy(m->handle);
    }
}


static void rendertarget_release(void *p) {
    efxjs_rendertarget *t = (efxjs_rendertarget *)p;
    if (t->alive) {
        efx_render_target_destroy(t->handle);
    }
}


static void fontdata_release(void *p) {
    efx_text_fontdata_destroy(((efxjs_fontdata *)p)->fd);
}


static void font_release(void *p) {
    efx_text_font_destroy(((efxjs_font *)p)->font);
}


static void texture_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    finalize_common(val, texture_class_id, texture_release);
}


static void imagedata_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    finalize_common(val, imagedata_class_id, imagedata_release);
}


static void meshdata_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    finalize_common(val, meshdata_class_id, meshdata_release);
}


static void mesh_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    finalize_common(val, mesh_class_id, mesh_release);
}


static void rendertarget_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    finalize_common(val, rendertarget_class_id, rendertarget_release);
}


static void fontdata_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    finalize_common(val, fontdata_class_id, fontdata_release);
}


static void font_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    finalize_common(val, font_class_id, font_release);
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
            return efx_api_type_error(ctx, "cannot destroy an engine-owned texture");
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
    return efx_api_type_error(ctx, "not a resource object");
}


static void particlesystem_release(void *p) {
    efxjs_particlesystem *s = (efxjs_particlesystem *)p;
    if (s->alive) {
        efx_render_particles_destroy(s->handle);
    }
}


static void audiodata_release(void *p) {
    efxjs_audiodata *d = (efxjs_audiodata *)p;
    if (d->data) {
        efx_audio_data_release(d->data);
    }
}


static void audiostream_release(void *p) {
    efxjs_audiostream *s = (efxjs_audiostream *)p;
    if (s->stream) {
        efx_audio_stream_release(s->stream);
    }
}


static void audio_release(void *p) {
    (void)p; /* dropping the handle never cuts off a fire-and-forget sound */
}


static void particlesystem_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    finalize_common(val, particlesystem_class_id, particlesystem_release);
}


static void audiodata_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    finalize_common(val, audiodata_class_id, audiodata_release);
}


static void audiostream_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    finalize_common(val, audiostream_class_id, audiostream_release);
}


static void audio_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    finalize_common(val, audio_class_id, audio_release);
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


JSClassID body_class_id;

JSClassID character_class_id;


/* generic live-opaque resolver: unwrap the wrapper for `id`, then check it is
 * alive. Returns the wrapper or NULL with a TypeError thrown. The messages and
 * the alive predicate are passed per class so every class keeps its existing
 * wording (the error catalog pins them). */
static void *live_opaque(JSContext *ctx, JSValueConst v, JSClassID id,
                         const char *type_msg, const char *dead_msg,
                         int (*alive)(const void *)) {
    void *p = JS_GetOpaque2(ctx, v, id);
    if (!p) {
        efx_api_type_error(ctx, type_msg);
        return NULL;
    }
    if (!alive(p)) {
        efx_api_type_error(ctx, dead_msg);
        return NULL;
    }
    return p;
}


static int texture_alive(const void *p) {
    return ((const efxjs_texture *)p)->alive;
}

static int imagedata_alive(const void *p) {
    return ((const efxjs_imagedata *)p)->alive;
}

static int meshdata_alive(const void *p) {
    return ((const efxjs_meshdata *)p)->alive;
}

static int mesh_alive(const void *p) {
    return ((const efxjs_mesh *)p)->alive;
}

static int target_alive(const void *p) {
    return ((const efxjs_rendertarget *)p)->alive;
}

static int font_alive(const void *p) {
    return ((const efxjs_font *)p)->alive;
}

static int ps_alive(const void *p) {
    return ((const efxjs_particlesystem *)p)->alive;
}

static int body_alive(const void *p) {
    const efxjs_body *b = (const efxjs_body *)p;
    return b->alive && b->w && efx_physics_body_alive(b->w, b->handle);
}

static int character_alive(const void *p) {
    const efxjs_character *c = (const efxjs_character *)p;
    return c->alive && c->w &&
           efx_physics_character_alive(c->w, c->handle);
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
void efx_api_body_unpin(JSContext *ctx, efxjs_body *b) {
    if (!b->pinned) return;
    b->pinned = 0;
    JS_FreeValue(ctx, b->self);
}


void efx_api_character_unpin(JSContext *ctx, efxjs_character *c) {
    if (!c->pinned) return;
    c->pinned = 0;
    JS_FreeValue(ctx, c->self);
}


/* marks every wrapper destroyed and releases the world's references (after
 * efx_physics_clear, and at runtime teardown before the context is freed) */
void efx_api_physics_release_wrappers(JSContext *ctx) {
    struct efx_host_state *h = efx_api_host_state(ctx);
    efxjs_body *b = (efxjs_body *)h->physics_bodies;
    while (b) {
        efxjs_body *next = b->next;
        b->alive = 0;
        efx_api_body_unpin(ctx, b);
        b = next;
    }
    efxjs_character *c = (efxjs_character *)h->physics_characters;
    while (c) {
        efxjs_character *next = c->next;
        c->alive = 0;
        efx_api_character_unpin(ctx, c);
        c = next;
    }
}


void efx_api_physics_release(JSContext *ctx) {
    efx_api_physics_release_wrappers(ctx);
}


/* ---- F12 binding helpers ---- */

/* live Mesh resolution for static-mesh colliders (defined with the F3
 * bindings); on failure it throws and returns NULL */
static efxjs_texture *get_live_texture(JSContext *ctx, JSValueConst v);

static efxjs_font *get_live_font(JSContext *ctx, JSValueConst v);


efxjs_body *efx_api_get_live_body(JSContext *ctx, JSValueConst v) {
    return live_opaque(ctx, v, body_class_id, "expected a Body",
                       "using a destroyed Body", body_alive);
}


efxjs_character *efx_api_get_live_character(JSContext *ctx, JSValueConst v) {
    return live_opaque(ctx, v, character_class_id, "expected a Character",
                       "using a destroyed Character", character_alive);
}


/* read-only query properties (Texture.width / Texture.height), resolved
 * through the render layer's texture registry at read time */
static JSValue efx_js_texture_getWidth(JSContext *ctx, JSValueConst this_val) {
    efxjs_texture *t = get_live_texture(ctx, this_val);
    if (!t) {
        return JS_EXCEPTION;
    }
    int w = 0, h = 0;
    efx_render_texture_size(t->handle, &w, &h);
    return JS_NewInt32(ctx, w);
}


static JSValue efx_js_texture_getHeight(JSContext *ctx, JSValueConst this_val) {
    efxjs_texture *t = get_live_texture(ctx, this_val);
    if (!t) {
        return JS_EXCEPTION;
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
    efxjs_imagedata *d = efx_api_get_live_imagedata(ctx, this_val);
    if (!d) {
        return JS_EXCEPTION;
    }
    return JS_NewInt32(ctx, d->w);
}


static JSValue efx_js_imagedata_getHeight(JSContext *ctx, JSValueConst this_val) {
    efxjs_imagedata *d = efx_api_get_live_imagedata(ctx, this_val);
    if (!d) {
        return JS_EXCEPTION;
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
    efxjs_meshdata *m = efx_api_get_live_meshdata(ctx, this_val);
    if (!m) {
        return JS_EXCEPTION;
    }
    return JS_NewInt32(ctx, m->md->surface_count);
}


static JSValue efx_js_mesh_getSurfaceCount(JSContext *ctx,
                                           JSValueConst this_val) {
    efxjs_mesh *m = efx_api_get_live_mesh(ctx, this_val);
    if (!m) {
        return JS_EXCEPTION;
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
    efxjs_rendertarget *t = efx_api_get_live_render_target(ctx, this_val);
    if (!t) {
        return JS_EXCEPTION;
    }
    int w = 0, h = 0;
    efx_render_target_size(t->handle, &w, &h);
    return JS_NewInt32(ctx, w);
}


static JSValue efx_js_target_getHeight(JSContext *ctx, JSValueConst this_val) {
    efxjs_rendertarget *t = efx_api_get_live_render_target(ctx, this_val);
    if (!t) {
        return JS_EXCEPTION;
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
static JSValue font_metric(JSContext *ctx, JSValueConst this_val,
                           float (*metric)(const efx_text_font *)) {
    efxjs_font *f = get_live_font(ctx, this_val);
    if (!f) {
        return JS_EXCEPTION;
    }
    return JS_NewFloat64(ctx, (double)metric(f->font));
}


static JSValue efx_js_font_getSize(JSContext *ctx, JSValueConst this_val) {
    return font_metric(ctx, this_val, efx_text_font_size);
}


static JSValue efx_js_font_getLineHeight(JSContext *ctx, JSValueConst this_val) {
    return font_metric(ctx, this_val, efx_text_font_line_height);
}


static JSValue efx_js_font_getAscent(JSContext *ctx, JSValueConst this_val) {
    return font_metric(ctx, this_val, efx_text_font_ascent);
}


static JSValue efx_js_font_getDescent(JSContext *ctx, JSValueConst this_val) {
    return font_metric(ctx, this_val, efx_text_font_descent);
}


static const JSCFunctionListEntry font_proto_funcs[] = {
    JS_CGETSET_DEF("size", efx_js_font_getSize, NULL),
    JS_CGETSET_DEF("lineHeight", efx_js_font_getLineHeight, NULL),
    JS_CGETSET_DEF("ascent", efx_js_font_getAscent, NULL),
    JS_CGETSET_DEF("descent", efx_js_font_getDescent, NULL),
};


/* ------------------------------------------ F11 particle system bindings */

efxjs_particlesystem *efx_api_get_live_ps(JSContext *ctx, JSValueConst v) {
    return live_opaque(ctx, v, particlesystem_class_id,
                       "expected a ParticleSystem",
                       "using a destroyed resource", ps_alive);
}


JSValue efx_api_plain_error(JSContext *ctx, const char *msg) {
    return JS_ThrowPlainError(ctx, "%s", msg);
}


#define EFX_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))


static const efx_class_spec CLASS_SPECS[] = {
    { &texture_class_id, &texture_class_def, texture_proto_funcs,
      EFX_ARRAY_COUNT(texture_proto_funcs), 1 },
    { &imagedata_class_id, &imagedata_class_def, imagedata_proto_funcs,
      EFX_ARRAY_COUNT(imagedata_proto_funcs), 1 },
    { &meshdata_class_id, &meshdata_class_def, meshdata_proto_funcs,
      EFX_ARRAY_COUNT(meshdata_proto_funcs), 1 },
    { &mesh_class_id, &mesh_class_def, mesh_proto_funcs,
      EFX_ARRAY_COUNT(mesh_proto_funcs), 1 },
    { &rendertarget_class_id, &rendertarget_class_def,
      rendertarget_proto_funcs, EFX_ARRAY_COUNT(rendertarget_proto_funcs), 1 },
    { &fontdata_class_id, &fontdata_class_def, NULL, 0, 1 },
    { &font_class_id, &font_class_def, font_proto_funcs,
      EFX_ARRAY_COUNT(font_proto_funcs), 1 },
    { &particlesystem_class_id, &particlesystem_class_def,
      particlesystem_proto_funcs,
      EFX_ARRAY_COUNT(particlesystem_proto_funcs), 1 },
    { &body_class_id, &body_class_def, body_proto_funcs,
      EFX_ARRAY_COUNT(body_proto_funcs), 0 },
    { &character_class_id, &character_class_def, character_proto_funcs,
      EFX_ARRAY_COUNT(character_proto_funcs), 0 },
    { &audiodata_class_id, &audiodata_class_def, NULL, 0, 1 },
    { &audiostream_class_id, &audiostream_class_def, NULL, 0, 1 },
    { &audio_class_id, &audio_class_def, audio_proto_funcs,
      EFX_ARRAY_COUNT(audio_proto_funcs), 1 },
};


int efx_api_init(JSContext *ctx) {
    static int registered;
    if (registered) {
        return 0;
    }
    JSRuntime *rt = JS_GetRuntime(ctx);
    const int nclasses = EFX_ARRAY_COUNT(CLASS_SPECS);
    for (int i = 0; i < nclasses; i++) {
        if (JS_NewClassID(rt, CLASS_SPECS[i].id) != *CLASS_SPECS[i].id) {
            return -1;
        }
    }
    for (int i = 0; i < nclasses; i++) {
        if (JS_NewClass(rt, *CLASS_SPECS[i].id, CLASS_SPECS[i].def) < 0) {
            return -1;
        }
    }
    /* Most classes share one destroy(); Body/Character define their own in
     * their function list. The shared function is dup'd per prototype so each
     * holds its own reference, then released once here. */
    JSValue destroy_fn = JS_NewCFunction(ctx, js_destroy_resource, "destroy", 0);
    for (int i = 0; i < nclasses; i++) {
        JSValue proto = JS_NewObject(ctx);
        if (CLASS_SPECS[i].shared_destroy) {
            JS_SetPropertyStr(ctx, proto, "destroy",
                              JS_DupValue(ctx, destroy_fn));
        }
        if (CLASS_SPECS[i].funcs && CLASS_SPECS[i].nfuncs > 0) {
            JS_SetPropertyFunctionList(ctx, proto, CLASS_SPECS[i].funcs,
                                       CLASS_SPECS[i].nfuncs);
        }
        JS_SetClassProto(ctx, *CLASS_SPECS[i].id, proto);
    }
    JS_FreeValue(ctx, destroy_fn);
    registered = 1;
    return 0;
}


efxjs_imagedata *efx_api_get_live_imagedata(JSContext *ctx, JSValueConst v) {
    return live_opaque(ctx, v, imagedata_class_id, "expected an ImageData",
                       "using a destroyed resource", imagedata_alive);
}


efxjs_rendertarget *efx_api_get_live_render_target(JSContext *ctx,
                                                  JSValueConst v) {
    return live_opaque(ctx, v, rendertarget_class_id,
                       "expected a RenderTarget",
                       "using a destroyed resource", target_alive);
}


static efxjs_texture *get_live_texture(JSContext *ctx, JSValueConst v) {
    return live_opaque(ctx, v, texture_class_id, "expected a Texture",
                       "using a destroyed resource", texture_alive);
}


static efxjs_font *get_live_font(JSContext *ctx, JSValueConst v) {
    return live_opaque(ctx, v, font_class_id, "expected a Font",
                       "using a destroyed resource", font_alive);
}


/* F5a texture coercion: a live Texture or a live RenderTarget — either is
 * accepted wherever a sampling source is required (drawQuad, material
 * maps, alphaMask). Returns 0 and sets *out_handle on success. */
int efx_api_get_live_sample(JSContext *ctx, JSValueConst v, uint64_t *out_handle) {
    efxjs_texture *t = JS_GetOpaque(v, texture_class_id);
    if (t) {
        if (!t->alive) {
            efx_api_type_error(ctx, "using a destroyed resource");
            return -1;
        }
        *out_handle = t->handle;
        return 0;
    }
    efxjs_rendertarget *rt = JS_GetOpaque(v, rendertarget_class_id);
    if (rt) {
        if (!rt->alive) {
            efx_api_type_error(ctx, "using a destroyed resource");
            return -1;
        }
        *out_handle = rt->handle;
        return 0;
    }
    efx_api_type_error(ctx, "expected a Texture or RenderTarget");
    return -1;
}


/* map render-module errors from the F5a redirection calls to JS
 * exceptions; returns a JS value to return from the binding */
JSValue efx_api_target_call_error(JSContext *ctx, int rc) {
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return efx_api_type_error(ctx, "expected a live RenderTarget");
    }
    if (rc == EFX_RENDER_ERR_NESTED) {
        return efx_api_type_error(ctx, "a render target is already active");
    }
    if (rc == EFX_RENDER_ERR_STATE) {
        return efx_api_type_error(ctx, "no render target is active");
    }
    if (rc == EFX_RENDER_ERR_BUDGET) {
        return efx_api_range_error(ctx, "display list budget exceeded");
    }
    if (rc == EFX_RENDER_ERR_NOMEM) {
        return efx_api_generic_error(ctx, "out of memory");
    }
    return efx_api_generic_error(ctx, "render target call failed");
}


/* ------------------------------------------------------------ F3 bindings */

/* read a flat number array (JS array or typed array; elements extracted by
 * index so any typed array kind works). Element rules: non-number →
 * TypeError, non-finite → RangeError (F2 precedent). Returns 0 on success. */
int efx_api_read_number_array(JSContext *ctx, JSValueConst v, float **out,
                             int *out_len, const char *what) {
    int is_ta = JS_GetTypedArrayType(v);
    if (!JS_IsArray(v) && is_ta < 0) {
        efx_api_type_error(ctx, what);
        return -1;
    }
    JSValue lenv = JS_GetPropertyStr(ctx, v, "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    if (len < 0) {
        efx_api_range_error(ctx, what);
        return -2;
    }
    float *buf = len ? malloc((size_t)len * sizeof(float)) : NULL;
    if (len && !buf) {
        efx_api_generic_error(ctx, "out of memory");
        return -3;
    }
    if (efx_api_read_elements(ctx, v, len, EFX_ELEM_NUMBER,
                      "array elements must be numbers",
                      "array elements must be finite numbers", NULL,
                      efx_api_sink_float, buf) != 0) {
        free(buf);
        return -2;
    }
    *out = buf;
    *out_len = (int)len;
    return 0;
}


/* reject unknown fields on an object with a TypeError naming the field */
int efx_api_check_known_fields(JSContext *ctx, JSValueConst obj,
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


/* read a [x,y,z] array (array or typed array) */
int efx_api_read_vec3(JSContext *ctx, JSValueConst v, float out[3],
                     const char *what) {
    int is_ta = JS_GetTypedArrayType(v);
    if (!JS_IsArray(v) && is_ta < 0) {
        efx_api_type_error(ctx, what);
        return -1;
    }
    JSValue lenv = JS_GetPropertyStr(ctx, v, "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    if (len < 0) {
        efx_api_range_error(ctx, what);
        return -1;
    }
    if (efx_api_read_elements(ctx, v, len, EFX_ELEM_NUMBER,
                      "array elements must be numbers",
                      "array elements must be finite numbers", NULL,
                      efx_api_sink_float_cap3, out) != 0) {
        return -1;
    }
    if (len != 3) {
        efx_api_range_error(ctx, "expected 3 numbers");
        return -1;
    }
    return 0;
}


/* parse one Phong channel color: required 4-element array */
int efx_api_read_channel_color(JSContext *ctx, JSValueConst channel,
                              const char *name, float out[4]) {
    JSValue cv = JS_GetPropertyStr(ctx, channel, "color");
    if (JS_IsUndefined(cv)) {
        JS_FreeValue(ctx, cv);
        JS_ThrowTypeError(ctx, "%s channel requires color", name);
        return -1;
    }
    int rc = efx_api_get_float_array(ctx, cv, out, 4);
    JS_FreeValue(ctx, cv);
    return rc == 0 ? 0 : -1;
}


/* parse a material map field (present = live Texture or RenderTarget, F5a;
 * null/omitted = none) */
int efx_api_read_material_map(JSContext *ctx, JSValueConst ch, const char *name,
                             uint64_t *out) {
    JSValue mv = JS_GetPropertyStr(ctx, ch, "map");
    if (JS_IsUndefined(mv) || JS_IsNull(mv)) {
        JS_FreeValue(ctx, mv);
        *out = 0;
        return 0;
    }
    int rc = efx_api_get_live_sample(ctx, mv, out);
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
int efx_api_read_material(JSContext *ctx, JSValueConst v, efx_material *out) {
    if (!JS_IsObject(v)) {
        efx_api_type_error(ctx, "material must be an object");
        return -1;
    }
    efx_material_default(out);
    static const char *known[] = {"ambient", "diffuse", "specular", "emissive",
                                  "alphaMask"};
    if (efx_api_check_known_fields(ctx, v, known, 5, "material") != 0) {
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
        if (efx_api_check_known_fields(ctx, ch, ck, nk, chan_keys[i]) != 0) {
            JS_FreeValue(ctx, ch);
            return -1;
        }
        if (efx_api_read_channel_color(ctx, ch, chan_keys[i], outs[i]) != 0) {
            JS_FreeValue(ctx, ch);
            return -1;
        }
        if (efx_api_read_material_map(ctx, ch, chan_keys[i], mouts[i]) != 0) {
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
                    efx_api_type_error(ctx, "shininess must be a number");
                    return -1;
                }
                if (!isfinite(d) || d <= 0) {
                    JS_FreeValue(ctx, ch);
                    efx_api_range_error(ctx, "shininess must be Finite and > 0");
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
        int rc = efx_api_get_live_sample(ctx, am, &out->alpha_mask);
        JS_FreeValue(ctx, am);
        if (rc != 0) {
            return -1;
        }
    } else {
        JS_FreeValue(ctx, am);
    }
    return 0;
}


efxjs_meshdata *efx_api_get_live_meshdata(JSContext *ctx, JSValueConst v) {
    return live_opaque(ctx, v, meshdata_class_id, "expected a MeshData",
                       "using a destroyed resource", meshdata_alive);
}


efxjs_mesh *efx_api_get_live_mesh(JSContext *ctx, JSValueConst v) {
    return live_opaque(ctx, v, mesh_class_id, "expected a Mesh",
                       "using a destroyed resource", mesh_alive);
}
