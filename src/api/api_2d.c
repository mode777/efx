#include "api/api_internal.h"


/* -------------------------------------------------------- F2 bindings */

JSValue efx_js_whiteTexture(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    struct efx_host_state *h = efx_api_host_state(ctx);
    if (!h->has_white_texture) {
        uint64_t handle = efx_render_white_texture();
        if (!handle) {
            return efx_api_generic_error(ctx, "white texture unavailable");
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
        return efx_api_type_error(ctx, "setClearColor requires a [r,g,b,a] array");
    }
    float c[4];
    int rc = efx_api_get_float_array(ctx, argv[0], c, 4);
    if (rc != 0) {
        return JS_EXCEPTION;
    }
    efx_render_set_clear_color(c);
    return JS_UNDEFINED;
}


/* (frameW, frameH, x, y, zoom, rotation); x/y may be NaN — the engine
 * resolves them to the frame center at record time */
JSValue efx_js_set_camera2d_wire(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 6) {
        return efx_api_type_error(ctx, "camera2d wire native requires 6 arguments");
    }
    efx_camera2d cam;
    memset(&cam, 0, sizeof(cam));
    cam.zoom = 1.0f;
    cam.x = NAN;
    cam.y = NAN;
    double d[6];
    for (int i = 0; i < 6; i++) {
        if (JS_ToFloat64(ctx, &d[i], argv[i]) < 0) {
            return JS_EXCEPTION;
        }
    }
    cam.frame_w = (float)d[0];
    cam.frame_h = (float)d[1];
    cam.x = (float)d[2];
    cam.y = (float)d[3];
    cam.zoom = (float)d[4];
    cam.rotation = (float)d[5];
    efx_render_set_camera(&cam);
    return JS_UNDEFINED;
}


/* ---- natives for the shared prelude validators (ADR 0049) ---- */

JSValue efx_js_check_image_data(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    (void)this_val;
    (void)argc;
    if (!efx_api_get_live_imagedata(ctx, argv[0])) {
        return JS_EXCEPTION;
    }
    return JS_UNDEFINED;
}

/* (width, height, Uint8Array width*height*4) */
JSValue efx_js_create_image_data_wire(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) {
        return efx_api_type_error(ctx, "image data wire native requires (w, h, pixels)");
    }
    int32_t w = 0, hgt = 0;
    if (JS_ToInt32(ctx, &w, argv[0]) < 0 || JS_ToInt32(ctx, &hgt, argv[1]) < 0) {
        return JS_EXCEPTION;
    }
    size_t blen = 0;
    uint8_t *bytes = JS_GetUint8Array(ctx, &blen, argv[2]);
    if (!bytes) {
        return efx_api_type_error(ctx, "pixels must be an array or typed array");
    }
    size_t n = (size_t)w * (size_t)hgt * 4u;
    if (blen < n) {
        return efx_api_range_error(ctx, "pixels length must be width*height*4");
    }
    uint8_t *buf = malloc(n ? n : 1);
    if (!buf) {
        return efx_api_generic_error(ctx, "out of memory");
    }
    memcpy(buf, bytes, n);
    efxjs_imagedata *d = calloc(1, sizeof(efxjs_imagedata));
    if (!d) {
        free(buf);
        return efx_api_generic_error(ctx, "out of memory");
    }
    d->pixels = buf;
    d->w = w;
    d->h = hgt;
    d->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, imagedata_class_id);
    JS_SetOpaque(obj, d);
    return obj;
}


/* (imageData, wrap, filter, mipmaps) */
JSValue efx_js_create_texture_wire(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 4) {
        return efx_api_type_error(ctx, "texture wire native requires (image, wrap, filter, mipmaps)");
    }
    efxjs_imagedata *d = efx_api_get_live_imagedata(ctx, argv[0]);
    if (!d) {
        return JS_EXCEPTION;
    }
    int wrap = EFX_TEX_WRAP_REPEAT, filter = EFX_FILTER_LINEAR, mipmaps = 0;
    if (JS_ToInt32(ctx, &wrap, argv[1]) < 0 ||
        JS_ToInt32(ctx, &filter, argv[2]) < 0 ||
        JS_ToInt32(ctx, &mipmaps, argv[3]) < 0) {
        return JS_EXCEPTION;
    }
    uint64_t handle = efx_render_texture_create(d->w, d->h, d->pixels, wrap,
                                                filter, mipmaps);
    if (!handle) {
        return efx_api_generic_error(ctx, "texture upload failed (no GPU context?)");
    }
    efxjs_texture *t = calloc(1, sizeof(efxjs_texture));
    if (!t) {
        return efx_api_generic_error(ctx, "out of memory");
    }
    t->handle = handle;
    t->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, texture_class_id);
    JS_SetOpaque(obj, t);
    return obj;
}


typedef struct {
    float color[4];
    float rotation;
    float scale;
    float src[4];
    int has_src;
    float size[2];
    int has_size;
    float origin[2];
    int has_origin;
    int blend; /* EFX_BLEND_* override, or EFX_BLEND_INHERIT */
} quad_opts;

static void quad_opts_default(quad_opts *q) {
    memset(q, 0, sizeof(*q));
    q->color[0] = q->color[1] = q->color[2] = q->color[3] = 1.0f;
    q->scale = 1.0f;
    q->blend = EFX_BLEND_INHERIT;
}

static int check_quad_opts_fields(JSContext *ctx, JSValueConst opts) {
    static const char *known[] = {"color",  "rotation", "scale", "sourceRect",
                                  "size",   "origin",   "blend"};
    JSPropertyEnum *props = NULL;
    uint32_t nprops = 0;
    if (JS_GetOwnPropertyNames(ctx, &props, &nprops, opts,
                               JS_GPN_STRING_MASK) == 0) {
        for (uint32_t i = 0; i < nprops; i++) {
            const char *k = JS_AtomToCString(ctx, props[i].atom);
            int ok = 0;
            for (int j = 0; j < 7; j++) {
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
                JS_ThrowTypeError(ctx, "unknown drawQuad option");
                return -1;
            }
        }
        js_free(ctx, props);
    }
    return 0;
}

/* parse the drawQuad opts bag into `q` (first failing check wins) */
static int read_quad_opts(JSContext *ctx, uint64_t tex_handle,
                          JSValueConst opts, quad_opts *q) {
    quad_opts_default(q);
    if (check_quad_opts_fields(ctx, opts) != 0) {
        return -1;
    }

    JSValue cv = JS_GetPropertyStr(ctx, opts, "color");
    if (!JS_IsUndefined(cv)) {
        if (efx_api_get_float_array(ctx, cv, q->color, 4) != 0) {
            JS_FreeValue(ctx, cv);
            return -1;
        }
    }
    JS_FreeValue(ctx, cv);

    JSValue rv = JS_GetPropertyStr(ctx, opts, "rotation");
    if (!JS_IsUndefined(rv)) {
        double d;
        if (JS_ToFloat64(ctx, &d, rv) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, rv);
            efx_api_type_error(ctx, "rotation must be a finite number");
            return -1;
        }
        q->rotation = (float)d;
    }
    JS_FreeValue(ctx, rv);

    JSValue sv = JS_GetPropertyStr(ctx, opts, "scale");
    if (!JS_IsUndefined(sv)) {
        double d;
        if (JS_ToFloat64(ctx, &d, sv) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, sv);
            efx_api_type_error(ctx, "scale must be a finite number");
            return -1;
        }
        if (d <= 0) {
            JS_FreeValue(ctx, sv);
            efx_api_range_error(ctx, "scale must be > 0");
            return -1;
        }
        q->scale = (float)d;
    }
    JS_FreeValue(ctx, sv);

    JSValue zv = JS_GetPropertyStr(ctx, opts, "size");
    if (!JS_IsUndefined(zv)) {
        if (efx_api_get_float_array(ctx, zv, q->size, 2) != 0) {
            JS_FreeValue(ctx, zv);
            return -1;
        }
        if (q->size[0] <= 0 || q->size[1] <= 0) {
            JS_FreeValue(ctx, zv);
            efx_api_range_error(ctx, "size entries must be > 0");
            return -1;
        }
        q->has_size = 1;
    }
    JS_FreeValue(ctx, zv);

    JSValue ov = JS_GetPropertyStr(ctx, opts, "origin");
    if (!JS_IsUndefined(ov)) {
        if (efx_api_get_float_array(ctx, ov, q->origin, 2) != 0) {
            JS_FreeValue(ctx, ov);
            return -1;
        }
        q->has_origin = 1;
    }
    JS_FreeValue(ctx, ov);

    JSValue srcv = JS_GetPropertyStr(ctx, opts, "sourceRect");
    if (!JS_IsUndefined(srcv)) {
        if (efx_api_read_source_rect(ctx, tex_handle, srcv, q->src,
                                     &q->has_src) != 0) {
            return -1;
        }
    }

    JSValue bv = JS_GetPropertyStr(ctx, opts, "blend");
    if (!JS_IsUndefined(bv)) {
        if (efx_api_read_blend(ctx, bv, &q->blend) != 0) {
            JS_FreeValue(ctx, bv);
            return -1;
        }
    }
    JS_FreeValue(ctx, bv);
    return 0;
}

JSValue efx_js_drawQuad(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) {
        return efx_api_type_error(ctx, "drawQuad requires (texture, x, y, opts?)");
    }
    double x, y;
    if (JS_ToFloat64(ctx, &x, argv[1]) < 0 || JS_ToFloat64(ctx, &y, argv[2]) < 0) {
        return efx_api_type_error(ctx, "x and y must be numbers");
    }
    if (!isfinite(x) || !isfinite(y)) {
        return efx_api_range_error(ctx, "x and y must be finite");
    }
    /* F5a texture coercion: a live Texture or a live RenderTarget */
    uint64_t tex_handle = 0;
    if (efx_api_get_live_sample(ctx, argv[0], &tex_handle) != 0) {
        return JS_EXCEPTION;
    }

    quad_opts q;
    if (argc >= 4 && !JS_IsUndefined(argv[3])) {
        if (!JS_IsObject(argv[3])) {
            return efx_api_type_error(ctx, "opts must be an object");
        }
        if (read_quad_opts(ctx, tex_handle, argv[3], &q) != 0) {
            return JS_EXCEPTION;
        }
    } else {
        quad_opts_default(&q);
    }

    /* size derivation: explicit size -> sourceRect extent -> texture
     * pixels (a render target's extent plays the texture's role, F5a) */
    float w, h;
    if (q.has_size) {
        w = q.size[0];
        h = q.size[1];
    } else if (q.has_src) {
        w = q.src[2];
        h = q.src[3];
    } else {
        int tw = 0, th = 0;
        efx_render_sample_size(tex_handle, &tw, &th);
        w = (float)tw;
        h = (float)th;
    }
    float origin_x = q.has_origin ? q.origin[0] : w * 0.5f;
    float origin_y = q.has_origin ? q.origin[1] : h * 0.5f;

    int rc = efx_render_quad((float)x, (float)y, w, h,
                             tex_handle, q.color, q.rotation, q.scale, q.src,
                             q.has_src, origin_x, origin_y, q.blend);
    if (rc == EFX_RENDER_ERR_BUDGET) {
        return efx_api_range_error(ctx, "display list budget exceeded");
    }
    if (rc == EFX_RENDER_ERR_SINK) {
        return efx_api_generic_error(ctx, "no render surface (draw calls need a window)");
    }
    if (rc == EFX_RENDER_ERR_FEEDBACK) {
        return efx_api_type_error(ctx,
                          "cannot sample the render target being drawn into");
    }
    if (rc != EFX_RENDER_OK) {
        return efx_api_generic_error(ctx, "drawQuad failed");
    }
    return JS_UNDEFINED;
}


JSValue efx_js_setBlendMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "setBlendMode requires a mode string");
    }
    const char *s = JS_ToCString(ctx, argv[0]);
    if (!s) {
        return efx_api_type_error(ctx, "setBlendMode requires a mode string");
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
        return efx_api_type_error(ctx, "unknown blend mode");
    }
    JS_FreeCString(ctx, s);
    efx_render_set_blend(mode);
    return JS_UNDEFINED;
}
