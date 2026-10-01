#include "api/api_internal.h"


/* -------------------------------------------------------- F5a bindings */

JSValue efx_js_createRenderTarget(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return efx_api_type_error(ctx, "createRenderTarget requires an options object");
    }
    JSValueConst opts = argv[0];
    static const char *known[] = {"width", "height"};
    if (efx_api_check_known_fields(ctx, opts, known, 2, "createRenderTarget") != 0) {
        return JS_EXCEPTION;
    }
    double w = 0, h = 0;
    static const char *keys[] = {"width", "height"};
    double *outs[] = {&w, &h};
    for (int i = 0; i < 2; i++) {
        JSValue v = JS_GetPropertyStr(ctx, opts, keys[i]);
        if (JS_IsUndefined(v)) {
            JS_FreeValue(ctx, v);
            return efx_api_type_error(ctx, "createRenderTarget requires width and height");
        }
        int bad = !JS_IsNumber(v) || JS_ToFloat64(ctx, outs[i], v) < 0;
        JS_FreeValue(ctx, v);
        if (bad) {
            return efx_api_type_error(ctx, "width and height must be numbers");
        }
        if (!isfinite(*outs[i]) || *outs[i] <= 0 ||
            *outs[i] != floor(*outs[i]) ||
            *outs[i] > (double)EFX_RENDER_MAX_TARGET_SIZE) {
            return efx_api_range_error(
                ctx, "width and height must be integers in 1..4096");
        }
    }
    uint64_t handle = efx_render_target_create((int)w, (int)h);
    if (!handle) {
        return efx_api_generic_error(ctx, "render target creation failed (no GPU context?)");
    }
    efxjs_rendertarget *t = calloc(1, sizeof(efxjs_rendertarget));
    if (!t) {
        efx_render_target_destroy(handle);
        return efx_api_generic_error(ctx, "out of memory");
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
        return efx_api_type_error(ctx, "beginRenderTarget requires a RenderTarget");
    }
    efxjs_rendertarget *t = efx_api_get_live_render_target(ctx, argv[0]);
    if (!t) {
        return JS_EXCEPTION;
    }
    int rc = efx_render_begin_target(t->handle);
    if (rc != EFX_RENDER_OK) {
        return efx_api_target_call_error(ctx, rc);
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
        return efx_api_target_call_error(ctx, rc);
    }
    return JS_UNDEFINED;
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
        efx_api_type_error(ctx, "post-effect entry must be an object");
        return -1;
    }
    JSValue ev = JS_GetPropertyStr(ctx, v, "effect");
    if (!JS_IsString(ev)) {
        JS_FreeValue(ctx, ev);
        efx_api_type_error(ctx, "post-effect entry requires an effect name");
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
        efx_api_type_error(ctx, "unknown post effect");
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
    if (efx_api_check_known_fields(ctx, v, keys, nkeys, "post effect") != 0) {
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
            int rc = efx_api_get_float_array(ctx, tv, out->u.color_filter.tint, 4);
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
        return efx_api_type_error(ctx, "setPostEffects requires an array or null");
    }
    if (JS_IsNull(argv[0]) || JS_IsUndefined(argv[0])) {
        efx_render_set_post_effects(NULL, 0);
        return JS_UNDEFINED;
    }
    if (!JS_IsArray(argv[0])) {
        return efx_api_type_error(ctx, "setPostEffects requires an array or null");
    }
    JSValue lenv = JS_GetPropertyStr(ctx, argv[0], "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    if (len < 0) {
        return efx_api_type_error(ctx, "setPostEffects requires an array or null");
    }
    if (len > EFX_POST_MAX_ENTRIES) {
        return efx_api_range_error(ctx, "post-effect chain is limited to 8 entries");
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
        return efx_api_type_error(ctx, "unknown post effect");
    }
    if (rc == EFX_POST_ERR_COUNT) {
        return efx_api_range_error(ctx, "post-effect chain is limited to 8 entries");
    }
    if (rc == EFX_POST_ERR_RANGE) {
        return efx_api_range_error(ctx, "post-effect option out of range");
    }
    if (rc != EFX_POST_OK) {
        return efx_api_generic_error(ctx, "setPostEffects failed");
    }
    return JS_UNDEFINED;
}


JSValue efx_js_setRenderScale(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "setRenderScale requires a scale number");
    }
    double scale = 0;
    if (!JS_IsNumber(argv[0]) || JS_ToFloat64(ctx, &scale, argv[0]) < 0) {
        return efx_api_type_error(ctx, "scale must be a number");
    }
    if (!isfinite(scale) || scale <= 0.0 || scale > 2.0) {
        return efx_api_range_error(ctx, "scale must be in (0, 2]");
    }
    int filter = EFX_FILTER_LINEAR;
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        if (!JS_IsObject(argv[1])) {
            return efx_api_type_error(ctx, "setRenderScale options must be an object");
        }
        static const char *known[] = {"filter"};
        if (efx_api_check_known_fields(ctx, argv[1], known, 1, "setRenderScale") != 0) {
            return JS_EXCEPTION;
        }
        JSValue fv = JS_GetPropertyStr(ctx, argv[1], "filter");
        if (!JS_IsUndefined(fv)) {
            const char *fs = JS_ToCString(ctx, fv);
            if (!fs) {
                JS_FreeValue(ctx, fv);
                return efx_api_type_error(ctx, "filter must be a string");
            }
            if (strcmp(fs, "nearest") == 0) {
                filter = EFX_FILTER_NEAREST;
            } else if (strcmp(fs, "linear") == 0) {
                filter = EFX_FILTER_LINEAR;
            } else {
                JS_FreeCString(ctx, fs);
                JS_FreeValue(ctx, fv);
                return efx_api_type_error(ctx, "unknown filter");
            }
            JS_FreeCString(ctx, fs);
        }
        JS_FreeValue(ctx, fv);
    }
    int rc = efx_render_set_render_scale((float)scale, filter);
    if (rc == EFX_POST_ERR_RANGE) {
        return efx_api_range_error(ctx, "scale must be in (0, 2]");
    }
    if (rc == EFX_POST_ERR_FILTER) {
        return efx_api_type_error(ctx, "unknown filter");
    }
    if (rc != EFX_POST_OK) {
        return efx_api_generic_error(ctx, "setRenderScale failed");
    }
    return JS_UNDEFINED;
}
