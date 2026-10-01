/* R22 spike-only timing harness (throwaway branch, never merged): measures
 * the quickjs cost of the prelude validation path against the native C path
 * for one cold API (createParticleSystem x1000) and one hot API (drawQuad
 * with a full option bag x10000). Prints the median of 5 runs per side. */
#include "render/render.h"
#include "runtime/runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void *mock_create(void *ud, int w, int h, const uint8_t *rgba,
                         int wrap, int filter, int mipmaps) {
    (void)ud; (void)rgba; (void)wrap; (void)filter; (void)mipmaps;
    return malloc((size_t)(w * h * 4 > 0 ? w * h * 4 : 1));
}

static void mock_destroy(void *ud, void *native) {
    (void)ud;
    free(native);
}

static const efx_render_sink g_sink = {
    NULL, mock_create, mock_destroy, NULL, NULL, NULL, NULL, NULL,
};

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

/* median of 5 runs; each run makes `n` calls with the given args, destroying
 * each result so memory stays flat */
static double bench(JSContext *ctx, JSValueConst fn, JSValueConst *args,
                    int nargs, int n) {
    double samples[5];
    for (int run = 0; run < 5; run++) {
        efx_render_end_frame();
        efx_render_begin_frame();
        double t0 = now_ms();
        for (int i = 0; i < n; i++) {
            JSValue r = JS_Call(ctx, fn, JS_UNDEFINED, nargs, args);
            if (JS_IsException(r)) {
                fprintf(stderr, "bench call failed\n");
                JS_GetException(ctx);
                exit(1);
            }
            if (JS_IsObject(r)) {
                JSValue d = JS_GetPropertyStr(ctx, r, "destroy");
                if (JS_IsFunction(ctx, d)) {
                    JSValue dr = JS_Call(ctx, d, r, 0, NULL);
                    if (JS_IsException(dr)) {
                        JS_GetException(ctx);
                    } else {
                        JS_FreeValue(ctx, dr);
                    }
                }
                JS_FreeValue(ctx, d);
            }
            JS_FreeValue(ctx, r);
        }
        samples[run] = now_ms() - t0;
    }
    qsort(samples, 5, sizeof(double), cmp_double);
    return samples[2];
}

static JSValue get_global_fn(JSContext *ctx, const char *name) {
    JSValue glob = JS_GetGlobalObject(ctx);
    JSValue efx = JS_GetPropertyStr(ctx, glob, "efx");
    JS_FreeValue(ctx, glob);
    JSValue fn = JS_GetPropertyStr(ctx, efx, name);
    JS_FreeValue(ctx, efx);
    if (!JS_IsFunction(ctx, fn)) {
        fprintf(stderr, "efx.%s is not a function (did the prelude install?)\n",
                name);
        exit(1);
    }
    return fn;
}

int main(void) {
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    efx_runtime *rt = efx_runtime_new(NULL, 0);
    if (!rt) {
        return 2;
    }
    JSContext *ctx = efx_runtime_context(rt);

    /* the full particles option bag and the drawQuad bag, as globals */
    const char *setup =
        "efx.__tex = efx.createTexture(efx.createImageData({ width: 4,"
        " height: 4, pixels: new Uint8Array(4 * 4 * 4).fill(255) }));"
        "efx.__bag = { texture: efx.__tex, max: 32, lifetime: [1, 2],"
        " emissionRate: 10, position: [0, 0, 0], direction: [0, 1, 0],"
        " speed: [1, 2], gravity: [0, -1, 0], spread: 0.4,"
        " radialAcceleration: [0.1, 1], tangentialAcceleration: 0.5,"
        " linearDamping: [0, 0.2], sizes: [1, 3], sizeVariation: 0.3,"
        " colors: [[1, 0, 0, 1], [1, 1, 0, 0]], rotation: [0, 90],"
        " spin: [-45, 45], spinVariation: 0.2, relativeRotation: true,"
        " facing: 'view', blend: 'additive', space: 'world',"
        " emissionShape: { shape: 'sphere', size: [1, 1, 1] },"
        " insertMode: 'random', speedScale: 1.5, emitterLifetime: 5 };"
        "efx.__quad = { color: [0.5, 0.25, 0.1, 0.8], rotation: 45,"
        " scale: 2, size: [10, 20], origin: [1, 2],"
        " sourceRect: { x: 0, y: 0, w: 2, h: 2 } };";
    if (efx_runtime_eval_string(rt, "setup", setup) != 0) {
        return 2;
    }
    JSValue glob = JS_GetGlobalObject(ctx);
    JSValue efx = JS_GetPropertyStr(ctx, glob, "efx");
    JSValue bag = JS_GetPropertyStr(ctx, efx, "__bag");
    JSValue quad = JS_GetPropertyStr(ctx, efx, "__quad");
    JSValue tex = JS_GetPropertyStr(ctx, efx, "__tex");
    JS_FreeValue(ctx, glob);

    printf("=== R22 spike timing (desktop quickjs, %s build) ===\n",
#ifdef NDEBUG
           "Release"
#else
           "Debug"
#endif
           );

    /* cold: createParticleSystem, prelude path vs today's C path */
    JSValue ps_js = get_global_fn(ctx, "createParticleSystem");
    JSValue ps_c = get_global_fn(ctx, "createParticleSystemC");
    double t_pre = bench(ctx, ps_js, &bag, 1, 1000);
    double t_c = bench(ctx, ps_c, &bag, 1, 1000);
    printf("cold createParticleSystem x1000: prelude %.1f ms, C %.1f ms"
           " (delta +%.1f ms, %.2fx)\n",
           t_pre, t_c, t_pre - t_c, t_pre / t_c);
    JSValue wire_only = get_global_fn(ctx, "__r22wireOnly");
    double t_wire = bench(ctx, wire_only, &bag, 1, 1000);
    printf("     validation+wire only x1000: %.1f ms (native unpack+create share"
           " of the C path: %.1f ms)\n", t_wire, t_c - 0.0);
    JS_FreeValue(ctx, wire_only);
    JS_FreeValue(ctx, ps_js);
    JS_FreeValue(ctx, ps_c);

    /* hot: drawQuad with a full option bag x10000 in one frame */
    JSValue quad_native = get_global_fn(ctx, "drawQuad");
    JSValue quad_js = get_global_fn(ctx, "drawQuadJS");
    JSValue qargs[4] = { JS_NewFloat64(ctx, 5), JS_NewFloat64(ctx, 6), tex,
                         quad };
    double h_native = bench(ctx, quad_native, qargs, 4, 10000);
    double h_js = bench(ctx, quad_js, qargs, 4, 10000);
    printf("hot drawQuad x10000: native %.1f ms, prelude %.1f ms"
           " (delta +%.1f ms, %.2fx)\n",
           h_native, h_js, h_js - h_native, h_js / h_native);
    JS_FreeValue(ctx, qargs[0]);
    JS_FreeValue(ctx, qargs[1]);
    JS_FreeValue(ctx, quad_native);
    JS_FreeValue(ctx, quad_js);

    JS_FreeValue(ctx, bag);
    JS_FreeValue(ctx, quad);
    JS_FreeValue(ctx, tex);
    JS_FreeValue(ctx, efx);
    efx_runtime_destroy(rt);
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}
