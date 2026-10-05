/*
 * Headless JS-API tests for the F2 2D layer: installs a mock GPU sink,
 * runs the real quickjs runtime + api bindings, and asserts semantics by
 * driving JS snippets and inspecting the display list from C.
 * Usage: efx_api_tests [<case>] ; exit 0 = pass.
 */
#include "render/render.h"
#include "resource/resource.h"
#include "runtime/runtime.h"
#include "runtime/runtime_internal.h"
#include "input/input.h"
#include "input/gamepad.h"

#include <stdlib.h>

#include "../test_support.h"

#ifndef EFX_RES_FIXTURES
#define EFX_RES_FIXTURES "tests/fixtures/resource"
#endif
#ifndef EFX_MOD_FIXTURES
#define EFX_MOD_FIXTURES "tests/fixtures/modules"
#endif
#ifndef EFX_MOD_ERROR_FIXTURES
#define EFX_MOD_ERROR_FIXTURES "tests/fixtures/modules_error"
#endif
#ifndef EFX_AUDIO_FIXTURES
#define EFX_AUDIO_FIXTURES "tests/fixtures/audio"
#endif

#define GLTF_SEQ_MAX 8
static int g_seq_wrap[GLTF_SEQ_MAX];
static int g_seq_filter[GLTF_SEQ_MAX];
static int g_seq_mipmaps[GLTF_SEQ_MAX];
static int g_seq_n;

static void *mock_create(void *ud, int w, int h, const uint8_t *rgba,
                         int wrap, int filter, int mipmaps) {
    (void)ud; (void)rgba;
    if (g_seq_n < GLTF_SEQ_MAX) {
        g_seq_wrap[g_seq_n] = wrap;
        g_seq_filter[g_seq_n] = filter;
        g_seq_mipmaps[g_seq_n] = mipmaps;
    }
    g_seq_n++;
    return malloc((size_t)(w * h * 4 > 0 ? w * h * 4 : 1));
}

static void mock_destroy(void *ud, void *native) {
    (void)ud;
    free(native);
}

static const efx_render_sink g_sink = {
    NULL, mock_create, mock_destroy, NULL, NULL, NULL, NULL, NULL,
};

static efx_runtime *g_rt;

static int run_js(const char *code) {
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    g_rt = efx_runtime_new(NULL, 0);
    if (!g_rt) {
        return 1;
    }
    int rc = efx_runtime_eval_string(g_rt, "test", code);
    if (rc == 0) {
        efx_runtime_collect(g_rt);
    }
    return rc;
}

static void end_js(void) {
    efx_runtime_destroy(g_rt);
    g_rt = NULL;
    efx_render_end_frame();
    efx_render_shutdown();
}

#define REQUIRE(c, msg) do { if (!(c)) { end_js(); return fail(msg); } } while (0)

/* JS assertion helper: t(fn, kind) requires fn() to throw a `kind` error */
#define T_HELPER                                                              \
    "function t(fn, kind) {"                                                  \
    "  try { fn(); throw new Error('did not throw'); }"                       \
    "  catch (e) {"                                                           \
    "    if (e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;" \
    "    if (!(e instanceof kind)) throw new Error('wrong kind: ' + e);"      \
    "  }"                                                                     \
    "}"

static int ok_js(const char *code) {
    int rc = run_js(code);
    if (rc != 0) {
        fprintf(stderr, "snippet raised unexpectedly: %.120s\n", code);
        return 1;
    }
    return 0;
}

static int err_js(const char *code, const char *what) {
    if (run_js(code) == 0) {
        fprintf(stderr, "snippet did not raise: %s\n", what);
        return 1;
    }
    return 0;
}

static int rec_count(void) {
    int n = 0;
    efx_render_records(&n);
    return n;
}

/* white texture: exists, stable identity, destroy() throws */
static int white(void) {
    REQUIRE(!ok_js("const a = efx.graphics.whiteTexture; const b = efx.graphics.whiteTexture; if (a !== b) throw new Error('identity');"
                   "try { a.destroy(); throw new Error('no'); } catch (e) { if (!(e instanceof TypeError)) throw e; }"),
            "white texture identity/destroy");
    end_js();
    return 0;
}

/* ADR 0052: without a rendering surface the white texture is still available
 * (CPU-only) and drawable; destroy() still throws */
static int white_sinkless(void) {
    efx_render_install_sink(NULL);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    efx_runtime *rt = efx_runtime_new(NULL, 0);
    if (!rt) {
        return fail("runtime");
    }
    const char *code =
        "const w = efx.graphics.whiteTexture;"
        "if (w.width !== 1 || w.height !== 1) throw new Error('white dims');"
        "try { w.destroy(); throw new Error('destroy did not throw'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }"
        "efx.graphics.drawQuad(w, 0, 0, { size: [4, 4] });";
    int rc = efx_runtime_eval_string(rt, "sinkless", code);
    if (rc == 0) {
        efx_runtime_collect(rt);
    }
    int n = rec_count();
    efx_runtime_destroy(rt);
    efx_render_end_frame();
    efx_render_shutdown();
    if (rc != 0) {
        return fail("white sinkless js");
    }
    if (n != 1) {
        return fail("white sinkless record count");
    }
    return 0;
}

/* end-to-end: JS camera + quad options land in the composed record */
static int quad_record(void) {
    const char *code =
        "efx.graphics.setCamera2D({ frame: [640, 480], x: 320, y: 240, zoom: 2, rotation: 0 });"
        "efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0,"
        "  { rotation: 90, scale: 1.5, color: [1, 0, 0, 1], size: [64, 32],"
        "    sourceRect: { x: 0, y: 0, w: 1, h: 1 } });";
    REQUIRE(!ok_js(code), "snippet");
    REQUIRE(rec_count() == 1, "record count");
    efx_camera2d cam = {640, 480, 320, 240, 2, 0};
    efx_affine expect = efx_affine_mul(efx_camera_matrix(&cam, 640, 480),
                                       efx_quad_matrix(0, 0, 32, 16, 90, 1.5f));
    const efx_record *r = efx_render_records(NULL);
    REQUIRE(feq(r[0].u.quad.m.a, expect.a) &&
            feq(r[0].u.quad.m.tx, expect.tx) && feq(r[0].u.quad.m.ty, expect.ty),
            "composed transform mismatch");
    REQUIRE(feq(r[0].u.quad.tw, 1) && feq(r[0].u.quad.sw, 1),
            "source rect/texture size");
    REQUIRE(feq(r[0].u.quad.w, 64) && feq(r[0].u.quad.h, 32),
            "explicit size overrides derivation");
    REQUIRE(feq(r[0].u.quad.color[0], 1) && feq(r[0].u.quad.color[1], 0) &&
            feq(r[0].u.quad.color[3], 1), "tint");
    REQUIRE(r[0].u.quad.blend == EFX_BLEND_ALPHA, "blend");
    end_js();
    return 0;
}

/* size derivation: explicit size -> sourceRect extent -> texture pixels;
 * scale applies after the size is determined */
static int size_derivation(void) {
    const char *code =
        "const img = efx.graphics.createImageData(64, 32, new Uint8Array(64 * 32 * 4));"
        "const tex = efx.graphics.createTexture(img);"
        "efx.graphics.drawQuad(tex, 0, 0);"                                                    /* texture pixels */
        "efx.graphics.drawQuad(tex, 0, 0, { sourceRect: { x: 0, y: 0, w: 8, h: 4 } });"        /* src extent */
        "efx.graphics.drawQuad(tex, 0, 0, { sourceRect: { x: 0, y: 0, w: 8, h: 4 }, size: [50, 20] });"
        "efx.graphics.drawQuad(tex, 0, 0, { size: [32, 16], scale: 2 });";                     /* scale after size */
    REQUIRE(!ok_js(code), "snippet");
    const efx_record *r = efx_render_records(NULL);
    REQUIRE(rec_count() == 4, "record count");
    REQUIRE(feq(r[0].u.quad.w, 64) && feq(r[0].u.quad.h, 32),
            "derive from texture pixels");
    REQUIRE(feq(r[1].u.quad.w, 8) && feq(r[1].u.quad.h, 4),
            "derive from sourceRect");
    REQUIRE(feq(r[2].u.quad.w, 50) && feq(r[2].u.quad.h, 20),
            "explicit size overrides sourceRect");
    /* scale 2 around the (default center) pivot: matrix a-component = 2 */
    REQUIRE(feq(r[3].u.quad.w, 32) && feq(r[3].u.quad.h, 16) &&
            feq(r[3].u.quad.m.a, 2), "scale applies after size");
    end_js();
    return 0;
}

/* origin: pivot point in quad-local pixels; placement unchanged without
 * rotation/scale; rotation around origin [0,0] fixes the top-left corner */
static int origin_pivot(void) {
    const char *code =
        "const img = efx.graphics.createImageData(64, 32, new Uint8Array(64 * 32 * 4));"
        "const tex = efx.graphics.createTexture(img);"
        "efx.graphics.drawQuad(tex, 10, 20);"
        "efx.graphics.drawQuad(tex, 10, 20, { origin: [50, 100] });"              /* no transform: same */
        "efx.graphics.drawQuad(tex, 10, 20, { origin: [0, 0], rotation: 90 });";  /* pivot at top-left */
    REQUIRE(!ok_js(code), "snippet");
    const efx_record *r = efx_render_records(NULL);
    REQUIRE(rec_count() == 3, "record count");
    /* untransformed: origin must not move the quad */
    REQUIRE(feq(r[0].u.quad.m.tx, r[1].u.quad.m.tx) &&
            feq(r[0].u.quad.m.ty, r[1].u.quad.m.ty) &&
            feq(r[0].u.quad.m.a, r[1].u.quad.m.a),
            "origin must not move an untransformed quad");
    /* origin [0,0] + rotation 90 (y-down, clockwise): local (0,0) maps to
     * (10, 20) and local (64, 0) maps to (10, 20 + 64) */
    REQUIRE(feq(r[2].u.quad.m.a + r[2].u.quad.m.c * 0 + r[2].u.quad.m.tx, 10) &&
            feq(r[2].u.quad.m.b * 0 + r[2].u.quad.m.d * 0 + r[2].u.quad.m.ty, 20),
            "origin pivot corner position");
    REQUIRE(feq(r[2].u.quad.m.a * 64 + r[2].u.quad.m.tx, 10) &&
            feq(r[2].u.quad.m.b * 64 + r[2].u.quad.m.ty, 84),
            "origin pivot rotation direction");
    end_js();
    return 0;
}

/* validation matrix for size/origin/zero-extent sourceRect */
static int quad_validation(void) {
    const char *code =
        T_HELPER
        "t(() => efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0, { size: [0, 10] }), RangeError);"
        "t(() => efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0, { size: [10] }), RangeError);"
        "t(() => efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0, { size: 'big' }), TypeError);"
        "t(() => efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0, { origin: [NaN, 0] }), RangeError);"
        "t(() => efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0, { origin: 'center' }), TypeError);"
        "t(() => efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0,"
        "  { sourceRect: { x: 0, y: 0, w: 0, h: 1 } }), RangeError);"
        "t(() => efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0, { size: [4, 4], frobnicate: 1 }), TypeError);";
    REQUIRE(!ok_js(code), "quad validation matrix");
    REQUIRE(rec_count() == 0, "failed calls must record nothing");
    end_js();
    return 0;
}

/* Texture width/height getters: values, whiteTexture, destroyed throws */
static int texture_size_getters(void) {
    const char *code =
        "const img = efx.graphics.createImageData(64, 32, new Uint8Array(64 * 32 * 4));"
        "const tex = efx.graphics.createTexture(img);"
        "if (tex.width !== 64 || tex.height !== 32) throw new Error('texture size');"
        "if (efx.graphics.whiteTexture.width !== 1 || efx.graphics.whiteTexture.height !== 1)"
        "  throw new Error('white texture size');"
        "tex.destroy();"
        "try { tex.width; throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }"
        "try { efx.graphics.drawQuad(tex, 0, 0); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }";
    REQUIRE(!ok_js(code), "texture size getters");
    end_js();
    return 0;
}

/* camera is snapshotted per record */
static int camera_snapshot(void) {
    const char *code =
        "efx.graphics.setCamera2D({ frame: [640, 480] });"
        "efx.graphics.drawQuad(efx.graphics.whiteTexture, 100, 0, { size: [8, 8] });"
        "efx.graphics.setCamera2D({ frame: [640, 480], x: 370, y: 0 });"
        "efx.graphics.drawQuad(efx.graphics.whiteTexture, 100, 0, { size: [8, 8] });";
    REQUIRE(!ok_js(code), "snippet");
    const efx_record *r = efx_render_records(NULL);
    REQUIRE(r[0].u.quad.m.tx != r[1].u.quad.m.tx, "camera not snapshotted");
    /* second view looks 50 world px right of the first: at zoom 1 the
       recorded quad shifts 50 frame px left (world moves right on screen) */
    REQUIRE(feq(r[1].u.quad.m.tx - r[0].u.quad.m.tx, -50.0f), "camera delta");
    end_js();
    return 0;
}

/* out-of-bounds sourceRect throws RangeError */
static int src_oob(void) {
    REQUIRE(!err_js("efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0,"
                    "  { sourceRect: { x: 0, y: 0, w: 5, h: 5 } });", "oob sourceRect"),
            "oob sourceRect must throw");
    end_js();
    return 0;
}

/* record budget surfaces as a thrown error from JS */
static int budget(void) {
    const char *code =
        "try {"
        "  for (let i = 0; i < 500000; i++) efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0);"
        "  throw new Error('budget not enforced');"
        "} catch (e) { if (!(e instanceof RangeError)) throw e; }";
    REQUIRE(!ok_js(code), "budget RangeError");
    end_js();
    return 0;
}

/* texture resource lifecycle at the JS level */
static int texture_lifecycle(void) {
    const char *code =
        "const img = efx.graphics.createImageData(2, 2, new Uint8Array(16));"
        "const tex = efx.graphics.createTexture(img);"
        "tex.destroy();"
        "tex.destroy();" /* idempotent */
        "try { efx.graphics.drawQuad(tex, 0, 0); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }";
    REQUIRE(!ok_js(code), "texture lifecycle");
    end_js();
    return 0;
}

/* blend snapshot at the JS level */
static int blend_snapshot(void) {
    const char *code =
        "efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0, { size: [4, 4] });"
        "efx.graphics.setBlendMode('subtractive');"
        "efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0, { size: [4, 4] });";
    REQUIRE(!ok_js(code), "snippet");
    const efx_record *r = efx_render_records(NULL);
    REQUIRE(r[0].u.quad.blend == EFX_BLEND_ALPHA &&
            r[1].u.quad.blend == EFX_BLEND_SUBTRACTIVE, "blend snapshot");
    end_js();
    return 0;
}

/* setClearColor stores through the JS binding */
static int clear_color_js(void) {
    REQUIRE(!ok_js("efx.graphics.setClearColor([0.1, 0.7, 0.3, 1]);"), "snippet");
    float c[4];
    efx_render_clear_color(c);
    REQUIRE(feq(c[0], 0.1f) && feq(c[1], 0.7f) && feq(c[2], 0.3f) &&
            feq(c[3], 1.0f), "clear color not stored");
    end_js();
    return 0;
}

/* default camera: frame == viewport, identity view */
static int default_camera(void) {
    REQUIRE(!ok_js("efx.graphics.drawQuad(efx.graphics.whiteTexture, 0, 0, { size: [4, 4] });"),
            "snippet");
    const efx_record *r = efx_render_records(NULL);
    REQUIRE(r[0].u.quad.frame_w == 1024 && r[0].u.quad.frame_h == 600 &&
            feq(r[0].u.quad.m.a, 1) && feq(r[0].u.quad.m.tx, 0) &&
            feq(r[0].u.quad.m.ty, 0), "default camera");
    end_js();
    return 0;
}

/* explicit lifecycle hooks: registration order, dt, unsubscribe, sugar */
static int hooks_registration(void) {
    const char *code =
        "globalThis.__hooksLog = [];"
        "try { efx.registerUpdateHook(123); __hooksLog.push('NO-THROW'); }"
        "catch (e) { __hooksLog.push('typeerror:' + (e instanceof TypeError)); }"
        "globalThis.__off = efx.registerUpdateHook(function (dt) {"
        "  __hooksLog.push('uA:' + (typeof dt === 'number' && isFinite(dt))); });"
        "efx.registerUpdateHook(function () { __hooksLog.push('uB'); });"
        "efx.registerRenderHook(function () { __hooksLog.push('r'); });"
        "globalThis.update = function (dt) {"
        "  __hooksLog.push('gU:' + (typeof dt === 'number' && isFinite(dt))); };"
        "globalThis.render = function () { __hooksLog.push('gR'); };";
    REQUIRE(!ok_js(code), "hooks snippet");
    int has_update = 0;
    int has_render = 0;
    efx_runtime_pick_hooks(g_rt, &has_update, &has_render);
    REQUIRE(has_update && has_render, "sugar hooks not picked up");
    REQUIRE(efx_runtime_call_hook(g_rt, 1, 0.5) == EFX_HOOK_OK &&
            efx_runtime_call_hook(g_rt, 0, 0.5) == EFX_HOOK_OK,
            "hook dispatch returned an error");
    /* unsubscribe is idempotent and removes the first hook */
    REQUIRE(efx_runtime_eval_string(g_rt, "unsub", "__off(); __off();") == 0,
            "unsubscribe snippet");
    REQUIRE(efx_runtime_call_hook(g_rt, 1, 0.25) == EFX_HOOK_OK,
            "post-unsubscribe dispatch");
    const char *want =
        "typeerror:true|uA:true|uB|gU:true|r|gR|uB|gU:true";
    char verify[512];
    snprintf(verify, sizeof(verify),
             "if (__hooksLog.join('|') !== '%s')"
             "  throw new Error('hook order: ' + __hooksLog.join('|'));",
             want);
    REQUIRE(efx_runtime_eval_string(g_rt, "verify", verify) == 0,
            "hook order/dt mismatch");
    end_js();
    return 0;
}


/* ------------------------------------------------------------------ F3 */

/* createMeshData: batch + shorthand, surfaceCount, validation matrix */
static int meshdata_js(void) {
    const char *code =
        "const P = [0,0,0, 1,0,0, 0,1,0];"
        "const md = efx.graphics.createMeshData(["
        "    { positions: P, normals: P, uvs: [0,0, 1,0, 0,1],"
        "      colors: [1,0,0,1, 0,1,0,1, 0,0,1,1], indices: [0,1,2] },"
        "    { positions: P },"
        "  ]);"
        "if (md.surfaceCount !== 2) throw new Error('surfaceCount');"
        "const one = efx.graphics.createMeshData([{ positions: P, indices: [0,1,2] }]);"
        "if (one.surfaceCount !== 1) throw new Error('shorthand');"
        T_HELPER
        "t(() => efx.graphics.createMeshData(), TypeError);"
        "t(() => efx.graphics.createMeshData({ positions: P }), TypeError);"
        "t(() => efx.graphics.createMeshData([]), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: [0,0,0] }]), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: [0,0,0, 1,0,1] }]), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P, indices: [0,1,3] }]), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P, indices: [0,1] }]), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P, frobnicate: 1 }]), TypeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P }], []), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: ['a',0,0, 1,0,0, 0,1,0] }]), TypeError);"
        "t(() => efx.graphics.createMeshData([{ positions: [NaN,0,0, 1,0,0, 0,1,0] }]), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P, normals: [0,0,1] }]), RangeError);"
        "one.destroy();"
        "try { one.surfaceCount; throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }";
    REQUIRE(!ok_js(code), "meshdata js");
    end_js();
    return 0;
}

/* 17 surfaces: RangeError cap */
static int meshdata_cap_js(void) {
    const char *code =
        "const P = [0,0,0, 1,0,0, 0,1,0];"
        "const S = [];"
        "for (let i = 0; i < 17; i++) S.push({ positions: P, indices: [0,1,2] });"
        "try { efx.graphics.createMeshData(S); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof RangeError)) throw e; }"
        "S.pop();"
        "if (efx.graphics.createMeshData(S).surfaceCount !== 16)"
        "  throw new Error('16 must be accepted');";
    REQUIRE(!ok_js(code), "meshdata cap");
    end_js();
    return 0;
}

/* createMesh + drawMesh: upload, whole-mesh record, validation */
static int mesh_js(void) {
    const char *code =
        "const P = [0,0,0, 1,0,0, 0,1,0];"
        "const md = efx.graphics.createMeshData([{ positions: P, indices: [0,1,2] }]);"
        "const mesh = efx.graphics.createMesh(md);"
        "if (mesh.surfaceCount !== 1) throw new Error('mesh surfaceCount');"
        "md.destroy();" /* Mesh is a copy */
        "if (mesh.surfaceCount !== 1) throw new Error('after source destroy');"
        "efx.graphics.setCamera3D([0, 2, 5], [0, 0, 0], 60);"
        "efx.graphics.drawMesh(mesh, { transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3,1],"
        "  color: [0.5, 0.25, 1, 1] });"
        T_HELPER
        "t(() => efx.graphics.drawMesh(), TypeError);"
        "t(() => efx.graphics.drawMesh({}), TypeError);"
        "t(() => efx.graphics.drawMesh(null), TypeError);"
        "t(() => efx.graphics.drawMesh(mesh, 5), TypeError);"
        "t(() => efx.graphics.drawMesh(mesh, { mesh }), TypeError);"
        "t(() => efx.graphics.drawMesh(mesh, { transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3] }), RangeError);"
        "t(() => efx.graphics.drawMesh(mesh, { transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3,'x',1] }), TypeError);"
        "t(() => efx.graphics.drawMesh(mesh, { color: [1, 0, 1] }), RangeError);"
        "t(() => efx.graphics.drawMesh(mesh, { frobnicate: 1 }), TypeError);"
        "const t2 = efx.graphics.createTexture("
        "  efx.graphics.createImageData(2, 2, new Uint8Array(16)));"
        "t2.destroy();"
        "efx.graphics.drawMesh(mesh);"
        "mesh.destroy(); mesh.destroy();" /* idempotent */
        "t(() => efx.graphics.drawMesh(mesh), TypeError);"
        "try { mesh.surfaceCount; throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }";
    REQUIRE(!ok_js(code), "mesh js");
    /* records: first drawMesh with explicit args, throws record nothing,
       second with defaults */
    const efx_record *r = efx_render_records(NULL);
    int n = rec_count();
    REQUIRE(n == 2, "mesh record count");
    REQUIRE(r[0].type == EFX_RECORD_MESH && r[1].type == EFX_RECORD_MESH,
            "mesh record type");
    REQUIRE(feq(r[0].u.mesh.transform[12], 1) &&
            feq(r[0].u.mesh.transform[13], 2) &&
            feq(r[0].u.mesh.transform[14], 3), "mesh transform");
    REQUIRE(feq(r[0].u.mesh.color[1], 0.25f), "mesh tint");
    float pos[3], target[3], fov, nearz, farz;
    efx_render_camera3d(pos, target, &fov, &nearz, &farz);
    REQUIRE(feq(r[0].u.mesh.camera.pos[2], 5) && feq(r[0].u.mesh.camera.fov, 60),
            "mesh camera snapshot");
    REQUIRE(feq(r[1].u.mesh.transform[0], 1) &&
            feq(r[1].u.mesh.transform[12], 0), "mesh default identity");
    REQUIRE(feq(r[1].u.mesh.color[3], 1), "mesh default tint");
    end_js();
    return 0;
}

/* setCamera3D: defaults, validation, separate from the 2D camera */
static int camera3d_js(void) {
    const char *code =
        "efx.graphics.setCamera3D([0, 1, 4], [0, 0, 0], 90);"
        T_HELPER
        "t(() => efx.graphics.setCamera3D(), TypeError);"
        "t(() => efx.graphics.setCamera3D(undefined, [0,0,0], 60), TypeError);"
        "t(() => efx.graphics.setCamera3D([0,0,0], [0,0,0], 'wide'), TypeError);"
        "t(() => efx.graphics.setCamera3D([0,0,0], [0,0,0], 60, { frobnicate: 1 }), TypeError);"
        "efx.graphics.setCamera3D([0, 0, 2], [0, 0, 0], 45);"
        /* defaults accepted for near/far */
        "efx.graphics.setCamera3D([0, 0, 2], [0, 0, 0], 45, { near: 0.5, far: 50 });";
    REQUIRE(!ok_js(code), "camera3d js");
    float pos[3], target[3], fov, nearz, farz;
    efx_render_camera3d(pos, target, &fov, &nearz, &farz);
    REQUIRE(feq(pos[2], 2) && feq(fov, 45), "camera3d state");
    REQUIRE(feq(nearz, 0.5f) && feq(farz, 50.0f), "camera3d near/far");
    end_js();
    return 0;
}

/* F4a: lights + per-surface materials through the real JS runtime */
static int f4a_js(void) {
    const char *code =
        "efx.graphics.setLight(0, { pos: [3,4,2], color: [1,0.95,0.9,1], range: 20 });"
        "efx.graphics.setDirectionalLight({ dir: [-0.5,-1,-0.3], color: [0.2,0.25,0.35,1] });"
        "const P=[0,0,0, 1,0,0, 0,1,0];"
        "const M={ ambient:{color:[0.05,0.05,0.05,1]},"
        "  diffuse:{color:[0.8,0.3,0.2,1]},"
        "  specular:{color:[1,1,1,1], shininess:64},"
        "  emissive:{color:[0,0,0,1]} };"
        "const md=efx.graphics.createMeshData("
        "  [{positions:P, indices:[0,1,2]}], [M]);"
        "const mesh=efx.graphics.createMesh(md);"
        "mesh.setSurfaceMaterial( 0, { diffuse:{color:[0.1,0.2,0.3,1]} });"
        "efx.graphics.setCamera3D([0,2,5], [0,0,0], 60);"
        "efx.graphics.drawMesh(mesh);"
        T_HELPER
        "t(()=>efx.graphics.setLight(4,{pos:[0,0,0],color:[1,1,1,1]}), RangeError);"
        "t(()=>efx.graphics.setLight(0,{color:[1,1,1,1]}), TypeError);"
        "t(()=>efx.graphics.setLight(0,{pos:[0,0,0],color:[1,1,1,1],range:-1}), RangeError);"
        "t(()=>efx.graphics.setDirectionalLight({dir:[0,0,0],color:[1,1,1,1]}), TypeError);"
        "t(()=>mesh.setSurfaceMaterial( 1, M), RangeError);"
        "t(()=>mesh.setSurfaceMaterial( 0, {diffuse:{color:[1,1,1,1],map:1}}), TypeError);"
        "t(()=>efx.graphics.createMeshData([{positions:P}], []), RangeError);"
        "t(()=>efx.graphics.createMeshData([{positions:P}], [{specular:{color:[1,1,1,1],shininess:0}}]), RangeError);"
        "mesh.destroy(); md.destroy();";
    REQUIRE(!ok_js(code), "f4a js");
    const efx_record *r = efx_render_records(NULL);
    int n = rec_count();
    REQUIRE(n == 1 && r[0].type == EFX_RECORD_MESH, "f4a record");
    REQUIRE(r[0].u.mesh.lights.points[0].enabled, "point light snapshot");
    REQUIRE(feq(r[0].u.mesh.lights.points[0].range, 20),
            "light range snapshot");
    REQUIRE(r[0].u.mesh.lights.directional.enabled, "directional snapshot");
    end_js();
    return 0;
}

/* F4b: per-channel maps + alphaMask parse, validation, and retention across
 * a destroyed-but-bound texture (desktop binding) */
static int f4b_js(void) {
    const char *code =
        "const img=efx.graphics.createImageData(1,1,"
        "  new Uint8Array([255,255,255,255]));"
        "const tex=efx.graphics.createTexture(img);"
        "const P=[0,0,0, 1,0,0, 0,1,0];"
        "const M={ diffuse:{color:[0.8,0.8,0.8,1], map:tex},"
        "  specular:{color:[1,1,1,1], shininess:32, map:tex},"
        "  alphaMask:tex };"
        "const md=efx.graphics.createMeshData(["
        "  {positions:P, uvs:[0,0, 1,0, 0,1], indices:[0,1,2]}],"
        "  [M]);"
        "const mesh=efx.graphics.createMesh(md);"
        "efx.graphics.setCamera3D([0,0,5], [0,0,0], 60);"
        "efx.graphics.drawMesh(mesh);"
        "tex.destroy();"                 /* retained by the bound map */
        "efx.graphics.drawMesh(mesh);"        /* still renders (no throw) */
        T_HELPER
        "t(()=>mesh.setSurfaceMaterial(0,{diffuse:{color:[1,1,1,1],map:tex}}), TypeError);"
        "t(()=>mesh.setSurfaceMaterial(0,{diffuse:{color:[1,1,1,1],map:1}}), TypeError);"
        "t(()=>mesh.setSurfaceMaterial(0,{alphaMask:5}), TypeError);"
        "t(()=>mesh.setSurfaceMaterial(0,{diffuse:{color:[1,1,1,1],frob:1}}), TypeError);"
        "mesh.setSurfaceMaterial(0,{diffuse:{color:[1,1,1,1]}});" /* release */
        "mesh.destroy(); md.destroy();";
    REQUIRE(!ok_js(code), "f4b js");
    REQUIRE(rec_count() >= 2, "f4b records");
    end_js();
    return 0;
}

/* F5a: render targets through the JS bindings — lifecycle, validation,
 * redirection errors, texture coercion in drawQuad and material maps */
static int f5a_js(void) {
    const char *code =
        T_HELPER
        /* validation matrix */
        "t(()=>efx.graphics.createRenderTarget(undefined,8), TypeError);"
        "t(()=>efx.graphics.createRenderTarget(0,8), RangeError);"
        "t(()=>efx.graphics.createRenderTarget(8,10.5), RangeError);"
        "t(()=>efx.graphics.createRenderTarget(8,4097), RangeError);"
        /* lifecycle + query properties */
        "const rt=efx.graphics.createRenderTarget(256,128);"
        "if(rt.width!==256||rt.height!==128) throw new Error('size');"
        "rt.destroy(); rt.destroy();"
        "t(()=>rt.width, TypeError);"
        /* coercion: an RT drives drawQuad size derivation + sourceRect */
        "const live=efx.graphics.createRenderTarget(64,32);"
        "efx.graphics.drawQuad(live,0,0);"
        "efx.graphics.drawQuad(live,0,0,{sourceRect:{x:0,y:0,w:16,h:16}});"
        "t(()=>efx.graphics.drawQuad(live,0,0,{sourceRect:{x:0,y:0,w:65,h:4}}), RangeError);"
        "t(()=>efx.graphics.drawQuad({},0,0), TypeError);"
        /* redirection: records land, nesting/balance throw */
        "efx.graphics.beginRenderTarget(live);"
        "efx.graphics.drawQuad(efx.graphics.whiteTexture,0,0);"
        "t(()=>efx.graphics.beginRenderTarget(live), TypeError);"
        "t(()=>efx.graphics.drawQuad(live,0,0), TypeError);" /* feedback */
        "efx.graphics.endRenderTarget();"
        "t(()=>efx.graphics.endRenderTarget(), TypeError);"
        "t(()=>efx.graphics.beginRenderTarget({}), TypeError);"
        /* material maps accept a live RT, reject a destroyed one */
        "const mesh=efx.graphics.createMesh(efx.graphics.createMeshData(["
        "  {positions:[0,0,0, 1,0,0, 0,1,0], uvs:[0,0, 1,0, 0,1], indices:[0,1,2]}]));"
        "efx.graphics.setCamera3D([0,0,5],[0,0,0],60);"
        "mesh.setSurfaceMaterial(0,{diffuse:{color:[1,1,1,1],map:live}});"
        "efx.graphics.drawMesh(mesh);"
        "efx.graphics.beginRenderTarget(live);"
        "t(()=>efx.graphics.drawMesh(mesh), TypeError);" /* mesh feedback */
        "efx.graphics.endRenderTarget();"
        "const dead=efx.graphics.createRenderTarget(8,8);"
        "dead.destroy();"
        "t(()=>mesh.setSurfaceMaterial(0,{diffuse:{color:[1,1,1,1],map:dead}}), TypeError);"
        "t(()=>efx.graphics.beginRenderTarget(dead), TypeError);"
        "mesh.destroy(); live.destroy();";
    REQUIRE(!ok_js(code), "f5a js");
    /* records: drawQuad(rt) x2, BEGIN, white quad, END, mesh, BEGIN, END */
    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    REQUIRE(count >= 6, "f5a record count");
    /* size derivation from the target extent (64x32) */
    REQUIRE(feq(recs[0].u.quad.w, 64) && feq(recs[0].u.quad.h, 32),
            "rt size derivation");
    REQUIRE(feq(recs[1].u.quad.w, 16) && feq(recs[1].u.quad.h, 16),
            "src extent derivation");
    /* each BEGIN record snapshots the frame's clear color (default black) */
    int begins = 0;
    for (int i = 0; i < count; i++) {
        if (recs[i].type == EFX_RECORD_BEGIN_TARGET) {
            REQUIRE(feq(recs[i].u.begin_target.clear[3], 1.0f),
                    "clear snapshot alpha");
            begins++;
        }
    }
    REQUIRE(begins == 2, "begin record count");
    end_js();
    return 0;
}

static int f5b_js(void) {
    const char *matrix =
        T_HELPER
        "t(()=>efx.graphics.setPostEffects('x'), TypeError);"
        "t(()=>efx.graphics.setPostEffects([1]), TypeError);"
        "t(()=>efx.graphics.setPostEffects([{effect:'vortex'}]), TypeError);"
        "t(()=>efx.graphics.setPostEffects([{effect:'blur',radius:0}]), RangeError);"
        "t(()=>efx.graphics.setPostEffects([{effect:'blur',radius:65}]), RangeError);"
        "t(()=>efx.graphics.setPostEffects([{effect:'blur',radius:'x'}]), TypeError);"
        "t(()=>efx.graphics.setPostEffects([{effect:'blur',frob:1}]), TypeError);"
        "t(()=>efx.graphics.setPostEffects([{effect:'bloom',strength:1.5}]), RangeError);"
        "t(()=>efx.graphics.setPostEffects([{effect:'colorFilter',tint:[1,1]}]), RangeError);"
        "t(()=>efx.graphics.setPostEffects([{effect:'blur',mix:2}]), RangeError);"
        "t(()=>efx.graphics.setPostEffects(new Array(9).fill({effect:'blur'})), RangeError);"
        "t(()=>efx.graphics.setRenderScale(0), RangeError);"
        "t(()=>efx.graphics.setRenderScale(2.5), RangeError);"
        "t(()=>efx.graphics.setRenderScale('x'), TypeError);"
        "t(()=>efx.graphics.setRenderScale(1,{filter:'bogus'}), TypeError);"
        "t(()=>efx.graphics.setRenderScale(1,{frob:1}), TypeError);"
        /* atomicity: a failed set leaves the previous chain */
        "efx.graphics.setPostEffects([{effect:'blur',radius:5,mix:0.25}]);"
        "t(()=>efx.graphics.setPostEffects([{effect:'nope'}]), TypeError);"
        "t(()=>efx.graphics.setPostEffects([{effect:'blur',radius:0}]), RangeError);"
        /* snapshot: later mutation of the entry must not change the chain */
        "const entry={effect:'blur',radius:3,mix:0.5};"
        "efx.graphics.setPostEffects([entry]);"
        "entry.radius=60; entry.mix=0.1; entry.effect='bloom';";
    REQUIRE(!ok_js(matrix), "f5b js matrix");
    efx_post_entry got[EFX_POST_MAX_ENTRIES];
    int n = 0;
    efx_render_post_effects(got, &n);
    if (n != 1 || got[0].effect != EFX_POST_BLUR)
        return fail("chain atomicity");
    if (!feq(got[0].u.blur.radius, 3.0f) || !feq(got[0].mix, 0.5f))
        return fail("chain snapshot");
    end_js();

    /* defaults are neutral and the chain persists across frames */
    REQUIRE(!ok_js("efx.graphics.setPostEffects([{effect:'colorFilter'}]);"),
            "f5b js defaults");
    efx_render_post_effects(got, &n);
    if (n != 1 || got[0].effect != EFX_POST_COLOR_FILTER)
        return fail("colorFilter stored");
    if (!feq(got[0].u.color_filter.brightness, 1.0f) ||
        !feq(got[0].u.color_filter.contrast, 1.0f) ||
        !feq(got[0].u.color_filter.saturation, 1.0f) ||
        !feq(got[0].u.color_filter.tint[0], 1.0f) ||
        !feq(got[0].u.color_filter.tint[3], 1.0f) || !feq(got[0].mix, 1.0f))
        return fail("colorFilter defaults");
    efx_render_begin_frame(); /* next frame: plain engine state persists */
    efx_render_post_effects(got, &n);
    if (n != 1) return fail("chain did not persist across frames");
    efx_render_end_frame();
    end_js();

    /* null and [] both clear */
    REQUIRE(!ok_js("efx.graphics.setPostEffects([{effect:'bloom'}]);efx.graphics.setPostEffects(null);"),
            "f5b js null clear");
    efx_render_post_effects(got, &n);
    if (n != 0) return fail("null did not clear");
    end_js();
    REQUIRE(!ok_js("efx.graphics.setPostEffects([{effect:'bloom'}]);efx.graphics.setPostEffects([]);"),
            "f5b js empty clear");
    efx_render_post_effects(got, &n);
    if (n != 0) return fail("[] did not clear");
    end_js();

    /* render scale persists and a rejected call leaves it in effect */
    REQUIRE(!ok_js("efx.graphics.setRenderScale(0.5,{filter:'nearest'});"
                   "try{efx.graphics.setRenderScale(0);}catch(e){}"), "f5b js scale");
    float sc = 0;
    int f = -1;
    efx_render_render_scale(&sc, &f);
    if (!feq(sc, 0.5f) || f != EFX_FILTER_NEAREST)
        return fail("scale state after failed call");
    end_js();
    return 0;
}

/* F6a: resource root on the runtime + loadText/loadImage bindings */
static int resource_js(void) {
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    g_rt = efx_runtime_new(NULL, 0);
    if (!g_rt) return fail("runtime");
    int err = EFX_RESOURCE_OK;
    efx_resource *res = efx_resource_open(EFX_RES_FIXTURES, &err);
    REQUIRE(res, "open fixtures");
    efx_runtime_set_resource(g_rt, res);
    int rc = efx_runtime_eval_string(g_rt, "test",
        "if (efx.io.loadText('hello.txt') !== 'hello efx\\n') throw new Error('text');"
        "var img = efx.graphics.loadImage('test_rgba.png');"
        "if (img.width !== 3 || img.height !== 2) throw new Error('dims');"
        "var px = efx.graphics.createTexture(img);"
        "if (px.width !== 3 || px.height !== 2) throw new Error('tex dims');"
        "var lt = efx.graphics.createTexture(efx.graphics.loadImage('test_rgba.png'));"
        "if (lt.width !== 3 || lt.height !== 2) throw new Error('composed tex dims');"
        "if (typeof efx.io.loadTexture !== 'undefined') throw new Error('loadTexture still present');"
        "img.destroy(); px.destroy(); lt.destroy();"
        "var e1 = 0; try { efx.io.loadText('nope.txt'); } catch (e) {"
        "  e1 = (e instanceof Error) ? 1 : 2; }"
        "if (e1 !== 1) throw new Error('missing not Error ('+e1+')');"
        "var e2 = 0; try { efx.io.loadText(5); } catch (e) {"
        "  e2 = (e instanceof TypeError) ? 1 : 2; }"
        "if (e2 !== 1) throw new Error('nonstring not TypeError ('+e2+')');");
    efx_runtime_destroy(g_rt);
    g_rt = NULL;
    efx_render_end_frame();
    efx_render_shutdown();
    efx_resource_close(res);
    if (rc != 0) return fail("resource js snippet raised");
    return 0;
}

/* F14: audio namespace — load/play/handles/music/validation through the
   real binding + shared prelude sugar (headless core is device-free) */
static int audio_js(void) {
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    g_rt = efx_runtime_new(NULL, 0);
    if (!g_rt) return fail("runtime");
    int err = EFX_RESOURCE_OK;
    efx_resource *res = efx_resource_open(EFX_AUDIO_FIXTURES, &err);
    REQUIRE(res, "open audio fixtures");
    efx_runtime_set_resource(g_rt, res);
    int rc = efx_runtime_eval_string(g_rt, "test",
        "var data = efx.audio.loadAudioData('tone.wav');"
        "if (!data || typeof data.destroy !== 'function') throw new Error('loadAudioData');"
        "var a = efx.audio.playAudio(data, { volume: 0.5, pan: -1 });"
        "if (!a) throw new Error('playAudio null');"
        "if (a.playing !== true) throw new Error('audio not playing');"
        "if (a.paused !== false) throw new Error('paused default');"
        "if (Math.abs(a.volume - 0.5) > 1e-6) throw new Error('volume getter');"
        "a.volume = 0.2; if (Math.abs(a.volume - 0.2) > 1e-6) throw new Error('volume setter');"
        "a.pan = 0.5; a.pitch = 1.5; a.loop = true;"
        "a.pause(); if (a.paused !== true || a.playing !== false) throw new Error('pause');"
        "a.resume(); if (a.paused !== false || a.playing !== true) throw new Error('resume');"
        "a.stop(); if (a.playing !== false) throw new Error('stop');"
        "a.destroy(); a.destroy();"
        "var stream = efx.audio.loadAudioStream('tone.mp3');"
        "if (!stream || typeof stream.destroy !== 'function') throw new Error('loadAudioStream');"
        "var b = efx.audio.playAudio(stream, { loop: true, volume: 0.3 });"
        "if (!b || b.playing !== true) throw new Error('stream play');"
        "b.pause(); b.resume(); b.stop(); b.destroy(); stream.destroy();"
        "if (typeof efx.audio.volume !== 'number') throw new Error('master getter');"
        "efx.audio.volume = 0.5; if (Math.abs(efx.audio.volume - 0.5) > 1e-6) throw new Error('master setter');"
        "var t1 = 0; try { efx.audio.loadAudioData(5); } catch (e) { t1 = (e instanceof TypeError) ? 1 : 2; }"
        "if (t1 !== 1) throw new Error('path type ('+t1+')');"
        "var t2 = 0; try { efx.audio.playAudio(data, { bogus: 1 }); } catch (e) { t2 = (e instanceof TypeError) ? 1 : 2; }"
        "if (t2 !== 1) throw new Error('unknown option ('+t2+')');"
        "var t3 = 0; try { efx.audio.loadAudioData('nope.wav'); } catch (e) { t3 = (e instanceof Error) ? 1 : 2; }"
        "if (t3 !== 1) throw new Error('missing audio ('+t3+')');"
        "var t4 = 0; try { efx.audio.playAudio({}); } catch (e) { t4 = (e instanceof TypeError) ? 1 : 2; }"
        "if (t4 !== 1) throw new Error('bad source ('+t4+')');"
        "data.destroy();"
        "var t5 = 0; try { efx.audio.playAudio(data); } catch (e) { t5 = (e instanceof Error) ? 1 : 2; }"
        "if (t5 !== 1) throw new Error('destroyed AudioData ('+t5+')');"
        "var t6 = 0; try { efx.audio.volume = -1; } catch (e) { t6 = (e instanceof RangeError) ? 1 : 2; }"
        "if (t6 !== 1) throw new Error('negative master ('+t6+')');");
    efx_runtime_destroy(g_rt);
    g_rt = NULL;
    efx_render_end_frame();
    efx_render_shutdown();
    efx_resource_close(res);
    if (rc != 0) return fail("audio js snippet raised");
    return 0;
}

/* F6b/F6e: createTexture sampler + mipmap options reach the native create */
static int createTexture_js(void) {
    const char *code =
        "var img = efx.graphics.createImageData(1, 1,"
        "  new Uint8Array([1, 2, 3, 4]));"
        "efx.graphics.createTexture(img);"
        "efx.graphics.createTexture(img, { wrap: 'clamp', filter: 'nearest' });"
        "efx.graphics.createTexture(img, { wrap: 'mirror', filter: 'nearest' });"
        "efx.graphics.createTexture(img, { mipmaps: true });"
        "efx.graphics.createTexture(img, { mipmaps: false, filter: 'nearest' });"
        "function boom(fn) { try { fn(); } catch (e) {"
        "  return (e instanceof TypeError) ? 1 : 2; } return 0; }"
        "if (boom(function () { efx.graphics.createTexture(img, { wrap: 'bogus' }); }) !== 1)"
        "  throw new Error('bad wrap');"
        "if (boom(function () { efx.graphics.createTexture(img, { filter: 'bogus' }); }) !== 1)"
        "  throw new Error('bad filter');"
        "if (boom(function () { efx.graphics.createTexture(img, { nope: 1 }); }) !== 1)"
        "  throw new Error('unknown field');"
        "if (boom(function () { efx.graphics.createTexture(img, { mipmaps: 'yes' }); }) !== 1)"
        "  throw new Error('non-boolean mipmaps');";
    g_seq_n = 0;
    REQUIRE(!ok_js(code), "createTexture options snippet");
    int ok = g_seq_n == 5 &&
             g_seq_wrap[0] == EFX_TEX_WRAP_REPEAT &&
             g_seq_filter[0] == EFX_FILTER_LINEAR &&
             g_seq_mipmaps[0] == 0 &&
             g_seq_wrap[1] == EFX_TEX_WRAP_CLAMP &&
             g_seq_filter[1] == EFX_FILTER_NEAREST &&
             g_seq_wrap[2] == EFX_TEX_WRAP_MIRROR &&
             g_seq_filter[2] == EFX_FILTER_NEAREST &&
             g_seq_mipmaps[3] == 1 &&
             g_seq_filter[3] == EFX_FILTER_LINEAR &&
             g_seq_mipmaps[4] == 0 &&
             g_seq_filter[4] == EFX_FILTER_NEAREST;
    end_js();
    return ok ? 0 : fail("createTexture sampler/mipmap option mapping");
}

/* F8a: font data -> baked font -> measure/draw + option validation */
static int font_js(void) {
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    g_rt = efx_runtime_new(NULL, 0);
    if (!g_rt) return fail("runtime");
    int err = EFX_RESOURCE_OK;
    efx_resource *res = efx_resource_open(EFX_RES_FIXTURES, &err);
    REQUIRE(res, "open fixtures");
    efx_runtime_set_resource(g_rt, res);
    int rc = efx_runtime_eval_string(g_rt, "test",
        "var fd = efx.graphics.loadFontData('font.ttf');"
        "var font = efx.graphics.createFont(fd, 32);"
        "if (font.size !== 32) throw new Error('size');"
        "if (!(font.lineHeight > 0)) throw new Error('lineHeight');"
        "if (!(font.ascent > 0)) throw new Error('ascent');"
        "if (!(font.descent < 0)) throw new Error('descent');"
        "var b = font.measure('hello world');"
        "if (!(b.width > 0) || b.lines !== 1) throw new Error('measure');"
        "var bw = font.measure('hello world', { width: 40 });"
        "if (bw.lines < 2) throw new Error('wrap lines');"
        "var bd = efx.graphics.drawText('AB', font, 10, 10, { color: [1, 0, 0, 1] });"
        "if (bd.lines !== 1) throw new Error('draw bounds');"
        "var fx = efx.graphics.createFont(fd, 24, { outline: { width: 2 },"
        "  shadow: { blur: 2, offset: [2, 2] } });"
        "efx.graphics.drawText('Hi', fx, 0, 0, { align: 'center',"
        "  outlineColor: [0, 0, 0, 1], shadowColor: [0, 0, 0, 1] });"
        "function boom(fn) { try { fn(); } catch (e) {"
        "  return e && e.constructor ? e.constructor.name : 'Error'; }"
        "  return 'none'; }"
        "if (boom(function () { efx.graphics.createFont(fd); }) !== 'TypeError')"
        "  throw new Error('missing size');"
        "if (boom(function () { efx.graphics.createFont(fd, 0); }) !== 'RangeError')"
        "  throw new Error('size 0');"
        "if (boom(function () { efx.graphics.createFont(fd, 16, { nope: 1 }); })"
        "    !== 'TypeError') throw new Error('unknown option');"
        "if (boom(function () { efx.graphics.createFont(fd, 16,"
        "    { outline: { width: 0 } }); }) !== 'RangeError')"
        "  throw new Error('outline width');"
        "if (boom(function () { efx.graphics.drawText('x', font, 0, 0,"
        "    { align: 'justify' }); }) !== 'TypeError')"
        "  throw new Error('justify without width');"
        "if (boom(function () { font.measure('x',"
        "    { align: 'bogus' }); }) !== 'TypeError')"
        "  throw new Error('bad align');"
        "if (boom(function () { efx.graphics.drawText('x', {}, 0, 0); }) !== 'TypeError')"
        "  throw new Error('non-font');"
        "font.destroy(); fx.destroy(); fd.destroy();"
        "if (boom(function () { font.size; }) !== 'TypeError')"
        "  throw new Error('destroyed getter');");
    efx_runtime_destroy(g_rt);
    g_rt = NULL;
    efx_render_end_frame();
    efx_render_shutdown();
    efx_resource_close(res);
    if (rc != 0) return fail("font js snippet raised");
    return 0;
}

/* F6b: loadMeshData imports a fixture and wires createMesh */
static int gltf_js(void) {
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    g_rt = efx_runtime_new(NULL, 0);
    if (!g_rt) return fail("runtime");
    int err = EFX_RESOURCE_OK;
    efx_resource *res = efx_resource_open(EFX_RES_FIXTURES "/gltf", &err);
    REQUIRE(res, "open gltf fixtures");
    efx_runtime_set_resource(g_rt, res);
    int rc = efx_runtime_eval_string(g_rt, "test",
        "var md = efx.graphics.loadMeshData('triangle.gltf');"
        "if (!(md instanceof Object) || md.surfaceCount !== 1) throw new Error('tri surfaceCount');"
        "var mesh = efx.graphics.createMesh(md);"
        "if (mesh.surfaceCount !== 1) throw new Error('mesh surfaceCount');"
        "mesh.destroy(); md.destroy();"
        "var q = efx.graphics.loadMeshData('quad.glb', { mesh: 'm' });"
        "if (q.surfaceCount !== 2) throw new Error('quad surfaceCount');"
        "q.destroy();"
        "var q2 = efx.graphics.loadMeshData('quad.glb', { mesh: 0 });"
        "if (q2.surfaceCount !== 2) throw new Error('quad index select');"
        "q2.destroy();"
        "function kind(fn) { try { fn(); } catch (e) {"
        "  if (e instanceof TypeError) return 'TypeError';"
        "  if (e instanceof Error) return 'Error';"
        "  return 'other'; } return 'none'; }"
        "if (kind(function () { efx.graphics.loadMeshData('corrupt.gltf'); }) !== 'Error')"
        "  throw new Error('corrupt not Error');"
        "if (kind(function () { efx.graphics.loadMeshData('triangle.gltf', { mesh: 'nope' }); }) !== 'Error')"
        "  throw new Error('unknown mesh not Error');"
        "if (kind(function () { efx.graphics.loadMeshData('triangle.gltf', { nope: 1 }); }) !== 'TypeError')"
        "  throw new Error('unknown field not TypeError');"
        "if (kind(function () { efx.graphics.loadMeshData('triangle.gltf', { mesh: {} }); }) !== 'TypeError')"
        "  throw new Error('bad mesh type not TypeError');"
        "if (kind(function () { efx.graphics.loadMeshData(5); }) !== 'TypeError')"
        "  throw new Error('bad path not TypeError');");
    efx_runtime_destroy(g_rt);
    g_rt = NULL;
    efx_render_end_frame();
    efx_render_shutdown();
    efx_resource_close(res);
    if (rc != 0) return fail("gltf js snippet raised");
    return 0;
}

/* F6c: skinned surface attributes accepted through createMeshData; the rig
 * itself stays opaque (no clip/joint query property, no new API) */
static int skin_js(void) {
    const char *code =
        "const P = [0,0,0, 1,0,0, 0,1,0];"
        "const J = [0,1,2,0, 1,0,0,0, 0,0,0,0];"
        "const W = [1,0,0,0, 0.5,0.5,0,0, 1,0,0,0];"
        "const md = efx.graphics.createMeshData([{ positions: P, joints: J, weights: W,"
        "  indices: [0,1,2] }]);"
        "if (md.surfaceCount !== 1) throw new Error('skinned surfaceCount');"
        "if (md.joints !== undefined || md.weights !== undefined)"
        "  throw new Error('rig must be opaque');"
        "if (md.clips !== undefined || md.jointCount !== undefined ||"
        "    md.clipCount !== undefined || md.skeleton !== undefined)"
        "  throw new Error('no rig query property');"
        "const mesh = efx.graphics.createMesh(md);"
        "if (mesh.clips !== undefined || mesh.jointCount !== undefined ||"
        "    mesh.skeleton !== undefined)"
        "  throw new Error('no rig query property on Mesh');"
        "if (typeof mesh.pose !== 'function')"
        "  throw new Error('pose missing in F7');"
        "if (efx.playAnimation !== undefined || efx.pauseAnimation !== undefined ||"
        "    efx.blendAnimations !== undefined)"
        "  throw new Error('no playback helper');"
        "md.destroy(); mesh.destroy();"
        T_HELPER
        "t(() => efx.graphics.createMeshData([{ positions: P, joints: J }]), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P, weights: W }]), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P, joints: J, weights: [1,0,0,0] }]), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P, joints: [0,1,2], weights: W }]), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P, joints: ['a',0,0,0, 1,0,0,0, 0,0,0,0], weights: W }]), TypeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P, joints: [0.5,0,0,0, 1,0,0,0, 0,0,0,0], weights: W }]), RangeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P, joints: J, weights: ['x',0,0,0, 0,0,0,0, 0,0,0,0] }]), TypeError);"
        "t(() => efx.graphics.createMeshData([{ positions: P, joints: J, weights: W, bogus: 1 }]), TypeError);";
    REQUIRE(!ok_js(code), "skin js");
    end_js();
    return 0;
}

/* F7: poseMesh + the skinned draw option over an imported rig */
static int pose_js(void) {
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    g_rt = efx_runtime_new(NULL, 0);
    if (!g_rt) return fail("runtime");
    int err = EFX_RESOURCE_OK;
    efx_resource *res = efx_resource_open(EFX_RES_FIXTURES "/gltf", &err);
    REQUIRE(res, "open gltf fixtures");
    efx_runtime_set_resource(g_rt, res);
    const char *code =
        "function kind(fn) { try { fn(); } catch (e) {"
        "  if (e instanceof TypeError) return 'TypeError';"
        "  if (e instanceof RangeError) return 'RangeError';"
        "  if (e instanceof Error) return 'Error'; return 'other'; } return 'none'; }"
        "var md = efx.graphics.loadMeshData('skin.gltf');"
        "var mesh = efx.graphics.createMesh(md); md.destroy();"
        "if (mesh.clips !== undefined || mesh.jointCount !== undefined)"
        "  throw new Error('rig must stay opaque');"
        "mesh.pose( { clip: 'move', time: 0.25 });"
        "mesh.pose( { clip: 0, time: 0.5 });"
        "mesh.pose( [{ clip: 'move', time: 0.1, weight: 1 },"
        "                    { clip: 'turn', time: 0.6, weight: 2 }]);"
        "mesh.pose( { clip: 'move', time: 5.5 });"
        "if (kind(function () { mesh.pose( { clip: 'nope', time: 0 }); }) !== 'Error')"
        "  throw new Error('unknown clip name');"
        "if (kind(function () { mesh.pose( { clip: 9, time: 0 }); }) !== 'RangeError')"
        "  throw new Error('clip index range');"
        "if (kind(function () { mesh.pose( { clip: 'move', time: 0, weight: -1 }); }) !== 'RangeError')"
        "  throw new Error('negative weight');"
        "if (kind(function () { mesh.pose( { clip: 'move', time: 0, bogus: 1 }); }) !== 'TypeError')"
        "  throw new Error('unknown sample field');"
        "if (kind(function () { mesh.pose( { clip: 'move', time: 'x' }); }) !== 'TypeError')"
        "  throw new Error('time type');"
        "if (kind(function () { mesh.pose( { clip: {}, time: 0 }); }) !== 'TypeError')"
        "  throw new Error('clip type');"
        "if (kind(function () { mesh.pose( 5); }) !== 'TypeError')"
        "  throw new Error('pose type');"
        "efx.graphics.drawMesh(mesh, { skinned: true });"
        "efx.graphics.drawMesh(mesh);"
        "if (kind(function () { efx.graphics.drawMesh(mesh, { skinned: 1 }); }) !== 'TypeError')"
        "  throw new Error('skinned type');"
        "if (kind(function () { efx.graphics.drawMesh(mesh, { bogus: 1 }); }) !== 'TypeError')"
        "  throw new Error('draw unknown field');"
        "var plain = efx.graphics.createMesh(efx.graphics.createMeshData(["
        "  { positions: [0,0,0, 1,0,0, 0,1,0], indices: [0,1,2] }]));"
        "if (kind(function () { plain.pose( { clip: 0, time: 0 }); }) !== 'TypeError')"
        "  throw new Error('rig-less pose');"
        "if (kind(function () { efx.graphics.drawMesh(plain, { skinned: true }); }) !== 'TypeError')"
        "  throw new Error('rig-less skinned draw');"
        "plain.destroy(); mesh.destroy();";
    int rc = efx_runtime_eval_string(g_rt, "test", code);
    int n = 0;
    const efx_record *recs = efx_render_records(&n);
    int ok = rc == 0 && n >= 2 &&
             recs[n - 2].type == EFX_RECORD_MESH &&
             recs[n - 2].u.mesh.skinned == 1 &&
             recs[n - 1].type == EFX_RECORD_MESH &&
             recs[n - 1].u.mesh.skinned == 0;
    efx_runtime_destroy(g_rt);
    g_rt = NULL;
    efx_render_end_frame();
    efx_render_shutdown();
    efx_resource_close(res);
    if (!ok) return fail("pose js");
    return 0;
}

/* F6d: evaluate REPL lines in the persistent global context — a throwing
 * line is recovered (fatal error flag stays clear), state persists across
 * lines, and efx.quit is a requested shutdown, not an error */
static int repl_eval(void) {
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    efx_runtime *rt = efx_runtime_new(NULL, 0);
    if (!rt) {
        return fail("runtime");
    }
    int ok = 1;
    if (efx_runtime_eval_repl_line(rt, "let x = 2") != 0) ok = 0;
    if (efx_runtime_eval_repl_line(rt, "if (x + 3 !== 5) throw new Error('state lost')") != 0) ok = 0;
    if (efx_runtime_eval_repl_line(rt, "throw new Error('repl-boom')") != 1) ok = 0;
    if (efx_runtime_in_error(rt)) ok = 0;
    if (efx_runtime_eval_repl_line(rt, "x + 1") != 0) ok = 0;
    if (efx_runtime_in_error(rt)) ok = 0;
    if (efx_runtime_eval_repl_line(rt, "efx.quit(7)") != 0) ok = 0;
    if (!efx_runtime_quit_requested(rt)) ok = 0;
    if (efx_runtime_quit_code(rt) != 7) ok = 0;
    if (efx_runtime_in_error(rt)) ok = 0;
    efx_runtime_destroy(rt);
    efx_render_end_frame();
    efx_render_shutdown();
    return ok ? 0 : fail("repl line evaluation");
}

/* F13: gamepad namespace bindings through the real quickjs runtime:
 * count/get/pad-view queries, connect/disconnect callbacks with the pad view,
 * validation/unsubscribe matrix (driven by the C injection seam). */
static int gamepad_js(void) {
    efx_input_reset();
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    g_rt = efx_runtime_new(NULL, 0);
    if (!g_rt) {
        return fail("runtime");
    }
    int rc = 0;
    const char *setup =
        "globalThis.__log = [];"
        "function kind(fn){ try { fn(); return 'none'; } catch (e) { return e.constructor.name; } }"
        "if (efx.gamepad.count !== 0) throw new Error('initial count');"
        "if (efx.gamepad.get(0) !== null) throw new Error('initial get');"
        "if (kind(() => efx.gamepad.onConnect(5)) !== 'TypeError') throw new Error('non-function reg');"
        "if (kind(() => efx.gamepad.onDisconnect(null)) !== 'TypeError') throw new Error('non-function reg2');"
        "var off = efx.gamepad.onConnect(function () { __log.push('SHOULD-NOT-FIRE'); });"
        "off(); off();"
        "efx.gamepad.onConnect(function (p) { __log.push('connect:' + p.name + ':' + p.connected); });"
        "efx.gamepad.onDisconnect(function (p) { __log.push('disconnect:' + p.name); });"
        "efx.registerUpdateHook(function (dt) {"
        "  var p = efx.gamepad.get(0);"
        "  __log.push('u:' + efx.gamepad.count + ':' + (p ? p.axis('leftX') : 'null') + ':' +"
        "    (p ? p.isDown('south') : 'null')); });";
    if (efx_runtime_eval_string(g_rt, "gp-setup", setup) != 0) {
        rc = fail("gamepad setup");
        goto done;
    }

    unsigned char btns[1] = {1};
    float axes[6] = {0.25f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f};
    efx_input_gamepad_inject_connect(0, "Pad", NULL, 1);
    efx_input_gamepad_inject_state(0, 1, btns, 6, axes);
    efx_input_begin_frame();
    if (efx_runtime_dispatch_input(g_rt) != EFX_HOOK_OK) {
        rc = fail("gamepad dispatch");
        goto done;
    }
    if (efx_runtime_eval_string(g_rt, "gp-mid",
        "if (efx.gamepad.count !== 1) throw new Error('count');"
        "var p = efx.gamepad.get(0);"
        "if (!p || !p.connected || p.name !== 'Pad' || !p.mapped) throw new Error('view');"
        "if (!p.isDown('south') || !p.isPressed('south') || p.isReleased('south')) throw new Error('button');"
        "if (p.axis('leftX') !== 0.25) throw new Error('axis');"
        "if (p.rawButton(0) !== 1) throw new Error('rawButton');"
        "if (p.rawAxis(0) !== 0.25) throw new Error('rawAxis');"
        "if (kind(() => p.isDown('notabutton')) !== 'TypeError') throw new Error('unknown button');"
        "if (kind(() => p.axis('leftZ')) !== 'TypeError') throw new Error('unknown axis');"
        "if (kind(() => p.rawButton('x')) !== 'TypeError') throw new Error('bad raw index');"
        "if (__log.join('|') !== 'connect:Pad:true') throw new Error('connect log: ' + __log.join('|'));"
        "if (efx.gamepad.get(3) !== null) throw new Error('empty slot');") != 0) {
        rc = fail("gamepad mid-frame state");
        goto done;
    }
    if (efx_runtime_call_hook(g_rt, 1, 0.0) != EFX_HOOK_OK) {
        rc = fail("gamepad update hook");
        goto done;
    }
    if (efx_runtime_eval_string(g_rt, "gp-frame1",
        "if (__log.join('|') !== 'connect:Pad:true|u:1:0.25:true')"
        "  throw new Error('callback-before-update: ' + __log.join('|'));") != 0) {
        rc = fail("gamepad callback ordering");
        goto done;
    }
    efx_input_end_frame();

    efx_input_gamepad_inject_disconnect(0);
    efx_input_begin_frame();
    if (efx_runtime_dispatch_input(g_rt) != EFX_HOOK_OK) {
        rc = fail("gamepad disconnect dispatch");
        goto done;
    }
    if (efx_runtime_eval_string(g_rt, "gp-frame2",
        "if (efx.gamepad.count !== 0 || efx.gamepad.get(0) !== null)"
        "  throw new Error('disconnect state');"
        "if (__log[__log.length - 1] !== 'disconnect:Pad')"
        "  throw new Error('disconnect log: ' + __log.join('|'));") != 0) {
        rc = fail("gamepad disconnect");
        goto done;
    }
    efx_input_end_frame();

done:
    efx_runtime_destroy(g_rt);
    g_rt = NULL;
    efx_render_end_frame();
    efx_render_shutdown();
    return rc;
}

/* F9: input namespace bindings + frame-staged dispatch through the real
 * quickjs runtime: query/event parity, validation/unsubscribe matrix, and
 * callback-before-update ordering (injected via the C simulation seam). */
static int input_js(void) {
    efx_input_reset();
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    efx_input_set_window(1024, 600, 1.0f);
    g_rt = efx_runtime_new(NULL, 0);
    if (!g_rt) {
        return fail("runtime");
    }
    int rc = 0;
    const char *setup =
        "globalThis.__log = [];"
        "function kind(fn){ try { fn(); return 'none'; } catch (e) { return e.constructor.name; } }"
        "if (kind(() => efx.keyboard.isDown('notakey')) !== 'TypeError') throw new Error('unknown key');"
        "if (kind(() => efx.mouse.isDown('side')) !== 'TypeError') throw new Error('unknown button');"
        "if (kind(() => efx.keyboard.onDown(5)) !== 'TypeError') throw new Error('non-function reg');"
        "if (kind(() => efx.mouse.onWheel(null)) !== 'TypeError') throw new Error('non-function reg2');"
        "var off = efx.keyboard.onDown(function () { __log.push('SHOULD-NOT-FIRE'); });"
        "off(); off();"
        "efx.keyboard.onDown(function (e) { __log.push('kd:' + e.key + ':' + e.repeat + ':' + e.mods.join(',')); });"
        "efx.keyboard.onUp(function (e) { __log.push('ku:' + e.key); });"
        "efx.keyboard.onChar(function (e) { __log.push('ch:' + e.char); });"
        "efx.mouse.onMove(function (e) { __log.push('mm:' + e.x + ':' + e.dx); });"
        "efx.mouse.onWheel(function (e) { __log.push('mw:' + e.dx + ':' + e.dy); });"
        "efx.registerUpdateHook(function (dt) {"
        "  __log.push('u:' + efx.keyboard.isDown('space') + ':' +"
        "    efx.keyboard.isPressed('space') + ':' + efx.keyboard.isReleased('space')); });";
    if (efx_runtime_eval_string(g_rt, "input-setup", setup) != 0) {
        rc = fail("input setup");
        goto done;
    }
    int key = efx_input_key_id("space");
    efx_input_inject_key(key, 1, 0, EFX_INPUT_MOD_SHIFT);
    efx_input_inject_char('A');
    efx_input_inject_mouse_move(10, 20, 3, 4);
    efx_input_inject_wheel(0, 2);
    efx_input_begin_frame();
    if (efx_runtime_dispatch_input(g_rt) != EFX_HOOK_OK) {
        rc = fail("input dispatch");
        goto done;
    }
    if (efx_runtime_eval_string(g_rt, "input-mid",
        "if (__log.join('|') !== 'kd:space:false:shift|ch:A|mm:10:3|mw:0:2')"
        "  throw new Error('order/params: ' + __log.join('|'));"
        "if (!efx.keyboard.isDown('space') || !efx.keyboard.isPressed('space') ||"
        "    efx.keyboard.isReleased('space')) throw new Error('key state');"
        "if (efx.mouse.x !== 10 || efx.mouse.y !== 20) throw new Error('pointer');"
        "if (efx.mouse.delta[0] !== 3 || efx.mouse.delta[1] !== 4) throw new Error('delta');"
        "if (efx.mouse.wheel[0] !== 0 || efx.mouse.wheel[1] !== 2) throw new Error('wheel');"
        "if (efx.window.width !== 1024 || efx.window.height !== 600 || efx.window.dpiScale !== 1)"
        "  throw new Error('window');"
        "if (efx.window.size[0] !== 1024 || efx.window.size[1] !== 600) throw new Error('window size');") != 0) {
        rc = fail("input mid-frame state");
        goto done;
    }
    if (efx_runtime_call_hook(g_rt, 1, 0.0) != EFX_HOOK_OK) {
        rc = fail("input update hook");
        goto done;
    }
    if (efx_runtime_eval_string(g_rt, "input-frame1",
        "if (__log.join('|') !== 'kd:space:false:shift|ch:A|mm:10:3|mw:0:2|u:true:true:false')"
        "  throw new Error('callback-before-update: ' + __log.join('|'));") != 0) {
        rc = fail("callback ordering");
        goto done;
    }
    efx_input_end_frame();

    efx_input_begin_frame();
    if (efx_runtime_call_hook(g_rt, 1, 0.0) != EFX_HOOK_OK) {
        rc = fail("frame2 update");
        goto done;
    }
    if (efx_runtime_eval_string(g_rt, "input-frame2",
        "if (__log[__log.length - 1] !== 'u:true:false:false') throw new Error('edge expiry');") != 0) {
        rc = fail("press edge expiry");
        goto done;
    }
    efx_input_end_frame();

    efx_input_inject_key(key, 0, 0, 0);
    efx_input_begin_frame();
    if (efx_runtime_dispatch_input(g_rt) != EFX_HOOK_OK ||
        efx_runtime_call_hook(g_rt, 1, 0.0) != EFX_HOOK_OK) {
        rc = fail("release dispatch");
        goto done;
    }
    if (efx_runtime_eval_string(g_rt, "input-frame3",
        "if (__log[__log.length - 2] !== 'ku:space' ||"
        "    __log[__log.length - 1] !== 'u:false:false:true')"
        "  throw new Error('release: ' + __log.join('|'));") != 0) {
        rc = fail("release edge");
        goto done;
    }
    efx_input_end_frame();

    efx_input_begin_frame();
    if (efx_runtime_call_hook(g_rt, 1, 0.0) != EFX_HOOK_OK) {
        rc = fail("frame4 update");
        goto done;
    }
    if (efx_runtime_eval_string(g_rt, "input-frame4",
        "if (__log[__log.length - 1] !== 'u:false:false:false') throw new Error('release expiry');") != 0) {
        rc = fail("release edge expiry");
        goto done;
    }
    efx_input_end_frame();

    efx_input_inject_key(efx_input_key_id("a"), 1, 0, 0);
    efx_input_focus_lost();
    efx_input_begin_frame();
    if (efx_runtime_eval_string(g_rt, "input-focus",
        "if (efx.keyboard.isDown('a')) throw new Error('focus clearing');") != 0) {
        rc = fail("focus clearing");
        goto done;
    }
    efx_input_end_frame();

done:
    efx_runtime_destroy(g_rt);
    g_rt = NULL;
    efx_render_end_frame();
    efx_render_shutdown();
    return rc;
}

/* F10: run an entry in a fresh runtime bound to `root`; returns 1 when the
 * run raised/reported an error (the expected outcome for the error cases). */
static int module_expect_error(const char *root, const char *entry,
                               const char *source) {
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    efx_runtime *rt = efx_runtime_new(NULL, 0);
    if (!rt) {
        efx_render_end_frame();
        efx_render_shutdown();
        return 0;
    }
    int err = EFX_RESOURCE_OK;
    efx_resource *res = efx_resource_open(root, &err);
    if (!res) {
        efx_runtime_destroy(rt);
        efx_render_end_frame();
        efx_render_shutdown();
        return 0;
    }
    efx_runtime_set_resource(rt, res);
    int rc = efx_runtime_run_entry(rt, entry, source);
    int failed = (rc != 0) || efx_runtime_in_error(rt);
    efx_runtime_destroy(rt);
    efx_resource_close(res);
    efx_render_end_frame();
    efx_render_shutdown();
    return failed;
}

/* F10: the shared CommonJS runtime driven through the desktop binding —
 * relative/parent/root resolution, the .js fallback, cache identity, cycles,
 * __esModule interop, star re-export, JSON modules, and module errors. */
static int module_js(void) {
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    g_rt = efx_runtime_new(NULL, 0);
    if (!g_rt) {
        efx_render_end_frame();
        efx_render_shutdown();
        return fail("runtime");
    }
    int err = EFX_RESOURCE_OK;
    efx_resource *res = efx_resource_open(EFX_MOD_FIXTURES, &err);
    REQUIRE(res, "open module fixtures");
    efx_runtime_set_resource(g_rt, res);
    const char *entry =
        "var m1 = require('./lib/math.js');"
        "var m2 = require('./lib/math.js');"
        "globalThis.__m = {"
        "  ident: m1 === m2, sum: m1.add(1, 2), value: m1.value,"
        "  root: require('shared/util.js').tag,"
        "  parent: require('./lib/uses_parent.js').tag,"
        "  fallback: require('./lib/math').value,"
        "  jsonName: require('./json/config.json').name,"
        "  cycName: require('./cycle/a.js').name,"
        "  cycB: require('./cycle/a.js').b.name,"
        "  cycAName: require('./cycle/a.js').b.aName,"
        "  cycDone: require('./cycle/a.js').done,"
        "  interop: require('./interop/consumer.js').got,"
        "  starNamed: require('./interop/star.js').named,"
        "  starExtra: require('./interop/star.js').extra"
        "};";
    int rc = efx_runtime_run_entry(g_rt, "entry.js", entry);
    if (rc != 0 || efx_runtime_in_error(g_rt)) {
        end_js();
        efx_resource_close(res);
        return fail("module success entry raised");
    }
    rc = efx_runtime_eval_string(g_rt, "check",
        "var m = globalThis.__m;"
        "if (!m.ident || m.sum !== 3 || m.value !== 21) throw new Error('identity/math');"
        "if (m.root !== 'shared' || m.parent !== 'shared') throw new Error('resolution');"
        "if (m.fallback !== 21) throw new Error('fallback');"
        "if (m.jsonName !== 'config') throw new Error('json');"
        "if (m.cycName !== 'a' || m.cycB !== 'b' || m.cycAName !== 'a' || !m.cycDone)"
        "  throw new Error('cycle');"
        "if (m.interop !== 42) throw new Error('interop');"
        "if (m.starNamed !== 'hello' || m.starExtra !== 'x') throw new Error('star');");
    efx_runtime_destroy(g_rt);
    g_rt = NULL;
    efx_render_end_frame();
    efx_render_shutdown();
    efx_resource_close(res);
    if (rc != 0) {
        return fail("module semantics assertion failed");
    }

    if (!module_expect_error(EFX_MOD_FIXTURES, "entry.js",
                             "require('./definitely_missing.js');")) {
        return fail("missing module did not error");
    }
    if (!module_expect_error(EFX_MOD_FIXTURES, "entry.js",
                             "require('../outside.js');")) {
        return fail("root escape did not error");
    }
    if (!module_expect_error(EFX_MOD_FIXTURES, "entry.js",
                             "require('./json/bad.json');")) {
        return fail("invalid JSON did not error");
    }
    if (!module_expect_error(EFX_MOD_FIXTURES, "entry.js",
                             "import x from './lib/math.js';")) {
        return fail("static import did not error");
    }
    if (!module_expect_error(EFX_MOD_FIXTURES, "not_a_module.js", NULL)) {
        return fail("missing entry did not error");
    }
    if (!module_expect_error(EFX_MOD_ERROR_FIXTURES, "main.js", NULL)) {
        return fail("throwing module did not error");
    }
    return 0;
}

/* F10: module-shaped and global entry hooks register once, in order, with
 * the same pick_hooks semantics on the desktop binding. */
static int module_hooks_js(void) {
    const char *root = EFX_MOD_FIXTURES;
    int ok = 1;

    /* exported hooks == local declarations: the explicit hook stays first and
       the entry hook is registered once (not twice) */
    {
        efx_render_install_sink(&g_sink);
        efx_render_reset_state();
        efx_render_set_viewport(1024, 600);
        efx_render_begin_frame();
        int err = EFX_RESOURCE_OK;
        efx_resource *res = efx_resource_open(root, &err);
        g_rt = efx_runtime_new(NULL, 0);
        if (!g_rt || !res) {
            ok = 0;
        } else {
            efx_runtime_set_resource(g_rt, res);
            const char *entry =
                "globalThis.__calls = [];"
                "function update(dt) { globalThis.__calls.push('u' + dt); }"
                "function render() { globalThis.__calls.push('r'); }"
                "module.exports.update = update;"
                "module.exports.render = render;"
                "efx.registerUpdateHook(function () { globalThis.__calls.push('explicit'); });";
            if (efx_runtime_run_entry(g_rt, "hooks.js", entry) != 0 ||
                efx_runtime_in_error(g_rt)) {
                ok = 0;
            } else {
                int has_u = 0, has_r = 0;
                efx_runtime_pick_hooks(g_rt, &has_u, &has_r);
                if (!has_u || !has_r) {
                    ok = 0;
                } else if (efx_runtime_call_hook(g_rt, 1, 0.5) != EFX_HOOK_OK ||
                           efx_runtime_call_hook(g_rt, 0, 0.0) != EFX_HOOK_OK) {
                    ok = 0;
                } else if (efx_runtime_eval_string(g_rt, "check",
                        "if (globalThis.__calls.join(',') !== 'explicit,u0.5,r')"
                        "  throw new Error(globalThis.__calls.join(','));") != 0) {
                    ok = 0;
                }
            }
        }
        efx_runtime_destroy(g_rt);
        g_rt = NULL;
        efx_render_end_frame();
        efx_render_shutdown();
        efx_resource_close(res);
        if (!ok) return fail("module-shaped exported hooks");
    }

    /* exports-only entry (no local declarations) */
    {
        efx_render_install_sink(&g_sink);
        efx_render_reset_state();
        efx_render_set_viewport(1024, 600);
        efx_render_begin_frame();
        int err = EFX_RESOURCE_OK;
        efx_resource *res = efx_resource_open(root, &err);
        g_rt = efx_runtime_new(NULL, 0);
        if (!g_rt || !res) {
            ok = 0;
        } else {
            efx_runtime_set_resource(g_rt, res);
            const char *entry =
                "globalThis.__n = 0;"
                "module.exports.update = function () { globalThis.__n++; };"
                "module.exports.render = function () {};";
            int has_u = 0, has_r = 0;
            if (efx_runtime_run_entry(g_rt, "hooks2.js", entry) != 0 ||
                efx_runtime_in_error(g_rt)) {
                ok = 0;
            } else {
                efx_runtime_pick_hooks(g_rt, &has_u, &has_r);
                if (!has_u || !has_r ||
                    efx_runtime_call_hook(g_rt, 1, 0.0) != EFX_HOOK_OK ||
                    efx_runtime_eval_string(g_rt, "check",
                        "if (globalThis.__n !== 1) throw new Error('exports hook');") != 0) {
                    ok = 0;
                }
            }
        }
        efx_runtime_destroy(g_rt);
        g_rt = NULL;
        efx_render_end_frame();
        efx_render_shutdown();
        efx_resource_close(res);
        if (!ok) return fail("exports-only hooks");
    }

    /* global-only entry still works through the load-time sugar */
    {
        efx_render_install_sink(&g_sink);
        efx_render_reset_state();
        efx_render_set_viewport(1024, 600);
        efx_render_begin_frame();
        int err = EFX_RESOURCE_OK;
        efx_resource *res = efx_resource_open(root, &err);
        g_rt = efx_runtime_new(NULL, 0);
        if (!g_rt || !res) {
            ok = 0;
        } else {
            efx_runtime_set_resource(g_rt, res);
            const char *entry =
                "globalThis.__g = 0;"
                "function update() { globalThis.__g++; }"
                "function render() {}";
            int has_u = 0, has_r = 0;
            if (efx_runtime_run_entry(g_rt, "hooks3.js", entry) != 0 ||
                efx_runtime_in_error(g_rt)) {
                ok = 0;
            } else {
                efx_runtime_pick_hooks(g_rt, &has_u, &has_r);
                if (!has_u || !has_r ||
                    efx_runtime_call_hook(g_rt, 1, 0.0) != EFX_HOOK_OK ||
                    efx_runtime_eval_string(g_rt, "check",
                        "if (globalThis.__g !== 1) throw new Error('global hook');") != 0) {
                    ok = 0;
                }
            }
        }
        efx_runtime_destroy(g_rt);
        g_rt = NULL;
        efx_render_end_frame();
        efx_render_shutdown();
        efx_resource_close(res);
        if (!ok) return fail("global-sugar hooks");
    }
    return 0;
}

/* F11: drawBillboard records a world-space quad with the 3D camera */
static int billboard_js(void) {
    const char *code =
        "const t = efx.graphics.createTexture(efx.graphics.createImageData(4, 4,"
        "  new Uint8Array(4 * 4 * 4).fill(255)));"
        "efx.graphics.setCamera3D([0, 0, 5], [0, 0, 0], 60);"
        "efx.graphics.setBlendMode('additive');"
        "efx.graphics.drawBillboard(t, [1, 2, 3], { size: [2, 3], facing: 'y',"
        "  rotation: 45, color: [0.5, 0.25, 0.1, 0.8] });"
        "try { efx.graphics.drawBillboard(undefined, [0,0,0], { size: [1,1] }); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }"
        "try { efx.graphics.drawBillboard(t, [0,0,0], { size: [0,1] }); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof RangeError)) throw e; }";
    REQUIRE(!ok_js(code), "billboard js snippet");
    REQUIRE(rec_count() == 1, "billboard record count");
    int n = 0;
    const efx_record *r = efx_render_records(&n);
    REQUIRE(r[0].type == EFX_RECORD_BILLBOARD, "billboard record type");
    const efx_billboard_record *b = &r[0].u.billboard;
    REQUIRE(feq(b->pos[0], 1) && feq(b->pos[2], 3) && feq(b->w, 2) &&
            b->facing == EFX_FACING_Y && b->blend == EFX_BLEND_ADDITIVE &&
            feq(b->camera.pos[2], 5), "billboard record fields");
    end_js();
    return 0;
}

/* F11: createParticleSystem config, emit/count/speedScale/destroy lifecycle */
static int particles_js(void) {
    const char *code =
        "const t = efx.graphics.createTexture(efx.graphics.createImageData(4, 4,"
        "  new Uint8Array(4 * 4 * 4).fill(255)));"
        "const ps = efx.graphics.createParticleSystem(t, 32,"
        "  [1, 2], { emissionRate: 10, position: [0, 0, 0],"
        "  direction: [0, 1, 0], speed: [1, 2], gravity: [0, -1, 0],"
        "  sizes: [1, 3], colors: [[1, 0, 0, 1], [1, 1, 0, 0]],"
        "  facing: 'view', blend: 'additive', emissionShape: { shape: 'sphere', size: [1, 1, 1] } });"
        "if (ps.count !== 0) throw new Error('fresh count ' + ps.count);"
        "ps.emit(5);"
        "if (ps.count !== 5) throw new Error('emit count ' + ps.count);"
        "ps.speedScale = 2;"
        "if (ps.speedScale !== 2) throw new Error('speedScale');"
        "efx.graphics.drawParticles(ps);"
        "ps.set({ emissionRate: 0, position: [1, 0, 0] });"
        "ps.pause(); ps.start(); ps.reset();"
        "if (ps.count !== 0) throw new Error('reset');"
        "ps.destroy();"
        "try { ps.emit(1); throw new Error('no'); } catch (e) {"
        "  if (!(e instanceof TypeError)) throw e; }"
        "try { efx.graphics.createParticleSystem(undefined, 4, 1); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }"
        "try { efx.graphics.createParticleSystem(t, 0, 1); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof RangeError)) throw e; }";
    REQUIRE(!ok_js(code), "particles js snippet");
    REQUIRE(rec_count() == 1, "particle record count");
    int n = 0;
    const efx_record *r = efx_render_records(&n);
    REQUIRE(r[0].type == EFX_RECORD_PARTICLES, "particle record type");
    end_js();
    return 0;
}

/* F11: drawSprites is a 2D batch and records nothing when an entry is bad.
 * Single runtime: the harness registers resource classes once per process. */
static int sprites_js(void) {
    const char *code =
        "const t = efx.graphics.createTexture(efx.graphics.createImageData(4, 4,"
        "  new Uint8Array(4 * 4 * 4).fill(255)));"
        "efx.graphics.drawSprites(t, [{ x: 0, y: 0, size: [4, 4] }, { x: 10, y: 0, rotation: 45 }]);"
        "try { efx.graphics.drawSprites(t, [{ x: 0, y: 0 }, { x: 1, y: 1, size: [0, 5] }]);"
        "  throw new Error('no'); } catch (e) {"
        "  if (!(e instanceof RangeError)) throw e; }";
    REQUIRE(!ok_js(code), "sprites js snippet");
    /* two valid sprites recorded; the failed call recorded none */
    REQUIRE(rec_count() == 2, "drawSprites atomicity / count");
    end_js();
    return 0;
}

/* F12: the efx.physics surface — world config, bodies, contacts, character,
 * queries, validation, and destroy/use-after-destroy safety */
static int physics_js(void) {
    const char *code =
        "efx.physics.clear();"
        "if (Math.abs(efx.physics.gravity[1] + 9.81) > 0.001) throw new Error('gravity default');"
        "efx.physics.gravity = [0, -10, 0];"
        "if (efx.physics.gravity[1] !== -10) throw new Error('gravity set');"
        "if (efx.physics.iterations !== 8) throw new Error('iter default');"
        "efx.physics.iterations = 12;"
        "try { efx.physics.gravity = [0, 1]; throw new Error('no'); } catch(e){ if(!(e instanceof RangeError)) throw e; }"
        "try { efx.physics.iterations = 0; throw new Error('no'); } catch(e){ if(!(e instanceof RangeError)) throw e; }"
        "const ground = efx.physics.createBody({ type: 'box', size: [10,1,10] }, { position: [0,-0.5,0] });"
        "const box = efx.physics.createBody({ type: 'box', size: [1,1,1] }, { dynamic: true, mass: 1, position: [0,1,0] });"
        "for (let i=0;i<180;i++) efx.physics.step(1/60);"
        "const cs = box.contacts;"
        "if (cs.length < 1) throw new Error('no contact');"
        "if (cs[0].body !== ground) throw new Error('contact identity');"
        "if (cs[0].normal[1] < 0.9) throw new Error('contact normal');"
        "const tf = box.transform;"
        "if (tf.length !== 16 || tf[12] !== box.position[0] || tf[13] !== box.position[1]) throw new Error('transform');"
        "if (Math.abs(box.position[1] - 0.5) > 0.06) throw new Error('rest y ' + box.position[1]);"
        "box.velocity = [1,0,0];"
        "if (box.velocity[0] !== 1) throw new Error('velocity set');"
        "box.applyImpulse([0,2,0]);"
        "if (box.velocity[1] < 1.9) throw new Error('impulse');"
        "const ch = efx.physics.createCharacter(0.4, 1.8, { position: [0,1,0] });"
        "const mr = ch.moveAndSlide([0,-0.5,0]);"
        "if (!mr.onFloor) throw new Error('char floor');"
        "if (typeof ch.onFloor !== 'boolean') throw new Error('onFloor');"
        "ch.velocity = [1,2,3];"
        "if (ch.velocity[2] !== 3) throw new Error('char velocity');"
        "const hit = efx.physics.raycast([0,5,0],[0,-1,0], 20);"
        "if (!hit || !hit.body) throw new Error('raycast');"
        "try { efx.physics.raycast([0,0,0],[1,0,0], undefined); throw new Error('no'); } catch(e){ if(!(e instanceof TypeError)) throw e; }"
        "const ov = efx.physics.overlap({ type:'sphere', radius: 1 }, { position: [0,0.5,0] });"
        "if (ov.length < 1) throw new Error('overlap');"
        "const sc = efx.physics.shapeCast({ type:'sphere', radius: 0.5 }, [0,5,0], [0,-4,0]);"
        "if (!sc || sc.fraction < 0 || sc.fraction > 1) throw new Error('shapecast');"
        "try { efx.physics.createBody({ type:'sphere', radius: 0 }); throw new Error('no'); } catch(e){ if(!(e instanceof RangeError)) throw e; }"
        "try { efx.physics.createBody({ type:'nope' }); throw new Error('no'); } catch(e){ if(!(e instanceof TypeError)) throw e; }"
        "try { efx.physics.createBody({ type:'sphere', radius: 1, bogus: 1 }); throw new Error('no'); } catch(e){ if(!(e instanceof TypeError)) throw e; }"
        "try { efx.physics.createBody({ type:'sphere', radius: 1 }, { dynamic: true, mass: 0 }); throw new Error('no'); } catch(e){ if(!(e instanceof RangeError)) throw e; }"
        "try { efx.physics.createCharacter(0.4, 0.5); throw new Error('no'); } catch(e){ if(!(e instanceof RangeError)) throw e; }"
        "box.destroy(); box.destroy();"
        "try { box.position; throw new Error('no'); } catch(e){ if(!(e instanceof TypeError)) throw e; }"
        "ch.destroy();"
        "efx.physics.clear();";
    REQUIRE(!ok_js(code), "physics js snippet");
    end_js();
    return 0;
}

/* destroy() on every non-Texture resource class returns without leaving an
 * exception pending on the context (invisible to scripts, so asserted here) */
static int destroy_no_pending_exception(void) {
    static const struct {
        const char *root;
        const char *expr;
    } rows[] = {
        {NULL, "efx.graphics.createImageData(1, 1, new Uint8Array(4))"},
        {NULL, "efx.graphics.createMeshData([{ positions: [0,0,0, 1,0,0, 0,1,0] }])"},
        {NULL, "efx.graphics.createMesh(efx.graphics.createMeshData([{ positions: [0,0,0, 1,0,0, 0,1,0] }]))"},
        {NULL, "efx.graphics.createRenderTarget(8, 8)"},
        {EFX_RES_FIXTURES, "efx.graphics.loadFontData('font.ttf')"},
        {EFX_RES_FIXTURES, "efx.graphics.createFont(efx.graphics.loadFontData('font.ttf'), 16)"},
        {NULL, "efx.graphics.createParticleSystem(efx.graphics.whiteTexture, 4, 1)"},
        {EFX_AUDIO_FIXTURES, "efx.audio.loadAudioData('tone.wav')"},
        {EFX_AUDIO_FIXTURES, "efx.audio.loadAudioStream('tone.mp3')"},
        {EFX_AUDIO_FIXTURES, "efx.audio.playAudio(efx.audio.loadAudioData('tone.wav'))"},
    };
    int failures = 0;
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        efx_render_install_sink(&g_sink);
        efx_render_reset_state();
        efx_render_set_viewport(1024, 600);
        efx_render_begin_frame();
        g_rt = efx_runtime_new(NULL, 0);
        if (!g_rt) return fail("runtime");
        efx_resource *res = NULL;
        if (rows[i].root) {
            int err = EFX_RESOURCE_OK;
            res = efx_resource_open(rows[i].root, &err);
            REQUIRE(res, "open fixtures");
            efx_runtime_set_resource(g_rt, res);
        }
        char code[256];
        snprintf(code, sizeof(code), "globalThis.r = %s;", rows[i].expr);
        JSContext *ctx = efx_runtime_context(g_rt);
        int created = efx_runtime_eval_string(g_rt, "test", code) == 0 &&
                      !JS_HasException(ctx);
        JSValue glob = JS_GetGlobalObject(ctx);
        JSValue r = JS_GetPropertyStr(ctx, glob, "r");
        JSValue fn = JS_GetPropertyStr(ctx, r, "destroy");
        JSValue ret = JS_Call(ctx, fn, r, 0, NULL);
        int threw = JS_IsException(ret);
        int pending = JS_HasException(ctx);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, fn);
        JS_FreeValue(ctx, r);
        JS_FreeValue(ctx, glob);
        end_js();
        efx_resource_close(res);
        if (!created || threw || pending) {
            fprintf(stderr, "%s: %s\n", rows[i].expr,
                    !created ? "create failed"
                             : threw ? "destroy threw"
                                     : "exception pending after destroy");
            failures++;
        }
    }
    return failures ? fail("destroy left an exception pending") : 0;
}

/* ADR 0050/0051 guard: efx.graphics holds exactly the 33 functions plus the
 * whiteTexture property; efx.math/efx.io/efx.color hold exactly their
 * members; efx.args is a read-only array; the root keeps only the lifecycle
 * members and the domain sub-namespaces. */
static int graphics_ns_js(void) {
    int rc = run_js(
        "var g = efx.graphics;"
        "if (!g || typeof g !== 'object') throw new Error('efx.graphics missing');"
        "var names = ['beginRenderTarget', 'createFont', 'createImageData',"
        "  'createMesh', 'createMeshData', 'createParticleSystem',"
        "  'createRenderTarget', 'createTexture', 'drawBillboard', 'drawMesh',"
        "  'drawParticles', 'drawQuad', 'drawSprites', 'drawText',"
        "  'endRenderTarget', 'loadFontData', 'loadImage', 'loadMeshData',"
        "  'makeCapsule', 'makeCube', 'makePlane', 'makeSphere',"
        "  'setBlendMode', 'setCamera2D', 'setCamera3D',"
        "  'setClearColor', 'setDirectionalLight', 'setLight',"
        "  'setPostEffects', 'setRenderScale'];"
        "for (var i = 0; i < names.length; i++) {"
        "  if (typeof g[names[i]] !== 'function')"
        "    throw new Error('efx.graphics missing ' + names[i]);"
        "  if (names[i] in efx) throw new Error('root alias remains: ' + names[i]);"
        "}"
        "if (g.measureText !== undefined || g.poseMesh !== undefined ||"
        "    g.setMeshSurfaceMaterial !== undefined)"
        "  throw new Error('removed graphics functions remain');"
        "if (!('whiteTexture' in g)) throw new Error('efx.graphics.whiteTexture missing');"
        "if (g.whiteTexture.width !== 1 || g.whiteTexture.height !== 1)"
        "  throw new Error('whiteTexture dims');"
        "if ('whiteTexture' in efx) throw new Error('root whiteTexture remains');"
        "if (Object.getOwnPropertyNames(g).length !== names.length + 1)"
        "  throw new Error('unexpected efx.graphics members');"
        "var m = efx.math;"
        "if (!m) throw new Error('efx.math missing');"
        "var mn = ['mat4', 'vec3', 'quat'];"
        "for (i = 0; i < mn.length; i++) {"
        "  if (!m[mn[i]] || typeof m[mn[i]] !== 'object')"
        "    throw new Error('efx.math missing ' + mn[i]);"
        "  if (mn[i] in efx) throw new Error('root math alias remains: ' + mn[i]);"
        "}"
        "if (Object.getOwnPropertyNames(m).length !== mn.length)"
        "  throw new Error('unexpected efx.math members');"
        "var io = efx.io;"
        "if (!io) throw new Error('efx.io missing');"
        "var ion = ['loadText', 'loadData'];"
        "for (i = 0; i < ion.length; i++) {"
        "  if (typeof io[ion[i]] !== 'function')"
        "    throw new Error('efx.io missing ' + ion[i]);"
        "  if (ion[i] in efx) throw new Error('root io alias remains: ' + ion[i]);"
        "}"
        "if (Object.getOwnPropertyNames(io).length !== ion.length)"
        "  throw new Error('unexpected efx.io members');"
        "var c = efx.color;"
        "if (!c) throw new Error('efx.color missing');"
        "var cn = ['aqua', 'black', 'blue', 'fuchsia', 'gray', 'green', 'lime',"
        "  'maroon', 'navy', 'olive', 'purple', 'red', 'silver', 'teal',"
        "  'white', 'yellow', 'transparent'];"
        "for (i = 0; i < cn.length; i++) {"
        "  var v = c[cn[i]];"
        "  if (!Array.isArray(v) || v.length !== 4) throw new Error('bad color ' + cn[i]);"
        "  if (!Object.isFrozen(v)) throw new Error('unfrozen color ' + cn[i]);"
        "}"
        "if (Object.getOwnPropertyNames(c).length !== cn.length)"
        "  throw new Error('unexpected efx.color members');"
        "if (c.white[0] !== 1 || c.white[1] !== 1 || c.white[2] !== 1 || c.white[3] !== 1)"
        "  throw new Error('color.white wrong');"
        "if (c.gray[0] !== 0.5 || c.green[1] !== 0.5 || c.silver[0] !== 0.75)"
        "  throw new Error('color level wrong');"
        "if (c.transparent[3] !== 0) throw new Error('transparent wrong');"
        "if (typeof efx.args === 'function') throw new Error('args still a function');"
        "if (!Array.isArray(efx.args)) throw new Error('args not an array');"
        "if (efx.args.length !== 0) throw new Error('args not empty');"
        "var root = ['log', 'quit', 'args', 'registerUpdateHook',"
        "  'registerRenderHook', 'math', 'io', 'color', 'keyboard', 'mouse',"
        "  'window', 'physics', 'gamepad', 'audio', 'graphics'];"
        "for (i = 0; i < root.length; i++) {"
        "  if (!(root[i] in efx)) throw new Error('root missing ' + root[i]);"
        "}"
        "var gone = ['whiteTexture', 'loadText', 'mat4', 'vec3', 'quat'];"
        "for (i = 0; i < gone.length; i++) {"
        "  if (gone[i] in efx) throw new Error('root alias remains: ' + gone[i]);"
        "}"
        "if (Object.getOwnPropertyNames(efx).length !== root.length)"
        "  throw new Error('unexpected root members: ' +"
        "    Object.getOwnPropertyNames(efx).join(','));");
    end_js();
    return rc == 0 ? 0 : fail("namespace shape");
}

static const efx_test_case cases[] = {
    EFX_CASE(graphics_ns_js),
    EFX_CASE(white),
    EFX_CASE(quad_record),
    EFX_CASE(size_derivation),
    EFX_CASE(origin_pivot),
    EFX_CASE(quad_validation),
    EFX_CASE(texture_size_getters),
    EFX_CASE(camera_snapshot),
    EFX_CASE(src_oob),
    EFX_CASE(budget),
    EFX_CASE(texture_lifecycle),
    EFX_CASE(blend_snapshot),
    EFX_CASE(default_camera),
    EFX_CASE(clear_color_js),
    EFX_CASE(hooks_registration),
    EFX_CASE(meshdata_js),
    EFX_CASE(meshdata_cap_js),
    EFX_CASE(mesh_js),
    EFX_CASE(camera3d_js),
    EFX_CASE(f4a_js),
    EFX_CASE(f4b_js),
    EFX_CASE(f5a_js),
    EFX_CASE(f5b_js),
    EFX_CASE(resource_js),
    EFX_CASE(createTexture_js),
    EFX_CASE(font_js),
    EFX_CASE(gltf_js),
    EFX_CASE(skin_js),
    EFX_CASE(pose_js),
    EFX_CASE(repl_eval),
    EFX_CASE(input_js),
    EFX_CASE(gamepad_js),
    EFX_CASE(module_js),
    EFX_CASE(module_hooks_js),
    EFX_CASE(billboard_js),
    EFX_CASE(particles_js),
    EFX_CASE(sprites_js),
    EFX_CASE(physics_js),
    EFX_CASE(audio_js),
    EFX_CASE(destroy_no_pending_exception),
    EFX_CASE(white_sinkless),
};

int main(int argc, char **argv) {
    return efx_test_main(cases, sizeof(cases) / sizeof(cases[0]), argc, argv);
}
