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


JSValue efx_js_setCamera2D(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return efx_api_type_error(ctx, "setCamera2D requires an options object");
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
        if (efx_api_get_float_array(ctx, frame, f, 2) != 0) {
            JS_FreeValue(ctx, frame);
            return JS_EXCEPTION;
        }
        if (!(f[0] > 0 && f[1] > 0)) {
            JS_FreeValue(ctx, frame);
            return efx_api_range_error(ctx, "frame must be positive");
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
                return efx_api_type_error(ctx, "camera fields must be finite numbers");
            }
            *targets[i] = (float)d;
        }
        JS_FreeValue(ctx, v);
    }
    if (!(cam.zoom > 0)) {
        return efx_api_range_error(ctx, "zoom must be > 0");
    }
    efx_render_set_camera(&cam);
    return JS_UNDEFINED;
}


/* validate width/height and compute the pixel byte count */
static int read_image_size(JSContext *ctx, JSValueConst opts, int32_t *out_w,
                           int32_t *out_h, size_t *out_n) {
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
        efx_api_range_error(ctx, "width and height must be positive");
        return -1;
    }
    double pw = (double)w * (double)hgt * 4.0;
    if (pw > (double)0x7fffffff) {
        efx_api_range_error(ctx, "image too large");
        return -1;
    }
    *out_w = w;
    *out_h = hgt;
    *out_n = (size_t)pw;
    return 0;
}

/* copy `n` pixel bytes from an array or typed array into a fresh buffer */
static int read_image_pixels(JSContext *ctx, JSValueConst opts, size_t n,
                             uint8_t **out_buf) {
    JSValue pixels = JS_GetPropertyStr(ctx, opts, "pixels");
    if (JS_IsUndefined(pixels)) {
        JS_FreeValue(ctx, pixels);
        efx_api_type_error(ctx, "createImageData requires pixels");
        return -1;
    }
    uint8_t *buf = malloc(n);
    if (!buf) {
        JS_FreeValue(ctx, pixels);
        efx_api_generic_error(ctx, "out of memory");
        return -1;
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
            efx_api_range_error(ctx, "pixel bytes must be integers 0..255");
        }
    } else {
        size_t blen = 0;
        uint8_t *ptr = JS_GetUint8Array(ctx, &blen, pixels);
        if (!ptr) {
            efx_api_type_error(ctx, "pixels must be an array or typed array");
            rc = -1;
        } else if (blen != n) {
            efx_api_range_error(ctx, "pixels length must be width*height*4");
            rc = -2;
        } else {
            memcpy(buf, ptr, n);
            rc = 0;
        }
    }
    JS_FreeValue(ctx, pixels);
    if (rc != 0) {
        free(buf);
        return -1;
    }
    *out_buf = buf;
    return 0;
}

/* format field: only 'rgba8' (the default) exists in F2 */
static int read_image_format(JSContext *ctx, JSValueConst opts) {
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
        efx_api_range_error(ctx, "unsupported image format (only 'rgba8')");
        return -1;
    }
    return 0;
}

/* unknown-field check (typo protection); the message omits a context word */
static int check_imagedata_fields(JSContext *ctx, JSValueConst opts) {
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
            return -1;
        }
    }
    return 0;
}

JSValue efx_js_createImageData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return efx_api_type_error(ctx, "createImageData requires an options object");
    }
    JSValueConst opts = argv[0];
    int32_t w = 0, hgt = 0;
    size_t n = 0;
    if (read_image_size(ctx, opts, &w, &hgt, &n) != 0) {
        return JS_EXCEPTION;
    }

    uint8_t *buf = NULL;
    if (read_image_pixels(ctx, opts, n, &buf) != 0) {
        return JS_EXCEPTION;
    }
    if (read_image_format(ctx, opts) != 0) {
        free(buf);
        return JS_EXCEPTION;
    }
    if (check_imagedata_fields(ctx, opts) != 0) {
        free(buf);
        return JS_EXCEPTION;
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


JSValue efx_js_createTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "createTexture requires an ImageData");
    }
    efxjs_imagedata *d = efx_api_get_live_imagedata(ctx, argv[0]);
    if (!d) {
        return JS_EXCEPTION;
    }
    int wrap = EFX_TEX_WRAP_REPEAT;
    int filter = EFX_FILTER_LINEAR;
    int mipmaps = 0;
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        if (!JS_IsObject(argv[1])) {
            return efx_api_type_error(ctx, "createTexture options must be an object");
        }
        static const char *known[] = {"wrap", "filter", "mipmaps"};
        if (efx_api_check_known_fields(ctx, argv[1], known, 3, "createTexture") != 0) {
            return JS_EXCEPTION;
        }
        JSValue wv = JS_GetPropertyStr(ctx, argv[1], "wrap");
        if (!JS_IsUndefined(wv)) {
            const char *s = JS_ToCString(ctx, wv);
            if (!s) {
                JS_FreeValue(ctx, wv);
                return efx_api_type_error(ctx, "wrap must be a string");
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
                return efx_api_type_error(ctx, "unknown wrap mode");
            }
            JS_FreeCString(ctx, s);
        }
        JS_FreeValue(ctx, wv);
        JSValue fv = JS_GetPropertyStr(ctx, argv[1], "filter");
        if (!JS_IsUndefined(fv)) {
            const char *s = JS_ToCString(ctx, fv);
            if (!s) {
                JS_FreeValue(ctx, fv);
                return efx_api_type_error(ctx, "filter must be a string");
            }
            if (strcmp(s, "nearest") == 0) {
                filter = EFX_FILTER_NEAREST;
            } else if (strcmp(s, "linear") == 0) {
                filter = EFX_FILTER_LINEAR;
            } else {
                JS_FreeCString(ctx, s);
                JS_FreeValue(ctx, fv);
                return efx_api_type_error(ctx, "unknown filter");
            }
            JS_FreeCString(ctx, s);
        }
        JS_FreeValue(ctx, fv);
        JSValue mv = JS_GetPropertyStr(ctx, argv[1], "mipmaps");
        if (!JS_IsUndefined(mv)) {
            if (!JS_IsBool(mv)) {
                JS_FreeValue(ctx, mv);
                return efx_api_type_error(ctx, "mipmaps must be a boolean");
            }
            mipmaps = JS_ToBool(ctx, mv) ? 1 : 0;
        }
        JS_FreeValue(ctx, mv);
    }
    uint64_t handle = efx_render_texture_create(d->w, d->h, d->pixels, wrap,
                                                filter, mipmaps);
    if (!handle) {
        return efx_api_generic_error(ctx, "texture upload failed (no GPU context?)");
    }
    efxjs_texture *t = calloc(1, sizeof(efxjs_texture));
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
} quad_opts;

static void quad_opts_default(quad_opts *q) {
    memset(q, 0, sizeof(*q));
    q->color[0] = q->color[1] = q->color[2] = q->color[3] = 1.0f;
    q->scale = 1.0f;
}

static int check_quad_opts_fields(JSContext *ctx, JSValueConst opts) {
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
    return 0;
}

JSValue efx_js_drawQuad(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) {
        return efx_api_type_error(ctx, "drawQuad requires (x, y, texture, opts?)");
    }
    double x, y;
    if (JS_ToFloat64(ctx, &x, argv[0]) < 0 || JS_ToFloat64(ctx, &y, argv[1]) < 0) {
        return efx_api_type_error(ctx, "x and y must be numbers");
    }
    if (!isfinite(x) || !isfinite(y)) {
        return efx_api_range_error(ctx, "x and y must be finite");
    }
    /* F5a texture coercion: a live Texture or a live RenderTarget */
    uint64_t tex_handle = 0;
    if (efx_api_get_live_sample(ctx, argv[2], &tex_handle) != 0) {
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
                             q.has_src, origin_x, origin_y);
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
