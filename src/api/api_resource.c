#include "api/api_internal.h"


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


JSValue efx_js_loadText(JSContext *ctx, JSValueConst this_val, int argc,
                        JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) {
        return efx_api_type_error(ctx, "loadText requires a path string");
    }
    struct efx_host_state *h = efx_api_host_state(ctx);
    if (!h->resource) {
        return efx_api_plain_error(ctx, "no resource root");
    }
    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) {
        return JS_EXCEPTION;
    }
    int err = EFX_RESOURCE_OK;
    char *text = efx_resource_read_text(h->resource, path, &err);
    JS_FreeCString(ctx, path);
    if (!text) {
        return efx_api_plain_error(ctx, resource_err_text(err));
    }
    JSValue out = JS_NewString(ctx, text);
    efx_resource_free(text);
    return out;
}


JSValue efx_js_loadImage(JSContext *ctx, JSValueConst this_val, int argc,
                         JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) {
        return efx_api_type_error(ctx, "loadImage requires a path string");
    }
    struct efx_host_state *h = efx_api_host_state(ctx);
    if (!h->resource) {
        return efx_api_plain_error(ctx, "no resource root");
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
        return efx_api_plain_error(ctx, resource_err_text(err));
    }
    int ierr = EFX_IMAGE_OK;
    efx_image *img = efx_image_decode(bytes, size, &ierr);
    efx_resource_free(bytes);
    if (!img) {
        return efx_api_plain_error(ctx, ierr == EFX_IMAGE_ERR_NOMEM ? "out of memory"
                                                            : "image decode failed");
    }
    size_t n = (size_t)img->width * (size_t)img->height * 4u;
    uint8_t *px = malloc(n ? n : 1);
    if (!px) {
        efx_image_free(img);
        return efx_api_generic_error(ctx, "out of memory");
    }
    memcpy(px, img->pixels, n);
    int w = img->width;
    int hh = img->height;
    efx_image_free(img);
    efxjs_imagedata *d = calloc(1, sizeof(efxjs_imagedata));
    if (!d) {
        free(px);
        return efx_api_generic_error(ctx, "out of memory");
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
        return efx_api_type_error(ctx, "loadMeshData requires a path string");
    }
    struct efx_host_state *h = efx_api_host_state(ctx);
    if (!h->resource) {
        return efx_api_plain_error(ctx, "no resource root");
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
            return efx_api_type_error(ctx, "loadMeshData options must be an object");
        }
        static const char *known[] = {"mesh"};
        if (efx_api_check_known_fields(ctx, argv[1], known, 1, "loadMeshData") != 0) {
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
                    return efx_api_type_error(ctx,
                                      "mesh must be a non-negative integer or a name");
                }
                opts.mesh_index = (int)d;
            } else {
                JS_FreeValue(ctx, mv);
                JS_FreeCString(ctx, path);
                return efx_api_type_error(ctx,
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
        return efx_api_plain_error(ctx, gltf_err_text(err));
    }
    efxjs_meshdata *wrap = calloc(1, sizeof(efxjs_meshdata));
    if (!wrap) {
        efx_meshdata_destroy(md);
        return efx_api_generic_error(ctx, "out of memory");
    }
    wrap->md = md;
    wrap->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, meshdata_class_id);
    JS_SetOpaque(obj, wrap);
    return obj;
}
