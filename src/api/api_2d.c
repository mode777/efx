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


JSValue efx_js_createImageData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return efx_api_type_error(ctx, "createImageData requires an options object");
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
        return efx_api_range_error(ctx, "width and height must be positive");
    }
    double pw = (double)w * (double)hgt * 4.0;
    if (pw > (double)0x7fffffff) {
        return efx_api_range_error(ctx, "image too large");
    }

    JSValue pixels = JS_GetPropertyStr(ctx, opts, "pixels");
    if (JS_IsUndefined(pixels)) {
        JS_FreeValue(ctx, pixels);
        return efx_api_type_error(ctx, "createImageData requires pixels");
    }
    size_t n = (size_t)pw;
    uint8_t *buf = malloc(n);
    if (!buf) {
        JS_FreeValue(ctx, pixels);
        return efx_api_generic_error(ctx, "out of memory");
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
        return efx_api_range_error(ctx, "unsupported image format (only 'rgba8')");
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
