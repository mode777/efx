/*
 * Headless JS-API tests for the F2 2D layer: installs a mock GPU sink,
 * runs the real quickjs runtime + api bindings, and asserts semantics by
 * driving JS snippets and inspecting the display list from C.
 * Usage: efx_api_tests <case> ; exit 0 = pass.
 */
#include "render/render.h"
#include "resource/resource.h"
#include "runtime/runtime.h"
#include "input/efx_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef EFX_RES_FIXTURES
#define EFX_RES_FIXTURES "tests/fixtures/resource"
#endif
#ifndef EFX_MOD_FIXTURES
#define EFX_MOD_FIXTURES "tests/fixtures/modules"
#endif
#ifndef EFX_MOD_ERROR_FIXTURES
#define EFX_MOD_ERROR_FIXTURES "tests/fixtures/modules_error"
#endif

static int fail(const char *what) {
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

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
        "efx.drawMesh(mesh, { transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3,1],"
        "  color: [0.5, 0.25, 1, 1] });"
        "function t(fn, kind) {"
        "  try { fn(); throw new Error('did not throw'); }"
        "  catch (e) {"
        "    if (e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;"
        "    if (!(e instanceof kind)) throw new Error('wrong kind: ' + e);"
        "  }"
        "}"
        "t(() => efx.drawMesh(), TypeError);"
        "t(() => efx.drawMesh({}), TypeError);"
        "t(() => efx.drawMesh(null), TypeError);"
        "t(() => efx.drawMesh(mesh, 5), TypeError);"
        "t(() => efx.drawMesh(mesh, { mesh }), TypeError);"
        "t(() => efx.drawMesh(mesh, { transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3] }), RangeError);"
        "t(() => efx.drawMesh(mesh, { transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3,'x',1] }), TypeError);"
        "t(() => efx.drawMesh(mesh, { color: [1, 0, 1] }), RangeError);"
        "t(() => efx.drawMesh(mesh, { frobnicate: 1 }), TypeError);"
        "const t2 = efx.createTexture("
        "  efx.createImageData({ width: 2, height: 2, pixels: new Uint8Array(16) }));"
        "t2.destroy();"
        "efx.drawMesh(mesh);"
        "mesh.destroy(); mesh.destroy();" /* idempotent */
        "t(() => efx.drawMesh(mesh), TypeError);"
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
        "efx.drawMesh(mesh);"
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
        "efx.drawMesh(mesh);"
        "tex.destroy();"                 /* retained by the bound map */
        "efx.drawMesh(mesh);"        /* still renders (no throw) */
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
        "efx.drawMesh(mesh);"
        "efx.beginRenderTarget(live);"
        "t(()=>efx.drawMesh(mesh), TypeError);" /* mesh feedback */
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
        "var lt = efx.createTexture(efx.loadImage('test_rgba.png'));"
        "if (lt.width !== 3 || lt.height !== 2) throw new Error('composed tex dims');"
        "if (typeof efx.loadTexture !== 'undefined') throw new Error('loadTexture still present');"
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

/* F6b/F6e: createTexture sampler + mipmap options reach the native create */
static int createTexture_js(void) {
    const char *code =
        "var img = efx.createImageData({ width: 1, height: 1,"
        "  pixels: new Uint8Array([1, 2, 3, 4]) });"
        "efx.createTexture(img);"
        "efx.createTexture(img, { wrap: 'clamp', filter: 'nearest' });"
        "efx.createTexture(img, { wrap: 'mirror', filter: 'nearest' });"
        "efx.createTexture(img, { mipmaps: true });"
        "efx.createTexture(img, { mipmaps: false, filter: 'nearest' });"
        "function boom(fn) { try { fn(); } catch (e) {"
        "  return (e instanceof TypeError) ? 1 : 2; } return 0; }"
        "if (boom(function () { efx.createTexture(img, { wrap: 'bogus' }); }) !== 1)"
        "  throw new Error('bad wrap');"
        "if (boom(function () { efx.createTexture(img, { filter: 'bogus' }); }) !== 1)"
        "  throw new Error('bad filter');"
        "if (boom(function () { efx.createTexture(img, { nope: 1 }); }) !== 1)"
        "  throw new Error('unknown field');"
        "if (boom(function () { efx.createTexture(img, { mipmaps: 'yes' }); }) !== 1)"
        "  throw new Error('non-boolean mipmaps');";
    g_seq_n = 0;
    if (ok_js(code)) {
        end_js();
        return fail("createTexture options snippet");
    }
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
    if (!res) {
        end_js();
        return fail("open fixtures");
    }
    efx_runtime_set_resource(g_rt, res);
    int rc = efx_runtime_eval_string(g_rt, "test",
        "var fd = efx.loadFontData('font.ttf');"
        "var font = efx.createFont(fd, { size: 32 });"
        "if (font.size !== 32) throw new Error('size');"
        "if (!(font.lineHeight > 0)) throw new Error('lineHeight');"
        "if (!(font.ascent > 0)) throw new Error('ascent');"
        "if (!(font.descent < 0)) throw new Error('descent');"
        "var b = efx.measureText('hello world', font);"
        "if (!(b.width > 0) || b.lines !== 1) throw new Error('measure');"
        "var bw = efx.measureText('hello world', font, { width: 40 });"
        "if (bw.lines < 2) throw new Error('wrap lines');"
        "var bd = efx.drawText('AB', font, 10, 10, { color: [1, 0, 0, 1] });"
        "if (bd.lines !== 1) throw new Error('draw bounds');"
        "var fx = efx.createFont(fd, { size: 24, outline: { width: 2 },"
        "  shadow: { blur: 2, offset: [2, 2] } });"
        "efx.drawText('Hi', fx, 0, 0, { align: 'center',"
        "  outlineColor: [0, 0, 0, 1], shadowColor: [0, 0, 0, 1] });"
        "function boom(fn) { try { fn(); } catch (e) {"
        "  return e && e.constructor ? e.constructor.name : 'Error'; }"
        "  return 'none'; }"
        "if (boom(function () { efx.createFont(fd, {}); }) !== 'TypeError')"
        "  throw new Error('missing size');"
        "if (boom(function () { efx.createFont(fd, { size: 0 }); }) !== 'RangeError')"
        "  throw new Error('size 0');"
        "if (boom(function () { efx.createFont(fd, { size: 16, nope: 1 }); })"
        "    !== 'TypeError') throw new Error('unknown option');"
        "if (boom(function () { efx.createFont(fd, { size: 16,"
        "    outline: { width: 0 } }); }) !== 'RangeError')"
        "  throw new Error('outline width');"
        "if (boom(function () { efx.drawText('x', font, 0, 0,"
        "    { align: 'justify' }); }) !== 'TypeError')"
        "  throw new Error('justify without width');"
        "if (boom(function () { efx.measureText('x', font,"
        "    { align: 'bogus' }); }) !== 'TypeError')"
        "  throw new Error('bad align');"
        "if (boom(function () { efx.drawText('x', {}, 0, 0); }) !== 'TypeError')"
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

/* F6c: skinned surface attributes accepted through createMeshData; the rig
 * itself stays opaque (no clip/joint query property, no new API) */
static int skin_js(void) {
    const char *code =
        "const P = [0,0,0, 1,0,0, 0,1,0];"
        "const J = [0,1,2,0, 1,0,0,0, 0,0,0,0];"
        "const W = [1,0,0,0, 0.5,0.5,0,0, 1,0,0,0];"
        "const md = efx.createMeshData({ positions: P, joints: J, weights: W,"
        "  indices: [0,1,2] });"
        "if (md.surfaceCount !== 1) throw new Error('skinned surfaceCount');"
        "if (md.joints !== undefined || md.weights !== undefined)"
        "  throw new Error('rig must be opaque');"
        "if (md.clips !== undefined || md.jointCount !== undefined ||"
        "    md.clipCount !== undefined || md.skeleton !== undefined)"
        "  throw new Error('no rig query property');"
        "const mesh = efx.createMesh(md);"
        "if (mesh.clips !== undefined || mesh.jointCount !== undefined ||"
        "    mesh.skeleton !== undefined)"
        "  throw new Error('no rig query property on Mesh');"
        "if (typeof efx.poseMesh !== 'function')"
        "  throw new Error('poseMesh missing in F7');"
        "if (efx.playAnimation !== undefined || efx.pauseAnimation !== undefined ||"
        "    efx.blendAnimations !== undefined)"
        "  throw new Error('no playback helper');"
        "md.destroy(); mesh.destroy();"
        "function t(fn, kind) {"
        "  try { fn(); throw new Error('did not throw'); }"
        "  catch (e) {"
        "    if (e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;"
        "    if (!(e instanceof kind)) throw new Error('wrong kind: ' + e);"
        "  }"
        "}"
        "t(() => efx.createMeshData({ positions: P, joints: J }), RangeError);"
        "t(() => efx.createMeshData({ positions: P, weights: W }), RangeError);"
        "t(() => efx.createMeshData({ positions: P, joints: J, weights: [1,0,0,0] }), RangeError);"
        "t(() => efx.createMeshData({ positions: P, joints: [0,1,2], weights: W }), RangeError);"
        "t(() => efx.createMeshData({ positions: P, joints: ['a',0,0,0, 1,0,0,0, 0,0,0,0], weights: W }), TypeError);"
        "t(() => efx.createMeshData({ positions: P, joints: [0.5,0,0,0, 1,0,0,0, 0,0,0,0], weights: W }), RangeError);"
        "t(() => efx.createMeshData({ positions: P, joints: J, weights: ['x',0,0,0, 0,0,0,0, 0,0,0,0] }), TypeError);"
        "t(() => efx.createMeshData({ positions: P, joints: J, weights: W, bogus: 1 }), TypeError);";
    if (ok_js(code)) {
        end_js();
        return fail("skin js");
    }
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
    if (!res) {
        end_js();
        return fail("open gltf fixtures");
    }
    efx_runtime_set_resource(g_rt, res);
    const char *code =
        "function kind(fn) { try { fn(); } catch (e) {"
        "  if (e instanceof TypeError) return 'TypeError';"
        "  if (e instanceof RangeError) return 'RangeError';"
        "  if (e instanceof Error) return 'Error'; return 'other'; } return 'none'; }"
        "var md = efx.loadMeshData('skin.gltf');"
        "var mesh = efx.createMesh(md); md.destroy();"
        "if (mesh.clips !== undefined || mesh.jointCount !== undefined)"
        "  throw new Error('rig must stay opaque');"
        "efx.poseMesh(mesh, { clip: 'move', time: 0.25 });"
        "efx.poseMesh(mesh, { clip: 0, time: 0.5 });"
        "efx.poseMesh(mesh, [{ clip: 'move', time: 0.1, weight: 1 },"
        "                    { clip: 'turn', time: 0.6, weight: 2 }]);"
        "efx.poseMesh(mesh, { clip: 'move', time: 5.5 });"
        "if (kind(function () { efx.poseMesh(mesh, { clip: 'nope', time: 0 }); }) !== 'Error')"
        "  throw new Error('unknown clip name');"
        "if (kind(function () { efx.poseMesh(mesh, { clip: 9, time: 0 }); }) !== 'RangeError')"
        "  throw new Error('clip index range');"
        "if (kind(function () { efx.poseMesh(mesh, { clip: 'move', time: 0, weight: -1 }); }) !== 'RangeError')"
        "  throw new Error('negative weight');"
        "if (kind(function () { efx.poseMesh(mesh, { clip: 'move', time: 0, bogus: 1 }); }) !== 'TypeError')"
        "  throw new Error('unknown sample field');"
        "if (kind(function () { efx.poseMesh(mesh, { clip: 'move', time: 'x' }); }) !== 'TypeError')"
        "  throw new Error('time type');"
        "if (kind(function () { efx.poseMesh(mesh, { clip: {}, time: 0 }); }) !== 'TypeError')"
        "  throw new Error('clip type');"
        "if (kind(function () { efx.poseMesh(mesh, 5); }) !== 'TypeError')"
        "  throw new Error('pose type');"
        "efx.drawMesh(mesh, { skinned: true });"
        "efx.drawMesh(mesh);"
        "if (kind(function () { efx.drawMesh(mesh, { skinned: 1 }); }) !== 'TypeError')"
        "  throw new Error('skinned type');"
        "if (kind(function () { efx.drawMesh(mesh, { bogus: 1 }); }) !== 'TypeError')"
        "  throw new Error('draw unknown field');"
        "var plain = efx.createMesh(efx.createMeshData({"
        "  positions: [0,0,0, 1,0,0, 0,1,0], indices: [0,1,2] }));"
        "if (kind(function () { efx.poseMesh(plain, { clip: 0, time: 0 }); }) !== 'TypeError')"
        "  throw new Error('rig-less pose');"
        "if (kind(function () { efx.drawMesh(plain, { skinned: true }); }) !== 'TypeError')"
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
    if (!res) {
        end_js();
        return fail("open module fixtures");
    }
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
    if (!strcmp(c, "font_js")) return font_js();
    if (!strcmp(c, "gltf_js")) return gltf_js();
    if (!strcmp(c, "skin_js")) return skin_js();
    if (!strcmp(c, "pose_js")) return pose_js();
    if (!strcmp(c, "repl_eval")) return repl_eval();
    if (!strcmp(c, "input_js")) return input_js();
    if (!strcmp(c, "module_js")) return module_js();
    if (!strcmp(c, "module_hooks_js")) return module_hooks_js();
    fprintf(stderr, "unknown case: %s\n", c);
    return 2;
}
