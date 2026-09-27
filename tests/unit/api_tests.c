/*
 * Headless JS-API tests for the F2 2D layer: installs a mock GPU sink,
 * runs the real quickjs runtime + api bindings, and asserts semantics by
 * driving JS snippets and inspecting the display list from C.
 * Usage: efx_api_tests <case> ; exit 0 = pass.
 */
#include "render/render.h"
#include "resource/resource.h"
#include "runtime/runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef EFX_RES_FIXTURES
#define EFX_RES_FIXTURES "tests/fixtures/resource"
#endif

static int fail(const char *what) {
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

#define GLTF_SEQ_MAX 8
static int g_seq_wrap[GLTF_SEQ_MAX];
static int g_seq_filter[GLTF_SEQ_MAX];
static int g_seq_n;

static void *mock_create(void *ud, int w, int h, const uint8_t *rgba,
                         int wrap, int filter) {
    (void)ud; (void)rgba;
    if (g_seq_n < GLTF_SEQ_MAX) {
        g_seq_wrap[g_seq_n] = wrap;
        g_seq_filter[g_seq_n] = filter;
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

static int feq(float a, float b) {
    return (a - b) < 0.001f && (b - a) < 0.001f;
}

/* white texture: exists, stable identity, destroy() throws */
static int white(void) {
    if (ok_js("const a = efx.whiteTexture; const b = efx.whiteTexture; if (a !== b) throw new Error('identity');"
              "try { a.destroy(); throw new Error('no'); } catch (e) { if (!(e instanceof TypeError)) throw e; }")) {
        end_js();
        return fail("white texture identity/destroy");
    }
    end_js();
    return 0;
}

/* end-to-end: JS camera + quad options land in the composed record */
static int quad_record(void) {
    const char *code =
        "efx.setCamera2D({ frame: [640, 480], x: 320, y: 240, zoom: 2, rotation: 0 });"
        "efx.drawQuad(0, 0, efx.whiteTexture,"
        "  { rotation: 90, scale: 1.5, color: [1, 0, 0, 1], size: [64, 32],"
        "    sourceRect: { x: 0, y: 0, w: 1, h: 1 } });";
    if (ok_js(code)) {
        end_js();
        return fail("snippet");
    }
    if (rec_count() != 1) {
        end_js();
        return fail("record count");
    }
    efx_camera2d cam = {640, 480, 320, 240, 2, 0};
    efx_affine expect = efx_affine_mul(efx_camera_matrix(&cam, 640, 480),
                                       efx_quad_matrix(0, 0, 32, 16, 90, 1.5f));
    const efx_record *r = efx_render_records(NULL);
    if (!feq(r[0].u.quad.m.a, expect.a) || !feq(r[0].u.quad.m.tx, expect.tx) ||
        !feq(r[0].u.quad.m.ty, expect.ty)) {
        end_js();
        return fail("composed transform mismatch");
    }
    if (!feq(r[0].u.quad.tw, 1) || !feq(r[0].u.quad.sw, 1)) {
        end_js();
        return fail("source rect/texture size");
    }
    if (!feq(r[0].u.quad.w, 64) || !feq(r[0].u.quad.h, 32)) {
        end_js();
        return fail("explicit size overrides derivation");
    }
    if (!feq(r[0].u.quad.color[0], 1) || !feq(r[0].u.quad.color[1], 0) || !feq(r[0].u.quad.color[3], 1)) {
        end_js();
        return fail("tint");
    }
    if (r[0].u.quad.blend != EFX_BLEND_ALPHA) {
        end_js();
        return fail("blend");
    }
    end_js();
    return 0;
}

/* size derivation: explicit size -> sourceRect extent -> texture pixels;
 * scale applies after the size is determined */
static int size_derivation(void) {
    const char *code =
        "const img = efx.createImageData({ width: 64, height: 32, pixels: new Uint8Array(64 * 32 * 4) });"
        "const tex = efx.createTexture(img);"
        "efx.drawQuad(0, 0, tex);"                                                    /* texture pixels */
        "efx.drawQuad(0, 0, tex, { sourceRect: { x: 0, y: 0, w: 8, h: 4 } });"        /* src extent */
        "efx.drawQuad(0, 0, tex, { sourceRect: { x: 0, y: 0, w: 8, h: 4 }, size: [50, 20] });"
        "efx.drawQuad(0, 0, tex, { size: [32, 16], scale: 2 });";                     /* scale after size */
    if (ok_js(code)) {
        end_js();
        return fail("snippet");
    }
    const efx_record *r = efx_render_records(NULL);
    if (rec_count() != 4) {
        end_js();
        return fail("record count");
    }
    if (!feq(r[0].u.quad.w, 64) || !feq(r[0].u.quad.h, 32)) {
        end_js();
        return fail("derive from texture pixels");
    }
    if (!feq(r[1].u.quad.w, 8) || !feq(r[1].u.quad.h, 4)) {
        end_js();
        return fail("derive from sourceRect");
    }
    if (!feq(r[2].u.quad.w, 50) || !feq(r[2].u.quad.h, 20)) {
        end_js();
        return fail("explicit size overrides sourceRect");
    }
    /* scale 2 around the (default center) pivot: matrix a-component = 2 */
    if (!feq(r[3].u.quad.w, 32) || !feq(r[3].u.quad.h, 16) || !feq(r[3].u.quad.m.a, 2)) {
        end_js();
        return fail("scale applies after size");
    }
    end_js();
    return 0;
}

/* origin: pivot point in quad-local pixels; placement unchanged without
 * rotation/scale; rotation around origin [0,0] fixes the top-left corner */
static int origin_pivot(void) {
    const char *code =
        "const img = efx.createImageData({ width: 64, height: 32, pixels: new Uint8Array(64 * 32 * 4) });"
        "const tex = efx.createTexture(img);"
        "efx.drawQuad(10, 20, tex);"
        "efx.drawQuad(10, 20, tex, { origin: [50, 100] });"              /* no transform: same */
        "efx.drawQuad(10, 20, tex, { origin: [0, 0], rotation: 90 });";  /* pivot at top-left */
    if (ok_js(code)) {
        end_js();
        return fail("snippet");
    }
    const efx_record *r = efx_render_records(NULL);
    if (rec_count() != 3) {
        end_js();
        return fail("record count");
    }
    /* untransformed: origin must not move the quad */
    if (!feq(r[0].u.quad.m.tx, r[1].u.quad.m.tx) || !feq(r[0].u.quad.m.ty, r[1].u.quad.m.ty) ||
        !feq(r[0].u.quad.m.a, r[1].u.quad.m.a)) {
        end_js();
        return fail("origin must not move an untransformed quad");
    }
    /* origin [0,0] + rotation 90 (y-down, clockwise): local (0,0) maps to
     * (10, 20) and local (64, 0) maps to (10, 20 + 64) */
    if (!feq(r[2].u.quad.m.a + r[2].u.quad.m.c * 0 + r[2].u.quad.m.tx, 10) ||
        !feq(r[2].u.quad.m.b * 0 + r[2].u.quad.m.d * 0 + r[2].u.quad.m.ty, 20)) {
        end_js();
        return fail("origin pivot corner position");
    }
    if (!feq(r[2].u.quad.m.a * 64 + r[2].u.quad.m.tx, 10) || !feq(r[2].u.quad.m.b * 64 + r[2].u.quad.m.ty, 84)) {
        end_js();
        return fail("origin pivot rotation direction");
    }
    end_js();
    return 0;
}

/* validation matrix for size/origin/zero-extent sourceRect */
static int quad_validation(void) {
    const char *code =
        "function t(fn, kind) {"
        "  try { fn(); throw new Error('did not throw'); }"
        "  catch (e) {"
        "    if (e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;"
        "    if (!(e instanceof kind)) throw new Error('wrong kind: ' + e);"
        "  }"
        "}"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { size: [0, 10] }), RangeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { size: [10] }), RangeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { size: 'big' }), TypeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { origin: [NaN, 0] }), RangeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { origin: 'center' }), TypeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture,"
        "  { sourceRect: { x: 0, y: 0, w: 0, h: 1 } }), RangeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { size: [4, 4], frobnicate: 1 }), TypeError);";
    if (ok_js(code)) {
        end_js();
        return fail("quad validation matrix");
    }
    if (rec_count() != 0) {
        end_js();
        return fail("failed calls must record nothing");
    }
    end_js();
    return 0;
}

/* Texture width/height getters: values, whiteTexture, destroyed throws */
static int texture_size_getters(void) {
    const char *code =
        "const img = efx.createImageData({ width: 64, height: 32, pixels: new Uint8Array(64 * 32 * 4) });"
        "const tex = efx.createTexture(img);"
        "if (tex.width !== 64 || tex.height !== 32) throw new Error('texture size');"
        "if (efx.whiteTexture.width !== 1 || efx.whiteTexture.height !== 1)"
        "  throw new Error('white texture size');"
        "tex.destroy();"
        "try { tex.width; throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }"
        "try { efx.drawQuad(0, 0, tex); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }";
    if (ok_js(code)) {
        end_js();
        return fail("texture size getters");
    }
    end_js();
    return 0;
}

/* camera is snapshotted per record */
static int camera_snapshot(void) {
    const char *code =
        "efx.setCamera2D({ frame: [640, 480] });"
        "efx.drawQuad(100, 0, efx.whiteTexture, { size: [8, 8] });"
        "efx.setCamera2D({ frame: [640, 480], x: 370, y: 0 });"
        "efx.drawQuad(100, 0, efx.whiteTexture, { size: [8, 8] });";
    if (ok_js(code)) {
        end_js();
        return fail("snippet");
    }
    const efx_record *r = efx_render_records(NULL);
    if (r[0].u.quad.m.tx == r[1].u.quad.m.tx) {
        end_js();
        return fail("camera not snapshotted");
    }
    /* second view looks 50 world px right of the first: at zoom 1 the
       recorded quad shifts 50 frame px left (world moves right on screen) */
    if (!feq(r[1].u.quad.m.tx - r[0].u.quad.m.tx, -50.0f)) {
        end_js();
        return fail("camera delta");
    }
    end_js();
    return 0;
}

/* out-of-bounds sourceRect throws RangeError */
static int src_oob(void) {
    if (err_js("efx.drawQuad(0, 0, efx.whiteTexture,"
               "  { sourceRect: { x: 0, y: 0, w: 5, h: 5 } });", "oob sourceRect")) {
        end_js();
        return fail("oob sourceRect must throw");
    }
    end_js();
    return 0;
}

/* record budget surfaces as a thrown error from JS */
static int budget(void) {
    const char *code =
        "try {"
        "  for (let i = 0; i < 500000; i++) efx.drawQuad(0, 0, efx.whiteTexture);"
        "  throw new Error('budget not enforced');"
        "} catch (e) { if (!(e instanceof RangeError)) throw e; }";
    if (ok_js(code)) {
        end_js();
        return fail("budget RangeError");
    }
    end_js();
    return 0;
}

/* texture resource lifecycle at the JS level */
static int texture_lifecycle(void) {
    const char *code =
        "const img = efx.createImageData({ width: 2, height: 2, pixels: new Uint8Array(16) });"
        "const tex = efx.createTexture(img);"
        "tex.destroy();"
        "tex.destroy();" /* idempotent */
        "try { efx.drawQuad(0, 0, tex); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }";
    if (ok_js(code)) {
        end_js();
        return fail("texture lifecycle");
    }
    end_js();
    return 0;
}

/* blend snapshot at the JS level */
static int blend_snapshot(void) {
    const char *code =
        "efx.drawQuad(0, 0, efx.whiteTexture, { size: [4, 4] });"
        "efx.setBlendMode('subtractive');"
        "efx.drawQuad(0, 0, efx.whiteTexture, { size: [4, 4] });";
    if (ok_js(code)) {
        end_js();
        return fail("snippet");
    }
    const efx_record *r = efx_render_records(NULL);
    if (r[0].u.quad.blend != EFX_BLEND_ALPHA || r[1].u.quad.blend != EFX_BLEND_SUBTRACTIVE) {
        end_js();
        return fail("blend snapshot");
    }
    end_js();
    return 0;
}

/* setClearColor stores through the JS binding */
static int clear_color_js(void) {
    if (ok_js("efx.setClearColor([0.1, 0.7, 0.3, 1]);")) {
        end_js();
        return fail("snippet");
    }
    float c[4];
    efx_render_clear_color(c);
    if (!feq(c[0], 0.1f) || !feq(c[1], 0.7f) || !feq(c[2], 0.3f) || !feq(c[3], 1.0f)) {
        end_js();
        return fail("clear color not stored");
    }
    end_js();
    return 0;
}

/* default camera: frame == viewport, identity view */
static int default_camera(void) {
    if (ok_js("efx.drawQuad(0, 0, efx.whiteTexture, { size: [4, 4] });")) {
        end_js();
        return fail("snippet");
    }
    const efx_record *r = efx_render_records(NULL);
    if (r[0].u.quad.frame_w != 1024 || r[0].u.quad.frame_h != 600 || !feq(r[0].u.quad.m.a, 1) ||
        !feq(r[0].u.quad.m.tx, 0) || !feq(r[0].u.quad.m.ty, 0)) {
        end_js();
        return fail("default camera");
    }
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
    if (ok_js(code)) {
        end_js();
        return fail("hooks snippet");
    }
    int has_update = 0;
    int has_render = 0;
    efx_runtime_pick_hooks(g_rt, &has_update, &has_render);
    if (!has_update || !has_render) {
        end_js();
        return fail("sugar hooks not picked up");
    }
    if (efx_runtime_call_hook(g_rt, 1, 0.5) != EFX_HOOK_OK ||
        efx_runtime_call_hook(g_rt, 0, 0.5) != EFX_HOOK_OK) {
        end_js();
        return fail("hook dispatch returned an error");
    }
    /* unsubscribe is idempotent and removes the first hook */
    if (efx_runtime_eval_string(g_rt, "unsub", "__off(); __off();") != 0) {
        end_js();
        return fail("unsubscribe snippet");
    }
    if (efx_runtime_call_hook(g_rt, 1, 0.25) != EFX_HOOK_OK) {
        end_js();
        return fail("post-unsubscribe dispatch");
    }
    const char *want =
        "typeerror:true|uA:true|uB|gU:true|r|gR|uB|gU:true";
    char verify[512];
    snprintf(verify, sizeof(verify),
             "if (__hooksLog.join('|') !== '%s')"
             "  throw new Error('hook order: ' + __hooksLog.join('|'));",
             want);
    if (efx_runtime_eval_string(g_rt, "verify", verify) != 0) {
        end_js();
        return fail("hook order/dt mismatch");
    }
    end_js();
    return 0;
}


/* ------------------------------------------------------------------ F3 */

/* createMeshData: batch + shorthand, surfaceCount, validation matrix */
static int meshdata_js(void) {
    const char *code =
        "const P = [0,0,0, 1,0,0, 0,1,0];"
        "const md = efx.createMeshData({"
        "  surfaces: ["
        "    { positions: P, normals: P, uvs: [0,0, 1,0, 0,1],"
        "      colors: [1,0,0,1, 0,1,0,1, 0,0,1,1], indices: [0,1,2] },"
        "    { positions: P },"
        "  ],"
        "});"
        "if (md.surfaceCount !== 2) throw new Error('surfaceCount');"
        "const one = efx.createMeshData({ positions: P, indices: [0,1,2] });"
        "if (one.surfaceCount !== 1) throw new Error('shorthand');"
        "function t(fn, kind) {"
        "  try { fn(); throw new Error('did not throw'); }"
        "  catch (e) {"
        "    if (e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;"
        "    if (!(e instanceof kind)) throw new Error('wrong kind: ' + e);"
        "  }"
        "}"
        "t(() => efx.createMeshData({}), TypeError);"
        "t(() => efx.createMeshData({ surfaces: [], positions: P }), TypeError);"
        "t(() => efx.createMeshData({ surfaces: [] }), RangeError);"
        "t(() => efx.createMeshData({ positions: [0,0,0] }), RangeError);"
        "t(() => efx.createMeshData({ positions: [0,0,0, 1,0,1] }), RangeError);"
        "t(() => efx.createMeshData({ positions: P, indices: [0,1,3] }), RangeError);"
        "t(() => efx.createMeshData({ positions: P, indices: [0,1] }), RangeError);"
        "t(() => efx.createMeshData({ positions: P, frobnicate: 1 }), TypeError);"
        "t(() => efx.createMeshData({ positions: P, materials: [] }), RangeError);"
        "t(() => efx.createMeshData({ positions: ['a',0,0, 1,0,0, 0,1,0] }), TypeError);"
        "t(() => efx.createMeshData({ positions: [NaN,0,0, 1,0,0, 0,1,0] }), RangeError);"
        "t(() => efx.createMeshData({ positions: P, normals: [0,0,1] }), RangeError);"
        "one.destroy();"
        "try { one.surfaceCount; throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }";
    if (ok_js(code)) {
        end_js();
        return fail("meshdata js");
    }
    end_js();
    return 0;
}

/* 17 surfaces: RangeError cap */
static int meshdata_cap_js(void) {
    const char *code =
        "const P = [0,0,0, 1,0,0, 0,1,0];"
        "const S = [];"
        "for (let i = 0; i < 17; i++) S.push({ positions: P, indices: [0,1,2] });"
        "try { efx.createMeshData({ surfaces: S }); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof RangeError)) throw e; }"
        "S.pop();"
        "if (efx.createMeshData({ surfaces: S }).surfaceCount !== 16)"
        "  throw new Error('16 must be accepted');";
    if (ok_js(code)) {
        end_js();
        return fail("meshdata cap");
    }
    end_js();
    return 0;
}

/* createMesh + drawMesh: upload, whole-mesh record, validation */
static int mesh_js(void) {
    const char *code =
        "const P = [0,0,0, 1,0,0, 0,1,0];"
        "const md = efx.createMeshData({ positions: P, indices: [0,1,2] });"
        "const mesh = efx.createMesh(md);"
        "if (mesh.surfaceCount !== 1) throw new Error('mesh surfaceCount');"
        "md.destroy();" /* Mesh is a copy */
        "if (mesh.surfaceCount !== 1) throw new Error('after source destroy');"
        "efx.setCamera3D({ pos: [0, 2, 5], target: [0, 0, 0], fov: 60 });"
        "efx.drawMesh({ mesh, transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3,1],"
        "  color: [0.5, 0.25, 1, 1] });"
        "function t(fn, kind) {"
        "  try { fn(); throw new Error('did not throw'); }"
        "  catch (e) {"
        "    if (e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;"
        "    if (!(e instanceof kind)) throw new Error('wrong kind: ' + e);"
        "  }"
        "}"
        "t(() => efx.drawMesh({}), TypeError);"
        "t(() => efx.drawMesh({ mesh: {} }), TypeError);"
        "t(() => efx.drawMesh({ mesh, transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3] }), RangeError);"
        "t(() => efx.drawMesh({ mesh, transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3,'x',1] }), TypeError);"
        "t(() => efx.drawMesh({ mesh, color: [1, 0, 1] }), RangeError);"
        "t(() => efx.drawMesh({ mesh, frobnicate: 1 }), TypeError);"
        "const t2 = efx.createTexture("
        "  efx.createImageData({ width: 2, height: 2, pixels: new Uint8Array(16) }));"
        "t2.destroy();"
        "efx.drawMesh({ mesh });"
        "mesh.destroy(); mesh.destroy();" /* idempotent */
        "t(() => efx.drawMesh({ mesh }), TypeError);"
        "try { mesh.surfaceCount; throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }";
    if (ok_js(code)) {
        end_js();
        return fail("mesh js");
    }
    /* records: first drawMesh with explicit args, throws record nothing,
       second with defaults */
    const efx_record *r = efx_render_records(NULL);
    int n = rec_count();
    if (n != 2) {
        end_js();
        return fail("mesh record count");
    }
    if (r[0].type != EFX_RECORD_MESH || r[1].type != EFX_RECORD_MESH) {
        end_js();
        return fail("mesh record type");
    }
    if (!feq(r[0].u.mesh.transform[12], 1) || !feq(r[0].u.mesh.transform[13], 2) ||
        !feq(r[0].u.mesh.transform[14], 3)) {
        end_js();
        return fail("mesh transform");
    }
    if (!feq(r[0].u.mesh.color[1], 0.25f)) {
        end_js();
        return fail("mesh tint");
    }
    float pos[3], target[3], fov, nearz, farz;
    efx_render_camera3d(pos, target, &fov, &nearz, &farz);
    if (!feq(r[0].u.mesh.camera.pos[2], 5) || !feq(r[0].u.mesh.camera.fov, 60)) {
        end_js();
        return fail("mesh camera snapshot");
    }
    if (!feq(r[1].u.mesh.transform[0], 1) || !feq(r[1].u.mesh.transform[12], 0)) {
        end_js();
        return fail("mesh default identity");
    }
    if (!feq(r[1].u.mesh.color[3], 1)) {
        end_js();
        return fail("mesh default tint");
    }
    end_js();
    return 0;
}

/* setCamera3D: defaults, validation, separate from the 2D camera */
static int camera3d_js(void) {
    const char *code =
        "efx.setCamera3D({ pos: [0, 1, 4], target: [0, 0, 0], fov: 90 });"
        "function t(fn, kind) {"
        "  try { fn(); throw new Error('did not throw'); }"
        "  catch (e) {"
        "    if (e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;"
        "    if (!(e instanceof kind)) throw new Error('wrong kind: ' + e);"
        "  }"
        "}"
        "t(() => efx.setCamera3D(), TypeError);"
        "t(() => efx.setCamera3D({ target: [0,0,0], fov: 60 }), TypeError);"
        "t(() => efx.setCamera3D({ pos: [0,0,0], target: [0,0,0], fov: 'wide' }), TypeError);"
        "t(() => efx.setCamera3D({ pos: [0,0,0], target: [0,0,0], fov: 60, frobnicate: 1 }), TypeError);"
        "efx.setCamera3D({ pos: [0, 0, 2], target: [0, 0, 0], fov: 45 });"
        /* defaults accepted for near/far */
        "efx.setCamera3D({ pos: [0, 0, 2], target: [0, 0, 0], fov: 45, near: 0.5, far: 50 });";
    if (ok_js(code)) {
        end_js();
        return fail("camera3d js");
    }
    float pos[3], target[3], fov, nearz, farz;
    efx_render_camera3d(pos, target, &fov, &nearz, &farz);
    if (!feq(pos[2], 2) || !feq(fov, 45)) {
        end_js();
        return fail("camera3d state");
    }
    if (!feq(nearz, 0.5f) || !feq(farz, 50.0f)) {
        end_js();
        return fail("camera3d near/far");
    }
    end_js();
    return 0;
}

/* F4a: lights + per-surface materials through the real JS runtime */
static int f4a_js(void) {
    const char *code =
        "efx.setLight(0, { pos: [3,4,2], color: [1,0.95,0.9,1], range: 20 });"
        "efx.setDirectionalLight({ dir: [-0.5,-1,-0.3], color: [0.2,0.25,0.35,1] });"
        "const P=[0,0,0, 1,0,0, 0,1,0];"
        "const M={ ambient:{color:[0.05,0.05,0.05,1]},"
        "  diffuse:{color:[0.8,0.3,0.2,1]},"
        "  specular:{color:[1,1,1,1], shininess:64},"
        "  emissive:{color:[0,0,0,1]} };"
        "const md=efx.createMeshData({"
        "  surfaces:[{positions:P, indices:[0,1,2]}], materials:[M] });"
        "const mesh=efx.createMesh(md);"
        "efx.setMeshSurfaceMaterial(mesh, 0, { diffuse:{color:[0.1,0.2,0.3,1]} });"
        "efx.setCamera3D({pos:[0,2,5], target:[0,0,0], fov:60});"
        "efx.drawMesh({ mesh });"
        "function t(fn, kind){"
        "  try{fn();throw new Error('no');}catch(e){"
        "    if(e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;"
        "    if(!(e instanceof kind)) throw new Error('wrong: '+e);"
        "  }"
        "}"
        "t(()=>efx.setLight(4,{pos:[0,0,0],color:[1,1,1,1]}), RangeError);"
        "t(()=>efx.setLight(0,{color:[1,1,1,1]}), TypeError);"
        "t(()=>efx.setLight(0,{pos:[0,0,0],color:[1,1,1,1],range:-1}), RangeError);"
        "t(()=>efx.setDirectionalLight({dir:[0,0,0],color:[1,1,1,1]}), TypeError);"
        "t(()=>efx.setMeshSurfaceMaterial(mesh, 1, M), RangeError);"
        "t(()=>efx.setMeshSurfaceMaterial(mesh, 0, {diffuse:{color:[1,1,1,1],map:1}}), TypeError);"
        "t(()=>efx.createMeshData({positions:P, materials:[]}), RangeError);"
        "t(()=>efx.createMeshData({positions:P, materials:[{specular:{color:[1,1,1,1],shininess:0}}]}), RangeError);"
        "mesh.destroy(); md.destroy();";
    if (ok_js(code)) {
        end_js();
        return fail("f4a js");
    }
    const efx_record *r = efx_render_records(NULL);
    int n = rec_count();
    if (n != 1 || r[0].type != EFX_RECORD_MESH) {
        end_js();
        return fail("f4a record");
    }
    if (!r[0].u.mesh.lights.points[0].enabled) {
        end_js();
        return fail("point light snapshot");
    }
    if (!feq(r[0].u.mesh.lights.points[0].range, 20)) {
        end_js();
        return fail("light range snapshot");
    }
    if (!r[0].u.mesh.lights.directional.enabled) {
        end_js();
        return fail("directional snapshot");
    }
    end_js();
    return 0;
}

/* F4b: per-channel maps + alphaMask parse, validation, and retention across
 * a destroyed-but-bound texture (desktop binding) */
static int f4b_js(void) {
    const char *code =
        "const img=efx.createImageData({width:1,height:1,"
        "  pixels:new Uint8Array([255,255,255,255])});"
        "const tex=efx.createTexture(img);"
        "const P=[0,0,0, 1,0,0, 0,1,0];"
        "const M={ diffuse:{color:[0.8,0.8,0.8,1], map:tex},"
        "  specular:{color:[1,1,1,1], shininess:32, map:tex},"
        "  alphaMask:tex };"
        "const md=efx.createMeshData({"
        "  surfaces:[{positions:P, uvs:[0,0, 1,0, 0,1], indices:[0,1,2]}],"
        "  materials:[M] });"
        "const mesh=efx.createMesh(md);"
        "efx.setCamera3D({pos:[0,0,5], target:[0,0,0], fov:60});"
        "efx.drawMesh({ mesh });"
        "tex.destroy();"                 /* retained by the bound map */
        "efx.drawMesh({ mesh });"        /* still renders (no throw) */
        "function t(fn,kind){"
        "  try{fn();throw new Error('no');}catch(e){"
        "    if(e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;"
        "    if(!(e instanceof kind)) throw new Error('wrong: '+e);"
        "  }"
        "}"
        "t(()=>efx.setMeshSurfaceMaterial(mesh,0,{diffuse:{color:[1,1,1,1],map:tex}}), TypeError);"
        "t(()=>efx.setMeshSurfaceMaterial(mesh,0,{diffuse:{color:[1,1,1,1],map:1}}), TypeError);"
        "t(()=>efx.setMeshSurfaceMaterial(mesh,0,{alphaMask:5}), TypeError);"
        "t(()=>efx.setMeshSurfaceMaterial(mesh,0,{diffuse:{color:[1,1,1,1],frob:1}}), TypeError);"
        "efx.setMeshSurfaceMaterial(mesh,0,{diffuse:{color:[1,1,1,1]}});" /* release */
        "mesh.destroy(); md.destroy();";
    if (ok_js(code)) {
        end_js();
        return fail("f4b js");
    }
    if (rec_count() < 2) {
        end_js();
        return fail("f4b records");
    }
    end_js();
    return 0;
}

/* F5a: render targets through the JS bindings — lifecycle, validation,
 * redirection errors, texture coercion in drawQuad and material maps */
static int f5a_js(void) {
    const char *code =
        "function t(fn,kind){"
        "  try{fn();throw new Error('no');}catch(e){"
        "    if(e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;"
        "    if(!(e instanceof kind)) throw new Error('wrong: '+e);"
        "  }"
        "}"
        /* validation matrix */
        "t(()=>efx.createRenderTarget({height:8}), TypeError);"
        "t(()=>efx.createRenderTarget({width:0,height:8}), RangeError);"
        "t(()=>efx.createRenderTarget({width:8,height:10.5}), RangeError);"
        "t(()=>efx.createRenderTarget({width:8,height:4097}), RangeError);"
        "t(()=>efx.createRenderTarget({width:8,height:8,frob:1}), TypeError);"
        /* lifecycle + query properties */
        "const rt=efx.createRenderTarget({width:256,height:128});"
        "if(rt.width!==256||rt.height!==128) throw new Error('size');"
        "rt.destroy(); rt.destroy();"
        "t(()=>rt.width, TypeError);"
        /* coercion: an RT drives drawQuad size derivation + sourceRect */
        "const live=efx.createRenderTarget({width:64,height:32});"
        "efx.drawQuad(0,0,live);"
        "efx.drawQuad(0,0,live,{sourceRect:{x:0,y:0,w:16,h:16}});"
        "t(()=>efx.drawQuad(0,0,live,{sourceRect:{x:0,y:0,w:65,h:4}}), RangeError);"
        "t(()=>efx.drawQuad(0,0,{}), TypeError);"
        /* redirection: records land, nesting/balance throw */
        "efx.beginRenderTarget(live);"
        "efx.drawQuad(0,0,efx.whiteTexture);"
        "t(()=>efx.beginRenderTarget(live), TypeError);"
        "t(()=>efx.drawQuad(0,0,live), TypeError);" /* feedback */
        "efx.endRenderTarget();"
        "t(()=>efx.endRenderTarget(), TypeError);"
        "t(()=>efx.beginRenderTarget({}), TypeError);"
        /* material maps accept a live RT, reject a destroyed one */
        "const mesh=efx.createMesh(efx.createMeshData({"
        "  positions:[0,0,0, 1,0,0, 0,1,0], uvs:[0,0, 1,0, 0,1], indices:[0,1,2]}));"
        "efx.setCamera3D({pos:[0,0,5],target:[0,0,0],fov:60});"
        "efx.setMeshSurfaceMaterial(mesh,0,{diffuse:{color:[1,1,1,1],map:live}});"
        "efx.drawMesh({mesh});"
        "efx.beginRenderTarget(live);"
        "t(()=>efx.drawMesh({mesh}), TypeError);" /* mesh feedback */
        "efx.endRenderTarget();"
        "const dead=efx.createRenderTarget({width:8,height:8});"
        "dead.destroy();"
        "t(()=>efx.setMeshSurfaceMaterial(mesh,0,{diffuse:{color:[1,1,1,1],map:dead}}), TypeError);"
        "t(()=>efx.beginRenderTarget(dead), TypeError);"
        "mesh.destroy(); live.destroy();";
    if (ok_js(code)) {
        end_js();
        return fail("f5a js");
    }
    /* records: drawQuad(rt) x2, BEGIN, white quad, END, mesh, BEGIN, END */
    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    if (count < 6) {
        end_js();
        return fail("f5a record count");
    }
    /* size derivation from the target extent (64x32) */
    if (!feq(recs[0].u.quad.w, 64) || !feq(recs[0].u.quad.h, 32)) {
        end_js();
        return fail("rt size derivation");
    }
    if (!feq(recs[1].u.quad.w, 16) || !feq(recs[1].u.quad.h, 16)) {
        end_js();
        return fail("src extent derivation");
    }
    /* each BEGIN record snapshots the frame's clear color (default black) */
    int begins = 0;
    for (int i = 0; i < count; i++) {
        if (recs[i].type == EFX_RECORD_BEGIN_TARGET) {
            if (!feq(recs[i].u.begin_target.clear[3], 1.0f)) {
                end_js();
                return fail("clear snapshot alpha");
            }
            begins++;
        }
    }
    if (begins != 2) {
        end_js();
        return fail("begin record count");
    }
    end_js();
    return 0;
}

static int f5b_js(void) {
    const char *matrix =
        "function t(fn,kind){"
        "  try{fn();throw new Error('no');}catch(e){"
        "    if(e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;"
        "    if(!(e instanceof kind)) throw new Error('wrong: '+e);"
        "  }"
        "}"
        "t(()=>efx.setPostEffects('x'), TypeError);"
        "t(()=>efx.setPostEffects([1]), TypeError);"
        "t(()=>efx.setPostEffects([{effect:'vortex'}]), TypeError);"
        "t(()=>efx.setPostEffects([{effect:'blur',radius:0}]), RangeError);"
        "t(()=>efx.setPostEffects([{effect:'blur',radius:65}]), RangeError);"
        "t(()=>efx.setPostEffects([{effect:'blur',radius:'x'}]), TypeError);"
        "t(()=>efx.setPostEffects([{effect:'blur',frob:1}]), TypeError);"
        "t(()=>efx.setPostEffects([{effect:'bloom',strength:1.5}]), RangeError);"
        "t(()=>efx.setPostEffects([{effect:'colorFilter',tint:[1,1]}]), RangeError);"
        "t(()=>efx.setPostEffects([{effect:'blur',mix:2}]), RangeError);"
        "t(()=>efx.setPostEffects(new Array(9).fill({effect:'blur'})), RangeError);"
        "t(()=>efx.setRenderScale(0), RangeError);"
        "t(()=>efx.setRenderScale(2.5), RangeError);"
        "t(()=>efx.setRenderScale('x'), TypeError);"
        "t(()=>efx.setRenderScale(1,{filter:'bogus'}), TypeError);"
        "t(()=>efx.setRenderScale(1,{frob:1}), TypeError);"
        /* atomicity: a failed set leaves the previous chain */
        "efx.setPostEffects([{effect:'blur',radius:5,mix:0.25}]);"
        "t(()=>efx.setPostEffects([{effect:'nope'}]), TypeError);"
        "t(()=>efx.setPostEffects([{effect:'blur',radius:0}]), RangeError);"
        /* snapshot: later mutation of the entry must not change the chain */
        "const entry={effect:'blur',radius:3,mix:0.5};"
        "efx.setPostEffects([entry]);"
        "entry.radius=60; entry.mix=0.1; entry.effect='bloom';";
    if (ok_js(matrix)) {
        end_js();
        return fail("f5b js matrix");
    }
    efx_post_entry got[EFX_POST_MAX_ENTRIES];
    int n = 0;
    efx_render_post_effects(got, &n);
    if (n != 1 || got[0].effect != EFX_POST_BLUR)
        return fail("chain atomicity");
    if (!feq(got[0].u.blur.radius, 3.0f) || !feq(got[0].mix, 0.5f))
        return fail("chain snapshot");
    end_js();

    /* defaults are neutral and the chain persists across frames */
    if (ok_js("efx.setPostEffects([{effect:'colorFilter'}]);")) {
        end_js();
        return fail("f5b js defaults");
    }
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
    if (ok_js("efx.setPostEffects([{effect:'bloom'}]);efx.setPostEffects(null);")) {
        end_js();
        return fail("f5b js null clear");
    }
    efx_render_post_effects(got, &n);
    if (n != 0) return fail("null did not clear");
    end_js();
    if (ok_js("efx.setPostEffects([{effect:'bloom'}]);efx.setPostEffects([]);")) {
        end_js();
        return fail("f5b js empty clear");
    }
    efx_render_post_effects(got, &n);
    if (n != 0) return fail("[] did not clear");
    end_js();

    /* render scale persists and a rejected call leaves it in effect */
    if (ok_js("efx.setRenderScale(0.5,{filter:'nearest'});"
              "try{efx.setRenderScale(0);}catch(e){}")) {
        end_js();
        return fail("f5b js scale");
    }
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
    if (!res) {
        end_js();
        return fail("open fixtures");
    }
    efx_runtime_set_resource(g_rt, res);
    int rc = efx_runtime_eval_string(g_rt, "test",
        "if (efx.loadText('hello.txt') !== 'hello efx\\n') throw new Error('text');"
        "var img = efx.loadImage('test_rgba.png');"
        "if (img.width !== 3 || img.height !== 2) throw new Error('dims');"
        "var px = efx.createTexture(img);"
        "if (px.width !== 3 || px.height !== 2) throw new Error('tex dims');"
        "var lt = efx.loadTexture('test_rgba.png');"
        "if (lt.width !== 3 || lt.height !== 2) throw new Error('loadTexture dims');"
        "img.destroy(); px.destroy(); lt.destroy();"
        "var e1 = 0; try { efx.loadText('nope.txt'); } catch (e) {"
        "  e1 = (e instanceof Error) ? 1 : 2; }"
        "if (e1 !== 1) throw new Error('missing not Error ('+e1+')');"
        "var e2 = 0; try { efx.loadText(5); } catch (e) {"
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

/* F6b: createTexture sampler options reach the native texture create */
static int createTexture_js(void) {
    const char *code =
        "var img = efx.createImageData({ width: 1, height: 1,"
        "  pixels: new Uint8Array([1, 2, 3, 4]) });"
        "efx.createTexture(img);"
        "efx.createTexture(img, { wrap: 'clamp', filter: 'nearest' });"
        "efx.createTexture(img, { wrap: 'mirror', filter: 'nearest' });"
        "function boom(fn) { try { fn(); } catch (e) {"
        "  return (e instanceof TypeError) ? 1 : 2; } return 0; }"
        "if (boom(function () { efx.createTexture(img, { wrap: 'bogus' }); }) !== 1)"
        "  throw new Error('bad wrap');"
        "if (boom(function () { efx.createTexture(img, { filter: 'bogus' }); }) !== 1)"
        "  throw new Error('bad filter');"
        "if (boom(function () { efx.createTexture(img, { nope: 1 }); }) !== 1)"
        "  throw new Error('unknown field');";
    g_seq_n = 0;
    if (ok_js(code)) {
        end_js();
        return fail("createTexture options snippet");
    }
    int ok = g_seq_n == 3 &&
             g_seq_wrap[0] == EFX_TEX_WRAP_REPEAT &&
             g_seq_filter[0] == EFX_FILTER_LINEAR &&
             g_seq_wrap[1] == EFX_TEX_WRAP_CLAMP &&
             g_seq_filter[1] == EFX_FILTER_NEAREST &&
             g_seq_wrap[2] == EFX_TEX_WRAP_MIRROR &&
             g_seq_filter[2] == EFX_FILTER_NEAREST;
    end_js();
    return ok ? 0 : fail("createTexture sampler option mapping");
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
    if (!res) {
        end_js();
        return fail("open gltf fixtures");
    }
    efx_runtime_set_resource(g_rt, res);
    int rc = efx_runtime_eval_string(g_rt, "test",
        "var md = efx.loadMeshData('triangle.gltf');"
        "if (!(md instanceof Object) || md.surfaceCount !== 1) throw new Error('tri surfaceCount');"
        "var mesh = efx.createMesh(md);"
        "if (mesh.surfaceCount !== 1) throw new Error('mesh surfaceCount');"
        "mesh.destroy(); md.destroy();"
        "var q = efx.loadMeshData('quad.glb', { mesh: 'm' });"
        "if (q.surfaceCount !== 2) throw new Error('quad surfaceCount');"
        "q.destroy();"
        "var q2 = efx.loadMeshData('quad.glb', { mesh: 0 });"
        "if (q2.surfaceCount !== 2) throw new Error('quad index select');"
        "q2.destroy();"
        "function kind(fn) { try { fn(); } catch (e) {"
        "  if (e instanceof TypeError) return 'TypeError';"
        "  if (e instanceof Error) return 'Error';"
        "  return 'other'; } return 'none'; }"
        "if (kind(function () { efx.loadMeshData('corrupt.gltf'); }) !== 'Error')"
        "  throw new Error('corrupt not Error');"
        "if (kind(function () { efx.loadMeshData('triangle.gltf', { mesh: 'nope' }); }) !== 'Error')"
        "  throw new Error('unknown mesh not Error');"
        "if (kind(function () { efx.loadMeshData('triangle.gltf', { nope: 1 }); }) !== 'TypeError')"
        "  throw new Error('unknown field not TypeError');"
        "if (kind(function () { efx.loadMeshData('triangle.gltf', { mesh: {} }); }) !== 'TypeError')"
        "  throw new Error('bad mesh type not TypeError');"
        "if (kind(function () { efx.loadMeshData(5); }) !== 'TypeError')"
        "  throw new Error('bad path not TypeError');");
    efx_runtime_destroy(g_rt);
    g_rt = NULL;
    efx_render_end_frame();
    efx_render_shutdown();
    efx_resource_close(res);
    if (rc != 0) return fail("gltf js snippet raised");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: efx_api_tests <case>\n");
        return 2;
    }
    const char *c = argv[1];
    if (!strcmp(c, "white")) return white();
    if (!strcmp(c, "quad_record")) return quad_record();
    if (!strcmp(c, "size_derivation")) return size_derivation();
    if (!strcmp(c, "origin_pivot")) return origin_pivot();
    if (!strcmp(c, "quad_validation")) return quad_validation();
    if (!strcmp(c, "texture_size_getters")) return texture_size_getters();
    if (!strcmp(c, "camera_snapshot")) return camera_snapshot();
    if (!strcmp(c, "src_oob")) return src_oob();
    if (!strcmp(c, "budget")) return budget();
    if (!strcmp(c, "texture_lifecycle")) return texture_lifecycle();
    if (!strcmp(c, "blend_snapshot")) return blend_snapshot();
    if (!strcmp(c, "default_camera")) return default_camera();
    if (!strcmp(c, "clear_color_js")) return clear_color_js();
    if (!strcmp(c, "hooks_registration")) return hooks_registration();
    if (!strcmp(c, "meshdata_js")) return meshdata_js();
    if (!strcmp(c, "meshdata_cap_js")) return meshdata_cap_js();
    if (!strcmp(c, "mesh_js")) return mesh_js();
    if (!strcmp(c, "camera3d_js")) return camera3d_js();
    if (!strcmp(c, "f4a_js")) return f4a_js();
    if (!strcmp(c, "f4b_js")) return f4b_js();
    if (!strcmp(c, "f5a_js")) return f5a_js();
    if (!strcmp(c, "f5b_js")) return f5b_js();
    if (!strcmp(c, "resource_js")) return resource_js();
    if (!strcmp(c, "createTexture_js")) return createTexture_js();
    if (!strcmp(c, "gltf_js")) return gltf_js();
    fprintf(stderr, "unknown case: %s\n", c);
    return 2;
}
