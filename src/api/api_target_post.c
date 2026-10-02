#include "api/api_internal.h"


/* -------------------------------------------------------- F5a bindings */

/* (width, height) — validated by the shared prelude (ADR 0049) */
JSValue efx_js_create_render_target_wire(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return efx_api_type_error(ctx, "render target wire native requires (w, h)");
    }
    int32_t w = 0, h = 0;
    if (JS_ToInt32(ctx, &w, argv[0]) < 0 || JS_ToInt32(ctx, &h, argv[1]) < 0) {
        return JS_EXCEPTION;
    }
    uint64_t handle = efx_render_target_create(w, h);
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




/* -------------------------------------------------------- F5b bindings */

/* the 9-float wire layout (prelude writer + src/web/bridge_target_post.c) */
#define EFX_POST_WIRE_STRIDE 9

/* native set from the prelude's normalized wire (ADR 0049): the chain was
 * validated and marshalled by the shared prelude validator */
JSValue efx_js_set_post_effects_wire(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return efx_api_type_error(ctx, "post wire native requires (wire, count)");
    }
    if (JS_IsNull(argv[0]) || JS_IsUndefined(argv[0])) {
        efx_render_set_post_effects(NULL, 0);
        return JS_UNDEFINED;
    }
    size_t blen = 0;
    uint8_t *bytes = NULL;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[0], NULL, NULL, NULL);
    if (JS_IsException(ab)) {
        return ab;
    }
    bytes = JS_GetArrayBuffer(ctx, &blen, ab);
    JS_FreeValue(ctx, ab);
    int32_t count = 0;
    if (JS_ToInt32(ctx, &count, argv[1]) < 0) {
        return JS_EXCEPTION;
    }
    if (count < 0 || count > EFX_POST_MAX_ENTRIES) {
        return efx_api_range_error(ctx, "post-effect chain is limited to 8 entries");
    }
    if (!bytes || blen < (size_t)count * EFX_POST_WIRE_STRIDE * sizeof(float)) {
        return efx_api_type_error(ctx, "post wire must be a Float32Array");
    }
    const float *w = (const float *)bytes;
    efx_post_entry entries[EFX_POST_MAX_ENTRIES];
    memset(entries, 0, sizeof(entries));
    for (int i = 0; i < count; i++) {
        const float *e = w + (size_t)i * EFX_POST_WIRE_STRIDE;
        entries[i].effect = (int)e[0];
        entries[i].mix = e[1];
        switch (entries[i].effect) {
        case EFX_POST_COLOR_FILTER:
            entries[i].u.color_filter.brightness = e[2];
            entries[i].u.color_filter.contrast = e[3];
            entries[i].u.color_filter.saturation = e[4];
            for (int k = 0; k < 4; k++) {
                entries[i].u.color_filter.tint[k] = e[5 + k];
            }
            break;
        case EFX_POST_BLUR:
            entries[i].u.blur.radius = e[2];
            break;
        case EFX_POST_BLOOM:
            entries[i].u.bloom.threshold = e[2];
            entries[i].u.bloom.strength = e[3];
            break;
        default:
            break;
        }
    }
    int rc = efx_render_set_post_effects(entries, (int)count);
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
