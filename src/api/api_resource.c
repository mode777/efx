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


/* natives for the shared prelude validator (ADR 0049): return the wrapper
 * object, or the NEGATED engine error code for the prelude's message table */
JSValue efx_js_load_image_wire(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
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
        return JS_NewInt32(ctx, -err);
    }
    int ierr = EFX_IMAGE_OK;
    efx_image *img = efx_image_decode(bytes, size, &ierr);
    efx_resource_free(bytes);
    if (!img) {
        /* -100: decode failure (distinct from the resource error codes) */
        return JS_NewInt32(ctx, -100);
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


/* (path, hasMesh, isName, index, nameOrNull) */
JSValue efx_js_load_meshdata_wire(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 5) {
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
    if (JS_ToInt32(ctx, &opts.has_mesh, argv[1]) < 0 ||
        JS_ToInt32(ctx, &opts.is_name, argv[2]) < 0 ||
        JS_ToInt32(ctx, &opts.mesh_index, argv[3]) < 0) {
        JS_FreeCString(ctx, path);
        return JS_EXCEPTION;
    }
    if (!JS_IsNull(argv[4]) && !JS_IsUndefined(argv[4])) {
        name = (char *)JS_ToCString(ctx, argv[4]);
        if (!name) {
            JS_FreeCString(ctx, path);
            return JS_EXCEPTION;
        }
        opts.mesh_name = name;
    }
    int err = EFX_GLTF_OK;
    efx_meshdata *md = efx_gltf_load_meshdata(h->resource, path, &opts, &err);
    if (name) {
        JS_FreeCString(ctx, name);
    }
    JS_FreeCString(ctx, path);
    if (!md) {
        return JS_NewInt32(ctx, -err);
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
