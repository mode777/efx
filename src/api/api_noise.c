#include "api/api_internal.h"
#include "noise/efx_noise.h"

/* ================================================ efx.noise (ADR 0059)
 *
 * Marshal-only wire natives (ADR 0049): the shared prelude validates the
 * option bag and passes fully-validated scalars. Wire layouts:
 *   noise2(seed, type, frequency, octaves, lacunarity, gain, warp, x, y)
 *   noise3(seed, type, frequency, octaves, lacunarity, gain, warp, x, y, z)
 *   fill2(seed, type, frequency, octaves, lacunarity, gain, warp, buf, w, h)
 */

static efx_noise_desc noise_desc_from_wire(int32_t seed, int32_t type,
                                           double frequency, int32_t octaves,
                                           double lacunarity, double gain,
                                           int32_t warp) {
    efx_noise_desc d;
    d.seed = (uint32_t)seed;
    d.type = (efx_noise_type)type;
    d.frequency = (float)frequency;
    d.octaves = octaves;
    d.lacunarity = (float)lacunarity;
    d.gain = (float)gain;
    d.warp = (efx_noise_warp)warp;
    return d;
}

static int noise_wire_args(JSContext *ctx, int argc, JSValueConst *argv,
                           int vec_args, efx_noise_desc *out, double *vec) {
    if (argc < 7 + vec_args) {
        efx_api_type_error(ctx, "noise wire native requires validated scalars");
        return -1;
    }
    int32_t seed = 0, type = 0, octaves = 0, warp = 0;
    double frequency = 0, lacunarity = 0, gain = 0;
    if (JS_ToInt32(ctx, &seed, argv[0]) < 0 ||
        JS_ToInt32(ctx, &type, argv[1]) < 0 ||
        JS_ToFloat64(ctx, &frequency, argv[2]) < 0 ||
        JS_ToInt32(ctx, &octaves, argv[3]) < 0 ||
        JS_ToFloat64(ctx, &lacunarity, argv[4]) < 0 ||
        JS_ToFloat64(ctx, &gain, argv[5]) < 0 ||
        JS_ToInt32(ctx, &warp, argv[6]) < 0) {
        return -1;
    }
    for (int i = 0; i < vec_args; i++) {
        if (JS_ToFloat64(ctx, &vec[i], argv[7 + i]) < 0) {
            return -1;
        }
    }
    *out = noise_desc_from_wire(seed, type, frequency, octaves, lacunarity,
                                gain, warp);
    return 0;
}

JSValue efx_js_noise2_wire(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv) {
    (void)this_val;
    efx_noise_desc d;
    double vec[2];
    if (noise_wire_args(ctx, argc, argv, 2, &d, vec) < 0) {
        return JS_EXCEPTION;
    }
    return JS_NewFloat64(ctx, efx_noise2(&d, (float)vec[0], (float)vec[1]));
}

JSValue efx_js_noise3_wire(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv) {
    (void)this_val;
    efx_noise_desc d;
    double vec[3];
    if (noise_wire_args(ctx, argc, argv, 3, &d, vec) < 0) {
        return JS_EXCEPTION;
    }
    return JS_NewFloat64(ctx,
                         efx_noise3(&d, (float)vec[0], (float)vec[1], (float)vec[2]));
}

/* local copy of the typed-array float reader (api_3d.c keeps the same
 * pattern file-local) */
static float *noise_wire_f32(JSContext *ctx, JSValueConst v, size_t *out_len) {
    JSValue ab = JS_GetTypedArrayBuffer(ctx, v, NULL, NULL, NULL);
    if (JS_IsException(ab)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        return NULL;
    }
    size_t blen = 0;
    uint8_t *bytes = JS_GetArrayBuffer(ctx, &blen, ab);
    JS_FreeValue(ctx, ab);
    if (!bytes || blen % sizeof(float) != 0) {
        return NULL;
    }
    *out_len = blen / sizeof(float);
    return (float *)bytes;
}

JSValue efx_js_noise_fill2_wire(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    (void)this_val;
    efx_noise_desc d;
    double vec[1];
    if (noise_wire_args(ctx, argc, argv, 0, &d, vec) < 0) {
        return JS_EXCEPTION;
    }
    if (argc < 10) {
        return efx_api_type_error(ctx, "noise wire native requires validated scalars");
    }
    int32_t w = 0, h = 0;
    if (JS_ToInt32(ctx, &w, argv[8]) < 0 || JS_ToInt32(ctx, &h, argv[9]) < 0) {
        return JS_EXCEPTION;
    }
    size_t flen = 0;
    float *buf = noise_wire_f32(ctx, argv[7], &flen);
    if (!buf) {
        return efx_api_type_error(ctx,
                                  "fill2 buffer must be a Float32Array");
    }
    if (w < 1 || h < 1 || (int64_t)w * (int64_t)h > (int64_t)flen) {
        return efx_api_range_error(ctx,
                                   "fill2 buffer length must be w*h floats");
    }
    efx_noise_fill2(&d, buf, w, h);
    return JS_UNDEFINED;
}

int efx_api_register_noise(JSContext *ctx, JSValueConst efx) {
    /* every member is installed by the shared prelude (ADR 0049); the
     * binding only creates the namespace object (ADR 0050 rule) */
    JSValue noise = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, efx, "noise", noise);
    return 0;
}
