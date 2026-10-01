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


static void efx_api_sink_float(void *ud, int32_t i, double d) {
    ((float *)ud)[i] = (float)d;
}


static void efx_api_sink_float_cap3(void *ud, int32_t i, double d) {
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


/* Per-class hooks, dispatched through CLASS_SPECS: `destroy` is what a script
 * destroy() does (idempotent), `release` what the GC finalizer does before the
 * wrapper is freed. They differ on purpose: ImageData frees its pixels only in
 * the finalizer, and Audio stops its voice only in destroy(). */

static void texture_release(void *p) {
    efxjs_texture *t = (efxjs_texture *)p;
    if (t->alive && !t->permanent) {
        efx_render_texture_destroy(t->handle);
    }
}

static JSValue texture_destroy(JSContext *ctx, void *p) {
    efxjs_texture *t = (efxjs_texture *)p;
    if (t->alive && t->permanent) {
        return efx_api_type_error(ctx, "cannot destroy an engine-owned texture");
    }
    texture_release(p);
    t->alive = 0;
    return JS_UNDEFINED;
}


static void imagedata_release(void *p) {
    free(((efxjs_imagedata *)p)->pixels);
}

static JSValue imagedata_destroy(JSContext *ctx, void *p) {
    (void)ctx;
    ((efxjs_imagedata *)p)->alive = 0;
    return JS_UNDEFINED;
}


static void meshdata_release(void *p) {
    efx_meshdata_destroy(((efxjs_meshdata *)p)->md);
}

static JSValue meshdata_destroy(JSContext *ctx, void *p) {
    efxjs_meshdata *d = (efxjs_meshdata *)p;
    (void)ctx;
    if (d->alive) {
        d->alive = 0;
        meshdata_release(p);
        d->md = NULL;
    }
    return JS_UNDEFINED;
}


static void mesh_release(void *p) {
    efxjs_mesh *m = (efxjs_mesh *)p;
    if (m->alive) {
        efx_render_mesh_destroy(m->handle);
    }
}

static JSValue mesh_destroy(JSContext *ctx, void *p) {
    (void)ctx;
    mesh_release(p);
    ((efxjs_mesh *)p)->alive = 0;
    return JS_UNDEFINED;
}


static void rendertarget_release(void *p) {
    efxjs_rendertarget *t = (efxjs_rendertarget *)p;
    if (t->alive) {
        efx_render_target_destroy(t->handle);
    }
}

static JSValue rendertarget_destroy(JSContext *ctx, void *p) {
    (void)ctx;
    rendertarget_release(p);
    ((efxjs_rendertarget *)p)->alive = 0;
    return JS_UNDEFINED;
}


static void fontdata_release(void *p) {
    efx_text_fontdata_destroy(((efxjs_fontdata *)p)->fd);
}

static JSValue fontdata_destroy(JSContext *ctx, void *p) {
    efxjs_fontdata *d = (efxjs_fontdata *)p;
    (void)ctx;
    if (d->alive) {
        d->alive = 0;
        fontdata_release(p);
        d->fd = NULL;
    }
    return JS_UNDEFINED;
}


static void font_release(void *p) {
    efx_text_font_destroy(((efxjs_font *)p)->font);
}

static JSValue font_destroy(JSContext *ctx, void *p) {
    efxjs_font *f = (efxjs_font *)p;
    (void)ctx;
    if (f->alive) {
        f->alive = 0;
        font_release(p);
        f->font = NULL;
    }
    return JS_UNDEFINED;
}


static void particlesystem_release(void *p) {
    efxjs_particlesystem *s = (efxjs_particlesystem *)p;
    if (s->alive) {
        efx_render_particles_destroy(s->handle);
    }
}

static JSValue particlesystem_destroy(JSContext *ctx, void *p) {
    (void)ctx;
    particlesystem_release(p);
    ((efxjs_particlesystem *)p)->alive = 0;
    return JS_UNDEFINED;
}


static void audiodata_release(void *p) {
    efxjs_audiodata *d = (efxjs_audiodata *)p;
    if (d->data) {
        efx_audio_data_release(d->data);
    }
}

static JSValue audiodata_destroy(JSContext *ctx, void *p) {
    efxjs_audiodata *d = (efxjs_audiodata *)p;
    (void)ctx;
    if (d->alive) {
        d->alive = 0;
        audiodata_release(p);
        d->data = NULL;
    }
    return JS_UNDEFINED;
}


static void audiostream_release(void *p) {
    efxjs_audiostream *s = (efxjs_audiostream *)p;
    if (s->stream) {
        efx_audio_stream_release(s->stream);
    }
}

static JSValue audiostream_destroy(JSContext *ctx, void *p) {
    efxjs_audiostream *s = (efxjs_audiostream *)p;
    (void)ctx;
    if (s->alive) {
        s->alive = 0;
        audiostream_release(p);
        s->stream = NULL;
    }
    return JS_UNDEFINED;
}


/* Audio has no release: dropping the handle never cuts off a
 * fire-and-forget sound */
static JSValue audio_destroy(JSContext *ctx, void *p) {
    efxjs_audio *a = (efxjs_audio *)p;
    (void)ctx;
    if (a->alive) {
        a->alive = 0;
        if (a->voice >= 0 && efx_audio_voice_serial(a->voice) == a->serial) {
            efx_audio_stop_voice(a->voice);
        }
        a->voice = -1;
    }
    return JS_UNDEFINED;
}


/* the CLASS_SPECS row for an object's class (with its wrapper in *out), or
 * NULL; never throws */
static const efx_class_spec *class_spec_of(JSValueConst v, void **out);

static void class_finalizer(JSRuntime *rt, JSValueConst val) {
    (void)rt;
    void *p = NULL;
    const efx_class_spec *s = class_spec_of(val, &p);
    if (s && p) {
        if (s->release) {
            s->release(p);
        }
        free(p);
    }
}

static JSValue js_destroy_resource(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    void *p = NULL;
    const efx_class_spec *s = class_spec_of(this_val, &p);
    if (!s || !p || !s->destroy) {
        return efx_api_type_error(ctx, "not a resource object");
    }
    return s->destroy(ctx, p);
}


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


/* live Mesh resolution for static-mesh colliders (defined with the F3
 * bindings); on failure it throws and returns NULL */
static efxjs_texture *get_live_texture(JSContext *ctx, JSValueConst v);

static efxjs_font *get_live_font(JSContext *ctx, JSValueConst v);


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
    { &texture_class_id, "Texture", texture_proto_funcs,
      EFX_ARRAY_COUNT(texture_proto_funcs), texture_destroy, texture_release },
    { &imagedata_class_id, "ImageData", imagedata_proto_funcs,
      EFX_ARRAY_COUNT(imagedata_proto_funcs), imagedata_destroy,
      imagedata_release },
    { &meshdata_class_id, "MeshData", meshdata_proto_funcs,
      EFX_ARRAY_COUNT(meshdata_proto_funcs), meshdata_destroy,
      meshdata_release },
    { &mesh_class_id, "Mesh", mesh_proto_funcs,
      EFX_ARRAY_COUNT(mesh_proto_funcs), mesh_destroy, mesh_release },
    { &rendertarget_class_id, "RenderTarget", rendertarget_proto_funcs,
      EFX_ARRAY_COUNT(rendertarget_proto_funcs), rendertarget_destroy,
      rendertarget_release },
    { &fontdata_class_id, "FontData", NULL, 0, fontdata_destroy,
      fontdata_release },
    { &font_class_id, "Font", font_proto_funcs,
      EFX_ARRAY_COUNT(font_proto_funcs), font_destroy, font_release },
    { &particlesystem_class_id, "ParticleSystem", particlesystem_proto_funcs,
      EFX_ARRAY_COUNT(particlesystem_proto_funcs), particlesystem_destroy,
      particlesystem_release },
    { &body_class_id, "Body", body_proto_funcs,
      EFX_ARRAY_COUNT(body_proto_funcs), NULL, efx_api_collider_release },
    { &character_class_id, "Character", character_proto_funcs,
      EFX_ARRAY_COUNT(character_proto_funcs), NULL, efx_api_collider_release },
    { &audiodata_class_id, "AudioData", NULL, 0, audiodata_destroy,
      audiodata_release },
    { &audiostream_class_id, "AudioStream", NULL, 0, audiostream_destroy,
      audiostream_release },
    { &audio_class_id, "Audio", audio_proto_funcs,
      EFX_ARRAY_COUNT(audio_proto_funcs), audio_destroy, NULL },
};


static const efx_class_spec *class_spec_of(JSValueConst v, void **out) {
    JSClassID id = 0;
    void *p = JS_GetAnyOpaque(v, &id); /* meaningful only for our classes */
    for (int i = 0; id && i < EFX_ARRAY_COUNT(CLASS_SPECS); i++) {
        if (*CLASS_SPECS[i].id == id) {
            *out = p;
            return &CLASS_SPECS[i];
        }
    }
    return NULL;
}


int efx_api_init(JSContext *ctx) {
    JSRuntime *rt = JS_GetRuntime(ctx);
    const int nclasses = EFX_ARRAY_COUNT(CLASS_SPECS);
    for (int i = 0; i < nclasses; i++) {
        if (JS_NewClassID(rt, CLASS_SPECS[i].id) != *CLASS_SPECS[i].id) {
            return -1;
        }
    }
    for (int i = 0; i < nclasses; i++) {
        JSClassDef def = { .class_name = CLASS_SPECS[i].name,
                           .finalizer = class_finalizer };
        if (JS_NewClass(rt, *CLASS_SPECS[i].id, &def) < 0) {
            return -1;
        }
    }
    /* Most classes share one destroy(); Body/Character define their own in
     * their function list. The shared function is dup'd per prototype so each
     * holds its own reference, then released once here. */
    JSValue destroy_fn = JS_NewCFunction(ctx, js_destroy_resource, "destroy", 0);
    for (int i = 0; i < nclasses; i++) {
        JSValue proto = JS_NewObject(ctx);
        if (CLASS_SPECS[i].destroy) {
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


/* parse a {x,y,w,h} sourceRect object, bounds-checked against the sampled
 * texture; consumes `srcv` and sets *has_src on success */
int efx_api_read_source_rect(JSContext *ctx, uint64_t tex, JSValueConst srcv,
                             float src[4], int *has_src) {
    if (!JS_IsObject(srcv)) {
        JS_FreeValue(ctx, srcv);
        efx_api_type_error(ctx, "sourceRect must be an object");
        return -1;
    }
    static const char *skeys[] = {"x", "y", "w", "h"};
    for (int i = 0; i < 4; i++) {
        JSValue f = JS_GetPropertyStr(ctx, srcv, skeys[i]);
        double d;
        if (JS_ToFloat64(ctx, &d, f) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, f);
            JS_FreeValue(ctx, srcv);
            efx_api_type_error(ctx, "sourceRect fields must be finite numbers");
            return -1;
        }
        JS_FreeValue(ctx, f);
        src[i] = (float)d;
    }
    JS_FreeValue(ctx, srcv);
    if (src[2] <= 0 || src[3] <= 0) {
        efx_api_range_error(ctx, "sourceRect extent must be > 0");
        return -1;
    }
    int tw = 0, th = 0;
    efx_render_sample_size(tex, &tw, &th);
    if (src[0] < 0 || src[1] < 0 ||
        src[0] + src[2] > (float)tw || src[1] + src[3] > (float)th) {
        efx_api_range_error(ctx, "sourceRect outside texture bounds");
        return -1;
    }
    *has_src = 1;
    return 0;
}


/* parse one Phong channel color: required 4-element array */
static int efx_api_read_channel_color(JSContext *ctx, JSValueConst channel,
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
static int efx_api_read_material_map(JSContext *ctx, JSValueConst ch, const char *name,
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
