#include "api/api.h"
#include "runtime/runtime_internal.h"
#include "render/render.h"
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

/* --------------------------------------------- F1 lifecycle hooks */

/* unsubscribe closure: magic selects the list (0 = update, 1 = render),
   func_data[0] carries the stable entry index (design D1/D2) */
static JSValue efx_js_unsubscribe(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic,
                                  JSValue *func_data) {
    (void)this_val;
    (void)argc;
    (void)argv;
    struct efx_host_state *h = host_state(ctx);
    struct efx_hook_list *list = magic ? &h->render_hooks : &h->update_hooks;
    int32_t idx = -1;
    JS_ToInt32(ctx, &idx, func_data[0]);
    if (idx >= 0 && idx < list->count) {
        list->entries[idx].active = 0; /* idempotent: repeated calls are no-ops */
    }
    return JS_UNDEFINED;
}

static JSValue register_hook(JSContext *ctx, JSValueConst fn, int is_render) {
    if (!JS_IsFunction(ctx, fn)) {
        return type_error(ctx, "hook must be a function");
    }
    struct efx_host_state *h = host_state(ctx);
    struct efx_hook_list *list = is_render ? &h->render_hooks : &h->update_hooks;
    int idx = efx_hooks_append(ctx, list, fn);
    if (idx < 0) {
        return generic_error(ctx, "out of memory");
    }
    JSValue data = JS_NewInt32(ctx, idx);
    JSValue unsub = JS_NewCFunctionData(ctx, efx_js_unsubscribe, 0,
                                        is_render ? 1 : 0, 1, &data);
    JS_FreeValue(ctx, data);
    return unsub;
}

JSValue efx_js_registerUpdateHook(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "registerUpdateHook requires a function");
    }
    return register_hook(ctx, argv[0], 0);
}

JSValue efx_js_registerRenderHook(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "registerRenderHook requires a function");
    }
    return register_hook(ctx, argv[0], 1);
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

typedef struct {
    uint64_t handle;
    int alive;
} efxjs_mesh;

typedef struct {
    uint64_t handle;
    int alive;
} efxjs_rendertarget;

static JSClassID texture_class_id;
static JSClassID imagedata_class_id;
static JSClassID meshdata_class_id;
static JSClassID mesh_class_id;
static JSClassID rendertarget_class_id;

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
    return type_error(ctx, "not a resource object");
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
        JS_NewClassID(rt, &rendertarget_class_id) != rendertarget_class_id) {
        return -1;
    }
    if (JS_NewClass(rt, texture_class_id, &texture_class_def) < 0 ||
        JS_NewClass(rt, imagedata_class_id, &imagedata_class_def) < 0 ||
        JS_NewClass(rt, meshdata_class_id, &meshdata_class_def) < 0 ||
        JS_NewClass(rt, mesh_class_id, &mesh_class_def) < 0 ||
        JS_NewClass(rt, rendertarget_class_id, &rendertarget_class_def) < 0) {
        return -1;
    }
    JSValue tex_proto = JS_NewObject(ctx);
    JSValue img_proto = JS_NewObject(ctx);
    JSValue md_proto = JS_NewObject(ctx);
    JSValue mesh_proto = JS_NewObject(ctx);
    JSValue rt_proto = JS_NewObject(ctx);
    JSValue m = JS_NewCFunction(ctx, js_destroy_resource, "destroy", 0);
    JS_SetPropertyStr(ctx, tex_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, img_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, md_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, mesh_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, rt_proto, "destroy", m);
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
    JS_SetClassProto(ctx, texture_class_id, tex_proto);
    JS_SetClassProto(ctx, imagedata_class_id, img_proto);
    JS_SetClassProto(ctx, meshdata_class_id, md_proto);
    JS_SetClassProto(ctx, mesh_class_id, mesh_proto);
    JS_SetClassProto(ctx, rendertarget_class_id, rt_proto);
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
