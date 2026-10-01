/*
 * Headless display-list unit tests (ADR 0019 record/assert gate).
 * Usage: efx_render_tests [<case-name>] ; exit 0 = pass.
 */
#include "render/render.h"
#include "render/skin.h"

#include <stdlib.h>

#include "../test_support.h"

/* mock sink --------------------------------------------------------- */

static int g_tex_created, g_tex_destroyed;
static int g_mesh_created, g_mesh_destroyed;
static int g_rt_created, g_rt_destroyed;
static int g_last_mesh_surf_count;
static int g_last_mesh_vert_total;
static int g_last_mesh_index_total;
static int g_last_mesh_skinned_total;

static void *mock_create(void *ud, int w, int h, const uint8_t *rgba,
                         int wrap, int filter, int mipmaps) {
    (void)ud; (void)w; (void)h; (void)rgba; (void)wrap; (void)filter;
    (void)mipmaps;
    g_tex_created++;
    return malloc(8);
}

static void mock_destroy(void *ud, void *native) {
    (void)ud;
    g_tex_destroyed++;
    free(native);
}

static void *mock_create_mesh(void *ud, const efx_mesh_gpu_surface *surfs,
                              int count) {
    (void)ud;
    g_mesh_created++;
    g_last_mesh_surf_count = count;
    g_last_mesh_vert_total = 0;
    g_last_mesh_index_total = 0;
    g_last_mesh_skinned_total = 0;
    for (int i = 0; i < count; i++) {
        if (surfs[i].vertex_count <= 0) {
            return NULL; /* bad surface: signal upload failure */
        }
        g_last_mesh_vert_total += surfs[i].vertex_count;
        g_last_mesh_index_total += surfs[i].index_count;
        g_last_mesh_skinned_total += surfs[i].skinned ? 1 : 0;
    }
    return malloc(8);
}

static void mock_destroy_mesh(void *ud, void *native) {
    (void)ud;
    g_mesh_destroyed++;
    free(native);
}

static void *mock_create_rt(void *ud, int w, int h) {
    (void)ud; (void)w; (void)h;
    g_rt_created++;
    return malloc(8);
}

static void mock_destroy_rt(void *ud, void *native) {
    (void)ud;
    g_rt_destroyed++;
    free(native);
}

static void install_mock_sink(void) {
    static const efx_render_sink sink = {
        NULL, mock_create, mock_destroy, mock_create_mesh, mock_destroy_mesh,
        NULL, mock_create_rt, mock_destroy_rt,
    };
    efx_render_install_sink(&sink);
    g_tex_created = 0;
    g_tex_destroyed = 0;
    g_mesh_created = 0;
    g_mesh_destroyed = 0;
    g_rt_created = 0;
    g_rt_destroyed = 0;
}

/* a two-surface fixture: surface 0 indexed with all attributes,
 * surface 1 positions-only non-indexed (design D1/D8) */
static efx_meshdata *make_two_surface_mesh(void) {
    static const float pos0[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    static const float nrm0[9] = {0, 0, 1, 0, 0, 1, 0, 0, 1};
    static const float uv0[6] = {0, 0, 1, 0, 0, 1};
    static const float col0[12] = {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1};
    static const uint32_t idx0[3] = {0, 1, 2};
    static const float pos1[9] = {0, 0, 1, 1, 0, 1, 0, 1, 1};
    efx_surface_src src[2];
    memset(src, 0, sizeof(src));
    src[0].positions_len = 9;
    src[0].normals_len = 9;
    src[0].uvs_len = 6;
    src[0].colors_len = 12;
    src[0].indices_len = 3;
    src[0].positions = pos0;
    src[0].normals = nrm0;
    src[0].uvs = uv0;
    src[0].colors = col0;
    src[0].indices = idx0;
    src[1].positions_len = 9;
    src[1].positions = pos1;
    int err = 0;
    efx_meshdata *md = efx_meshdata_create(src, 2, &err);
    if (!md) {
        fprintf(stderr, "fixture create failed: %d\n", err);
    }
    return md;
}

/* cases -------------------------------------------------------------- */

static int compose_camera(void) {
    efx_camera2d cam = {640, 480, 320, 240, 1, 0};
    efx_affine m = efx_camera_matrix(&cam, 640, 480);
    /* center maps to center */
    float px = m.a * 320 + m.c * 240 + m.tx;
    float py = m.b * 320 + m.d * 240 + m.ty;
    if (!feq(px, 320) || !feq(py, 240)) return fail("center not fixed");
    /* zoom 2: visible world width halves; world x=480 (the new right edge)
       maps to the frame's right edge (640) */
    cam.zoom = 2;
    m = efx_camera_matrix(&cam, 640, 480);
    px = m.a * 480 + m.c * 240 + m.tx;
    py = m.b * 480 + m.d * 240 + m.ty;
    if (!feq(px, 640) || !feq(py, 240)) return fail("zoom pivot");
    /* rotation 90 clockwise (y-down): right of center -> below center */
    cam.zoom = 1;
    cam.rotation = 90;
    m = efx_camera_matrix(&cam, 640, 480);
    px = m.a * 420 + m.c * 240 + m.tx;
    py = m.b * 420 + m.d * 240 + m.ty;
    if (!feq(px, 320) || !feq(py, 340)) return fail("rotation pivot/direction");
    return 0;
}

static int compose_quad(void) {
    /* corner anchoring, identity transform */
    efx_affine m = efx_quad_matrix(10, 20, 50, 25, 0, 1);
    float px = m.a * 0 + m.c * 0 + m.tx;
    float py = m.b * 0 + m.d * 0 + m.ty;
    if (!feq(px, 10) || !feq(py, 20)) return fail("top-left anchor");
    px = m.a * 100 + m.c * 50 + m.tx;
    py = m.b * 100 + m.d * 50 + m.ty;
    if (!feq(px, 110) || !feq(py, 70)) return fail("bottom-right corner");
    /* rotation pivots on quad center */
    m = efx_quad_matrix(10, 20, 50, 25, 90, 1);
    px = m.a * 0 + m.c * 0 + m.tx;
    py = m.b * 0 + m.d * 0 + m.ty;
    if (!feq(px, 85) || !feq(py, -5)) return fail("rotation pivot center");
    /* scale pivots on quad center: corners at center ± (2*50, 2*25) */
    m = efx_quad_matrix(10, 20, 50, 25, 0, 2);
    px = m.a * 0 + m.c * 0 + m.tx;
    py = m.b * 0 + m.d * 0 + m.ty;
    if (!feq(px, -40) || !feq(py, -5)) return fail("scale pivot center");
    return 0;
}

static int value_snapshot(void) {
    install_mock_sink();
    efx_render_set_viewport(1024, 768);
    efx_camera2d cam = {640, 480, 320, 240, 1, 0};
    efx_render_set_camera(&cam);
    efx_render_quad(0, 0, 32, 32, 0, NULL, 0, 1, NULL, 0, 16, 16);
    efx_camera2d cam2 = {640, 480, 100, 100, 1, 0};
    efx_render_set_camera(&cam2);
    efx_render_quad(0, 0, 32, 32, 0, NULL, 0, 1, NULL, 0, 16, 16);

    /* camera at record time applies; second quad sees new camera */
    efx_affine expect = efx_camera_matrix(&cam, 640, 480);
    expect = efx_affine_mul(expect, efx_quad_matrix(0, 0, 16, 16, 0, 1));
    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    if (count != 2) return fail("record count");
    if (!feq(recs[0].u.quad.m.a, expect.a) ||
        !feq(recs[0].u.quad.m.tx, expect.tx))
        return fail("first quad uses first camera");
    if (feq(recs[1].u.quad.m.tx, expect.tx))
        return fail("second quad used old camera");
    /* keys: stable sort key = record index */
    if (recs[0].sort_key != 0 || recs[1].sort_key != 1) return fail("sort keys");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int default_camera_viewport(void) {
    install_mock_sink();
    efx_render_set_viewport(1024, 600);
    efx_render_quad(0, 0, 8, 8, 0, NULL, 0, 1, NULL, 0, 4, 4);
    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    if (recs[0].u.quad.frame_w != 1024 || recs[0].u.quad.frame_h != 600)
        return fail("default frame = viewport");
    /* default view looks at frame center */
    if (!feq(recs[0].u.quad.m.a, 1) || !feq(recs[0].u.quad.m.tx, 0) ||
        !feq(recs[0].u.quad.m.ty, 0))
        return fail("default view identity");
    /* frame set through camera wins over viewport */
    efx_camera2d cam = {640, 480, 320, 240, 1, 0};
    efx_render_set_camera(&cam);
    efx_render_quad(0, 0, 8, 8, 0, NULL, 0, 1, NULL, 0, 4, 4);
    recs = efx_render_records(&count);
    if (recs[1].u.quad.frame_w != 640 || recs[1].u.quad.frame_h != 480)
        return fail("explicit frame");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int blend_snapshot(void) {
    install_mock_sink();
    efx_render_quad(0, 0, 8, 8, 0, NULL, 0, 1, NULL, 0, 4, 4);
    efx_render_set_blend(EFX_BLEND_ADDITIVE);
    efx_render_quad(0, 0, 8, 8, 0, NULL, 0, 1, NULL, 0, 4, 4);
    if (efx_render_set_blend(99) == 0) return fail("invalid blend accepted");
    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    if (recs[0].u.quad.blend != EFX_BLEND_ALPHA ||
        recs[1].u.quad.blend != EFX_BLEND_ADDITIVE)
        return fail("blend per record");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int record_budget(void) {
    static const efx_render_sink sink = {
        NULL, mock_create, mock_destroy, mock_create_mesh, mock_destroy_mesh,
        NULL, mock_create_rt, mock_destroy_rt,
    };
    efx_render_install_sink(&sink);
    int pushed = 0;
    for (;;) {
        int rc = efx_render_quad(0, 0, 1, 1, 0, NULL, 0, 1, NULL, 0, 0.5f, 0.5f);
        if (rc == EFX_RENDER_ERR_BUDGET) break;
        if (rc != EFX_RENDER_OK) return fail("unexpected error in budget loop");
        pushed++;
        if (pushed > 1000000) return fail("budget never reached");
    }
    if (pushed < 100000) return fail("budget suspiciously small");
    efx_render_begin_frame();
    int count = 0;
    efx_render_records(&count);
    if (count != 0) return fail("rewind after budget stop");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int texture_lifecycle(void) {
    install_mock_sink();
    uint8_t px[4] = {255, 0, 0, 255};
    uint64_t t1 = efx_render_texture_create(2, 2, px, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    if (!t1 || !efx_render_texture_alive(t1)) return fail("texture create");
    int w = 0, h = 0;
    efx_render_texture_size(t1, &w, &h);
    if (w != 2 || h != 2) return fail("texture size");
    /* deferred destroy: native alive until end_frame */
    if (efx_render_texture_destroy(t1) != EFX_RENDER_OK) return fail("destroy");
    if (efx_render_texture_alive(t1)) return fail("alive after destroy");
    if (g_tex_destroyed != 0) return fail("destroy not deferred");
    efx_render_end_frame();
    if (g_tex_destroyed != 1) return fail("deferred destroy not flushed");
    /* after the frame-end release the C handle is dead (JS wrapper layer
       supplies the documented idempotency via its own flag) */
    if (efx_render_texture_destroy(t1) == EFX_RENDER_OK)
        return fail("dead handle destroy");
    /* stale handle (generation bump) */
    uint64_t stale = t1;
    uint64_t t2 = efx_render_texture_create(1, 1, px, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    if (!t2 || t2 == stale) return fail("handle reuse");
    if (efx_render_texture_alive(stale)) return fail("stale handle alive");
    if (efx_render_texture_destroy(stale) == EFX_RENDER_OK)
        return fail("stale destroy");
    /* white texture: permanent, usable, not destroyable */
    uint64_t white = efx_render_white_texture();
    if (!white || !efx_render_texture_alive(white)) return fail("white texture");
    if (efx_render_texture_destroy(white) != EFX_RENDER_ERR_PERMANENT)
        return fail("white destroy must be refused");
    efx_render_end_frame();
    efx_render_shutdown();
    efx_render_install_sink(NULL);
    if (efx_render_white_texture() != 0) return fail("white after shutdown");
    return 0;
}

/* P12: queued (sink-less) texture creation always appends a fresh slot and
 * never reuses a freed one; the resulting handle sequence is observable, so
 * pin it before unifying the slot initialisation. */
static int texture_queued_handles(void) {
    uint8_t px[4] = {1, 2, 3, 4};
    uint64_t t1 = efx_render_texture_create(1, 1, px, EFX_TEX_WRAP_REPEAT,
                                            EFX_FILTER_LINEAR, 0);
    uint64_t t2 = efx_render_texture_create(1, 1, px, EFX_TEX_WRAP_REPEAT,
                                            EFX_FILTER_LINEAR, 0);
    if (t1 != (((uint64_t)1 << 32) | 1)) return fail("queued handle 1");
    if (t2 != (((uint64_t)1 << 32) | 2)) return fail("queued handle 2");
    /* destroying a queued texture frees its pending bytes but leaves the slot
       consumed; the next create appends rather than reusing it */
    if (efx_render_texture_destroy(t1) != EFX_RENDER_OK)
        return fail("destroy queued");
    uint64_t t3 = efx_render_texture_create(1, 1, px, EFX_TEX_WRAP_REPEAT,
                                            EFX_FILTER_LINEAR, 0);
    if (t3 != (((uint64_t)1 << 32) | 3)) return fail("queued handle 3 (append)");
    if (t1 == t3 || t2 == t3) return fail("queued handle reuse");
    /* growth past the initial capacity keeps the append-only sequence */
    uint64_t last = 0;
    for (int i = 0; i < 70; i++) {
        last = efx_render_texture_create(1, 1, px, EFX_TEX_WRAP_REPEAT,
                                         EFX_FILTER_LINEAR, 0);
        if (!last) return fail("queued grow");
    }
    if (last != (((uint64_t)1 << 32) | 73))
        return fail("queued handle after grow");
    efx_render_shutdown();
    return 0;
}

/* F6e: the mipmaps creation flag is stored on the slot and read back;
 * any truthy value normalizes to 1, absent/false to 0 */
static int texture_mipmaps(void) {    install_mock_sink();
    uint8_t px[16] = {0};
    uint64_t plain = efx_render_texture_create(2, 2, px, EFX_TEX_WRAP_REPEAT,
                                               EFX_FILTER_LINEAR, 0);
    uint64_t mip = efx_render_texture_create(2, 2, px, EFX_TEX_WRAP_REPEAT,
                                             EFX_FILTER_LINEAR, 1);
    uint64_t normalized = efx_render_texture_create(
        2, 2, px, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 7);
    if (!plain || !mip || !normalized) return fail("texture create");
    int w = 0, f = 0, m = -1;
    efx_render_texture_sampler(plain, &w, &f, &m);
    if (m != 0) return fail("plain mipmaps flag");
    efx_render_texture_sampler(mip, &w, &f, &m);
    if (m != 1) return fail("mipmaps flag not set");
    efx_render_texture_sampler(normalized, &w, &f, &m);
    if (m != 1) return fail("mipmaps flag not normalized");
    efx_render_texture_destroy(plain);
    efx_render_texture_destroy(mip);
    efx_render_texture_destroy(normalized);
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int record_fields(void) {
    install_mock_sink();
    uint8_t px[4] = {0, 0, 255, 255};
    uint64_t tex = efx_render_texture_create(64, 32, px, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    float color[4] = {1, 0.5, 0.25, 0.125};
    float src[4] = {8, 4, 16, 8};
    efx_render_quad(1, 2, 30, 40, tex, color, 45, 2, src, 1, 15, 20);
    int count = 0;
    const efx_record *r = efx_render_records(&count);
    if (count != 1) return fail("count");
    if (r[0].type != EFX_RECORD_QUAD) return fail("record type");
    if (r[0].u.quad.tw != 64 || r[0].u.quad.th != 32) return fail("texel size");
    if (!feq(r[0].u.quad.sx, 8) || !feq(r[0].u.quad.sw, 16) ||
        !feq(r[0].u.quad.sh, 8))
        return fail("source rect");
    if (!feq(r[0].u.quad.color[1], 0.5f) || !feq(r[0].u.quad.color[3], 0.125f))
        return fail("tint");
    /* default source rect = full texture */
    efx_render_quad(0, 0, 4, 4, tex, NULL, 0, 1, NULL, 0, 2, 2);
    r = efx_render_records(&count);
    if (!feq(r[1].u.quad.sw, 64) || !feq(r[1].u.quad.sh, 32))
        return fail("default src");
    /* default tint = opaque white */
    if (!feq(r[1].u.quad.color[0], 1) || !feq(r[1].u.quad.color[3], 1))
        return fail("default tint");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int batching(void) {
    install_mock_sink();
    uint8_t px[4] = {255, 255, 255, 255};
    uint64_t t1 = efx_render_texture_create(4, 4, px, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    uint64_t t2 = efx_render_texture_create(4, 4, px, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    /* sequence: A A B A  -> three runs (t1x2, t2, t1) */
    efx_render_quad(0, 0, 4, 4, t1, NULL, 0, 1, NULL, 0, 2, 2);
    efx_render_quad(5, 0, 4, 4, t1, NULL, 0, 1, NULL, 0, 2, 2);
    efx_render_set_blend(EFX_BLEND_ADDITIVE);
    efx_render_quad(9, 0, 4, 4, t1, NULL, 0, 1, NULL, 0, 2, 2);
    efx_render_quad(12, 0, 4, 4, t2, NULL, 0, 1, NULL, 0, 2, 2);
    efx_render_set_blend(EFX_BLEND_ALPHA);
    efx_render_quad(15, 0, 4, 4, t1, NULL, 0, 1, NULL, 0, 2, 2);
    int run_count = 0;
    const efx_draw_run *runs = efx_render_runs(&run_count);
    if (run_count != 4) return fail("expected 4 runs");
    if (runs[0].texture != t1 || runs[0].count != 2) return fail("run 0");
    if (runs[1].blend != EFX_BLEND_ADDITIVE) return fail("run 1 blend");
    if (runs[2].texture != t2) return fail("run 2 texture");
    if (runs[3].texture != t1 || runs[3].start != 4) return fail("run 3");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int meshdata_validation(void) {
    float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    uint32_t idx[3] = {0, 1, 2};
    efx_surface_src s;
    memset(&s, 0, sizeof(s));
    s.positions_len = 9;
    s.positions = pos;
    s.indices_len = 3;
    s.indices = idx;

    int err = -1;
    /* indexed: vertex count need not be divisible by 3 */
    s.indices = idx;
    efx_meshdata *md = efx_meshdata_create(&s, 1, &err);
    if (!md) return fail("valid indexed surface rejected");
    efx_meshdata_destroy(md);
    /* (idempotency is supplied by the JS wrapper's alive flag, as with
        ImageData; the core release is call-once) */

    /* non-indexed: vertex count must be divisible by 3 (2 verts is not) */
    s.indices_len = 0;
    s.indices = NULL;
    s.positions_len = 6;
    if (efx_meshdata_create(&s, 1, &err)) return fail("non-multiple vcount ok?");
    if (err != EFX_MESHERR_LEN) return fail("wrong error for vcount");
    s.positions_len = 9;

    /* index out of vertex range */
    uint32_t bad_idx[3] = {0, 3, 1};
    s.indices_len = 3;
    s.indices = bad_idx;
    if (efx_meshdata_create(&s, 1, &err)) return fail("oob index ok?");
    if (err != EFX_MESHERR_INDEX) return fail("wrong error for index");

    /* positions length not a multiple of 3 */
    s.indices_len = 0;
    s.indices = NULL;
    s.positions_len = 7;
    if (efx_meshdata_create(&s, 1, &err)) return fail("bad pos len ok?");
    if (err != EFX_MESHERR_LEN) return fail("wrong error for pos len");

    /* attribute mismatch: normals shorter than positions */
    static const float nrm[6] = {0, 0, 1, 0, 0, 1};
    s.positions_len = 9;
    s.normals_len = 6;
    s.normals = nrm;
    if (efx_meshdata_create(&s, 1, &err)) return fail("normals mismatch ok?");
    if (err != EFX_MESHERR_LEN) return fail("wrong error for normals");

    /* surface count cap: 17 surfaces rejected, 16 accepted */
    s.normals_len = 0;
    s.normals = NULL;
    efx_surface_src many[EFX_MESH_MAX_SURFACES + 1];
    for (int i = 0; i <= EFX_MESH_MAX_SURFACES; i++) {
        many[i] = s;
    }
    if (efx_meshdata_create(many, EFX_MESH_MAX_SURFACES + 1, &err))
        return fail("17 surfaces ok?");
    if (err != EFX_MESHERR_COUNT) return fail("wrong error for count");
    md = efx_meshdata_create(many, EFX_MESH_MAX_SURFACES, &err);
    if (!md) return fail("16 surfaces rejected");
    if (md->surface_count != EFX_MESH_MAX_SURFACES) return fail("surface count");
    efx_meshdata_destroy(md);

    /* deep copy: mutating the source does not affect the mesh data */
    s.positions_len = 9;
    float pos2[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    s.positions = pos2;
    md = efx_meshdata_create(&s, 1, &err);
    if (!md) return fail("copy fixture");
    pos2[0] = 99.0f;
    if (!feq(md->surfaces[0].positions[0], 0.0f)) return fail("not deep-copied");
    efx_meshdata_destroy(md);
    return 0;
}

/* F6c: joints/weights are a paired, four-influences-per-vertex attribute */
static int meshdata_skinning(void) {
    float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    uint32_t joints[12] = {0, 1, 2, 3, 0, 0, 0, 0, 1, 1, 1, 1};
    float weights[12] = {1, 0, 0, 0, 0.5f, 0.5f, 0, 0, 1, 0, 0, 0};
    efx_surface_src s;
    memset(&s, 0, sizeof(s));
    s.positions_len = 9;
    s.positions = pos;

    int err = -1;
    /* unpaired: joints without weights is rejected */
    s.joints_len = 12;
    s.joints = joints;
    if (efx_meshdata_create(&s, 1, &err)) return fail("unpaired joints ok?");
    if (err != EFX_MESHERR_LEN) return fail("wrong error for unpaired");

    /* unpaired: weights without joints is rejected */
    s.joints_len = 0;
    s.joints = NULL;
    s.weights_len = 12;
    s.weights = weights;
    if (efx_meshdata_create(&s, 1, &err)) return fail("unpaired weights ok?");
    if (err != EFX_MESHERR_LEN) return fail("wrong error for unpaired w");

    /* valid pair: retained and deep-copied */
    s.joints_len = 12;
    s.joints = joints;
    efx_meshdata *md = efx_meshdata_create(&s, 1, &err);
    if (!md) return fail("valid skinned surface rejected");
    if (!md->surfaces[0].joints || !md->surfaces[0].weights)
        return fail("skinned attributes not retained");
    if (md->surfaces[0].joints[1] != 1) return fail("joint value");
    if (!feq(md->surfaces[0].weights[4], 0.5f)) return fail("weight value");
    joints[0] = 99;
    weights[0] = 99.0f;
    if (md->surfaces[0].joints[0] != 0 || !feq(md->surfaces[0].weights[0], 1.0f))
        return fail("skinned attributes not deep-copied");
    efx_meshdata_destroy(md);

    /* count mismatch: joints shorter than vertexCount*4 */
    s.joints_len = 8;
    if (efx_meshdata_create(&s, 1, &err)) return fail("short joints ok?");
    if (err != EFX_MESHERR_LEN) return fail("wrong error for short joints");

    /* count mismatch: weights shorter than vertexCount*4 */
    s.joints_len = 12;
    s.weights_len = 4;
    if (efx_meshdata_create(&s, 1, &err)) return fail("short weights ok?");
    if (err != EFX_MESHERR_LEN) return fail("wrong error for short weights");
    return 0;
}

static int mesh_lifecycle(void) {
    install_mock_sink();
    efx_meshdata *md = make_two_surface_mesh();
    if (!md) return fail("fixture");
    uint64_t m1 = efx_render_mesh_create(md);
    if (!m1 || !efx_render_mesh_alive(m1)) return fail("mesh create");
    if (!efx_render_mesh_surface_count(m1)) {
        if (efx_render_mesh_surface_count(m1) != 2) return fail("surface count");
    }
    /* uploaded straight to the mock sink: 2 surfaces, 3+3 verts */
    if (g_mesh_created != 1) return fail("sink create_mesh not called");
    if (g_last_mesh_surf_count != 2) return fail("gpu surface count");
    if (g_last_mesh_vert_total != 6) return fail("gpu vertex total");
    /* surface 0: 3 real indices; surface 1: 3 synthesized identity ones */
    if (g_last_mesh_index_total != 6) return fail("gpu index total");

    /* Mesh is a copy: destroying the MeshData keeps the Mesh alive */
    efx_meshdata_destroy(md);
    if (!efx_render_mesh_alive(m1)) return fail("mesh depends on meshdata");

    /* alive drops immediately (JS wrapper owns the documented idempotency);
       the native release defers to frame end */
    if (efx_render_mesh_destroy(m1) != EFX_RENDER_OK) return fail("mesh destroy");
    if (efx_render_mesh_alive(m1)) return fail("alive after destroy");
    if (g_mesh_destroyed != 0) return fail("destroy not deferred");
    efx_render_end_frame();
    if (g_mesh_destroyed != 1) return fail("deferred mesh destroy");
    if (efx_render_mesh_alive(m1)) return fail("alive after destroy");
    if (efx_render_mesh_destroy(m1) == EFX_RENDER_OK)
        return fail("dead mesh handle destroy");

    /* the released slot is reused (same index, bumped generation) and the
       stale handle stays dead */
    uint64_t stale = m1;
    md = make_two_surface_mesh();
    uint64_t m2 = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    if (!m2 || m2 == stale) return fail("mesh handle reuse");
    if ((m2 & 0xffffffffu) != (stale & 0xffffffffu)) return fail("mesh slot not reused");
    if (efx_render_mesh_alive(stale)) return fail("stale mesh alive");
    if (!efx_render_mesh_alive(m2)) return fail("reused slot not alive");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int mesh_pending_upload(void) {
    /* no sink: create queues; installing the sink flushes (headless
       parity with texture pending uploads) */
    g_tex_created = g_mesh_created = 0;
    uint8_t px[4] = {255, 0, 0, 255};
    uint64_t t = efx_render_texture_create(2, 2, px, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    (void)t;
    if (g_tex_created != 0) return fail("texture created without sink");
    efx_meshdata *md = make_two_surface_mesh();
    if (!md) return fail("fixture");
    uint64_t m = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    if (!m) return fail("pending mesh create");
    if (g_mesh_created != 0) return fail("created without sink");
    static const efx_render_sink sink = {
        NULL, mock_create, mock_destroy, mock_create_mesh, mock_destroy_mesh,
        NULL, mock_create_rt, mock_destroy_rt,
    };
    efx_render_install_sink(&sink);
    if (g_mesh_created != 1) return fail("pending flush");
    if (g_last_mesh_surf_count != 2) return fail("flushed surface count");
    if (efx_render_mesh_destroy(m) != EFX_RENDER_OK) return fail("destroy");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int mesh_record_fields(void) {
    install_mock_sink();
    efx_meshdata *md = make_two_surface_mesh();
    uint64_t m = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    if (!m) return fail("mesh create");

    float transform[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 3, 4, 1};
    float tint[4] = {1, 0.5, 0.25, 1};
    efx_camera3d cam;
    memset(&cam, 0, sizeof(cam));
    cam.pos[0] = 0;
    cam.pos[1] = 2;
    cam.pos[2] = 5;
    cam.target[0] = 0;
    cam.target[1] = 0;
    cam.target[2] = 0;
    cam.fov = 60;
    cam.near_z = 0.1f;
    cam.far_z = 100.0f;
    efx_render_set_camera3d(&cam);
    efx_render_mesh(m, transform, tint, 0);
    /* value snapshot: a later camera change must not apply */
    efx_camera3d cam2;
    memset(&cam2, 0, sizeof(cam2));
    cam2.pos[2] = 50;
    cam2.fov = 30;
    efx_render_set_camera3d(&cam2);
    /* dead mesh handle rejected */
    if (efx_render_mesh(0, NULL, NULL, 0) != EFX_RENDER_ERR_HANDLE)
        return fail("null mesh accepted");

    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    if (count != 1) return fail("record count");
    const efx_mesh_record *mr = &recs[0].u.mesh;
    if (recs[0].type != EFX_RECORD_MESH) return fail("record type");
    if (mr->mesh != m) return fail("mesh handle");
    if (!feq(mr->transform[12], 2) || !feq(mr->transform[13], 3) ||
        !feq(mr->transform[14], 4))
        return fail("transform column-major storage");
    if (!feq(mr->color[1], 0.5f)) return fail("tint");
    /* camera snapshotted at record time */
    if (!feq(mr->camera.pos[2], 5) || !feq(mr->camera.fov, 60))
        return fail("camera snapshot");
    if (!feq(mr->camera.near_z, 0.1f) || !feq(mr->camera.far_z, 100.0f))
        return fail("camera depth range");

    /* default transform = identity, default tint = white */
    if (efx_render_mesh(m, NULL, NULL, 0) != EFX_RENDER_OK)
        return fail("default args rejected");
    recs = efx_render_records(&count);
    mr = &recs[1].u.mesh;
    if (!feq(mr->transform[0], 1) || !feq(mr->transform[5], 1) ||
        !feq(mr->transform[10], 1) || !feq(mr->transform[15], 1))
        return fail("default identity");
    if (!feq(mr->transform[12], 0)) return fail("identity translation");
    if (!feq(mr->color[0], 1) || !feq(mr->color[3], 1)) return fail("white tint");
    /* second record saw the NEW camera (value snapshot both ways) */
    if (!feq(mr->camera.pos[2], 50) || !feq(mr->camera.fov, 30))
        return fail("new camera on later record");
    /* blend value-snapshots into mesh records too */
    efx_render_set_blend(EFX_BLEND_ADDITIVE);
    efx_render_mesh(m, NULL, NULL, 0);
    recs = efx_render_records(&count);
    if (recs[2].u.mesh.blend != EFX_BLEND_ADDITIVE) return fail("mesh blend");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int mesh_record_order(void) {
    install_mock_sink();
    efx_meshdata *md = make_two_surface_mesh();
    uint64_t m = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    uint8_t px[4] = {255, 255, 255, 255};
    uint64_t t = efx_render_texture_create(4, 4, px, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    /* quads A A, mesh, quad B: quad runs must not span the mesh record */
    efx_render_quad(0, 0, 4, 4, t, NULL, 0, 1, NULL, 0, 2, 2);
    efx_render_quad(5, 0, 4, 4, t, NULL, 0, 1, NULL, 0, 2, 2);
    efx_render_mesh(m, NULL, NULL, 0);
    efx_render_quad(9, 0, 4, 4, t, NULL, 0, 1, NULL, 0, 2, 2);
    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    if (count != 4) return fail("record count");
    if (recs[0].type != EFX_RECORD_QUAD || recs[1].type != EFX_RECORD_QUAD ||
        recs[2].type != EFX_RECORD_MESH || recs[3].type != EFX_RECORD_QUAD)
        return fail("order");
    if (recs[2].sort_key != 2) return fail("mesh sort key");
    int run_count = 0;
    const efx_draw_run *runs = efx_render_runs(&run_count);
    if (run_count != 2) return fail("mesh must break quad runs");
    if (runs[0].start != 0 || runs[0].count != 2) return fail("run before mesh");
    if (runs[1].start != 3 || runs[1].count != 1) return fail("run after mesh");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int mesh_record_budget(void) {
    install_mock_sink();
    /* one 16-surface mesh records as ONE record (budget counts records,
       playback expands per surface — design Risks) */
    float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    uint32_t idx[3] = {0, 1, 2};
    efx_surface_src s;
    memset(&s, 0, sizeof(s));
    s.positions_len = 9;
    s.positions = pos;
    s.indices_len = 3;
    s.indices = idx;
    efx_surface_src many[EFX_MESH_MAX_SURFACES];
    for (int i = 0; i < EFX_MESH_MAX_SURFACES; i++) {
        many[i] = s;
    }
    int err = 0;
    efx_meshdata *md = efx_meshdata_create(many, EFX_MESH_MAX_SURFACES, &err);
    if (!md) return fail("16-surface fixture");
    uint64_t m = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    if (!m) return fail("16-surface mesh create");
    if (efx_render_mesh(m, NULL, NULL, 0) != EFX_RENDER_OK)
        return fail("16-surface mesh record");
    int count = 0;
    efx_render_records(&count);
    if (count != 1) return fail("16-surface mesh must be one record");
    /* and the budget still trips eventually on mesh records */
    int pushed = 0;
    for (;;) {
        int rc = efx_render_mesh(m, NULL, NULL, 0);
        if (rc == EFX_RENDER_ERR_BUDGET) break;
        if (rc != EFX_RENDER_OK) return fail("mesh budget loop error");
        pushed++;
        if (pushed > 1000000) return fail("mesh budget never reached");
    }
    if (pushed < 10000) return fail("mesh budget suspiciously small");
    efx_render_begin_frame();
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int lights_state(void) {
    install_mock_sink();
    efx_light_set ls;
    efx_render_lights(&ls);
    for (int i = 0; i < EFX_MAX_POINT_LIGHTS; i++) {
        if (ls.points[i].enabled) return fail("points default disabled");
    }
    if (ls.directional.enabled) return fail("directional default disabled");

    efx_point_light p;
    memset(&p, 0, sizeof(p));
    p.enabled = 1;
    p.pos[0] = 1; p.pos[1] = 2; p.pos[2] = 3;
    p.color[0] = 1; p.color[1] = 0.5f; p.color[2] = 0.25f; p.color[3] = 1;
    p.range = 10;
    efx_render_set_point_light(0, &p);
    efx_render_lights(&ls);
    if (!ls.points[0].enabled) return fail("slot 0 enabled");
    if (!feq(ls.points[0].pos[1], 2) || !feq(ls.points[0].range, 10))
        return fail("slot 0 values");
    if (ls.points[1].enabled) return fail("slot 1 untouched");

    /* replace only that slot */
    efx_point_light q = p;
    q.pos[0] = 9;
    efx_render_set_point_light(0, &q);
    efx_render_lights(&ls);
    if (!feq(ls.points[0].pos[0], 9)) return fail("slot replaced");

    /* null disables */
    efx_render_set_point_light(0, NULL);
    efx_render_lights(&ls);
    if (ls.points[0].enabled) return fail("null disables point");

    efx_dir_light dl;
    memset(&dl, 0, sizeof(dl));
    dl.enabled = 1;
    dl.dir[0] = 0; dl.dir[1] = -1; dl.dir[2] = 0;
    dl.color[0] = 0.2f; dl.color[1] = 0.3f; dl.color[2] = 0.4f; dl.color[3] = 1;
    efx_render_set_directional_light(&dl);
    efx_render_lights(&ls);
    if (!ls.directional.enabled || !feq(ls.directional.dir[1], -1))
        return fail("directional set");
    efx_render_set_directional_light(NULL);
    efx_render_lights(&ls);
    if (ls.directional.enabled) return fail("null disables directional");

    /* out-of-range slot ignored (binding raises RangeError before this) */
    efx_render_set_point_light(9, &p);
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int light_snapshot(void) {
    install_mock_sink();
    float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    uint32_t idx[3] = {0, 1, 2};
    efx_surface_src s;
    memset(&s, 0, sizeof(s));
    s.positions_len = 9;
    s.positions = pos;
    s.indices_len = 3;
    s.indices = idx;
    int err = 0;
    efx_meshdata *md = efx_meshdata_create(&s, 1, &err);
    if (!md) return fail("fixture");
    uint64_t m = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    if (!m) return fail("mesh create");

    efx_point_light p;
    memset(&p, 0, sizeof(p));
    p.enabled = 1;
    p.pos[0] = 1; p.pos[1] = 1; p.pos[2] = 1;
    p.color[0] = 1; p.color[3] = 1;
    efx_render_set_point_light(0, &p);
    if (efx_render_mesh(m, NULL, NULL, 0) != EFX_RENDER_OK)
        return fail("record");
    /* later light change must not alter the recorded snapshot */
    efx_render_set_point_light(0, NULL);
    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    if (count != 1) return fail("record count");
    if (!recs[0].u.mesh.lights.points[0].enabled ||
        !feq(recs[0].u.mesh.lights.points[0].pos[0], 1))
        return fail("light snapshot in record");
    if (recs[0].u.mesh.lights.directional.enabled)
        return fail("directional snapshot default");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int material_binding(void) {
    install_mock_sink();
    float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    uint32_t idx[3] = {0, 1, 2};
    efx_surface_src s;
    memset(&s, 0, sizeof(s));
    s.positions_len = 9;
    s.positions = pos;
    s.indices_len = 3;
    s.indices = idx;
    int err = 0;
    efx_meshdata *md = efx_meshdata_create(&s, 1, &err);
    if (!md) return fail("fixture");

    efx_material m;
    efx_material_default(&m);
    m.diffuse[0] = 0.25f;
    efx_meshdata_set_material(md, 0, &m, 1);
    if (!md->surfaces[0].has_material) return fail("meshdata material set");

    uint64_t h = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    if (!h) return fail("mesh create");

    efx_material got;
    if (efx_render_mesh_surface_material(h, 0, &got) != 1)
        return fail("material carried over at upload");
    if (!feq(got.diffuse[0], 0.25f)) return fail("material value copied");

    /* rebind + null reset */
    efx_material m2;
    efx_material_default(&m2);
    m2.emissive[1] = 0.5f;
    if (efx_render_mesh_set_material(h, 0, &m2, 1) != EFX_RENDER_OK)
        return fail("rebind");
    if (efx_render_mesh_surface_material(h, 0, &got) != 1 ||
        !feq(got.emissive[1], 0.5f))
        return fail("rebind value");
    if (efx_render_mesh_set_material(h, 0, NULL, 0) != EFX_RENDER_OK)
        return fail("null reset");
    if (efx_render_mesh_surface_material(h, 0, &got) != 0)
        return fail("default after reset");
    if (!feq(got.diffuse[0], 1.0f) || !feq(got.diffuse[1], 1.0f))
        return fail("default material white diffuse");
    if (!feq(got.shininess, 32.0f)) return fail("default shininess");

    /* out-of-range index and dead handle */
    if (efx_render_mesh_set_material(h, 5, &m, 1) != EFX_RENDER_ERR_INDEX)
        return fail("index range");
    if (efx_render_mesh_set_material(h + 1, 0, &m, 1) != EFX_RENDER_ERR_HANDLE)
        return fail("dead handle");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int lighting_reference(void) {
    efx_material mat;
    efx_material_default(&mat);
    efx_light_set ls;
    memset(&ls, 0, sizeof(ls));
    float alb[4] = {1, 1, 1, 1};
    float out[4];
    float world[3] = {0, 0, 0};
    float cam[3] = {0, 0, 5};

    /* no lights: default material is black */
    float n[3] = {0, 0, 1};
    efx_lighting_shade(&mat, &ls, world, n, cam, alb, NULL, out);
    if (!feq(out[0], 0) || !feq(out[1], 0) || !feq(out[2], 0))
        return fail("no lights -> black");
    if (!feq(out[3], 1)) return fail("alpha = albedo alpha");

    /* head-on point light: full diffuse */
    ls.points[0].enabled = 1;
    ls.points[0].pos[0] = 0; ls.points[0].pos[1] = 0; ls.points[0].pos[2] = 1;
    ls.points[0].color[0] = 1; ls.points[0].color[1] = 1; ls.points[0].color[2] = 1;
    ls.points[0].range = 0;
    efx_lighting_shade(&mat, &ls, world, n, cam, alb, NULL, out);
    if (!feq(out[0], 1) || !feq(out[1], 1) || !feq(out[2], 1))
        return fail("diffuse maximum");

    /* facing away: zero diffuse */
    float back[3] = {0, 0, -1};
    efx_lighting_shade(&mat, &ls, world, back, cam, alb, NULL, out);
    if (!feq(out[0], 0)) return fail("zero diffuse facing away");

    /* white albedo scaled by tint */
    float grey[4] = {0.5f, 0.5f, 0.5f, 1};
    efx_lighting_shade(&mat, &ls, world, n, cam, grey, NULL, out);
    if (!feq(out[0], 0.5f)) return fail("albedo scales diffuse");

    /* ambient-only flat */
    efx_material amb;
    efx_material_default(&amb);
    amb.ambient[0] = 0.5f; amb.ambient[1] = 0.5f; amb.ambient[2] = 0.5f;
    efx_light_set none;
    memset(&none, 0, sizeof(none));
    efx_lighting_shade(&amb, &none, world, n, cam, alb, NULL, out);
    if (!feq(out[0], 0.5f) || !feq(out[1], 0.5f)) return fail("ambient flat");

    /* emissive unmodulated by a dark albedo */
    efx_material em;
    efx_material_default(&em);
    em.diffuse[0] = em.diffuse[1] = em.diffuse[2] = 0;
    em.emissive[0] = 0.25f;
    float zero[4] = {0, 0, 0, 1};
    efx_lighting_shade(&em, &none, world, n, cam, zero, NULL, out);
    if (!feq(out[0], 0.25f)) return fail("emissive unmodulated");

    /* specular peak: H == N when L == V == N */
    efx_material spec;
    efx_material_default(&spec);
    spec.diffuse[0] = spec.diffuse[1] = spec.diffuse[2] = 0;
    spec.specular[0] = spec.specular[1] = spec.specular[2] = 1;
    spec.shininess = 32;
    efx_lighting_shade(&spec, &ls, world, n, cam, alb, NULL, out);
    if (!feq(out[0], 1)) return fail("specular peak");

    /* attenuation reaches zero at range */
    ls.points[0].pos[2] = 4;   /* distance 4 */
    ls.points[0].range = 4;    /* atten = 1 - 4/4 = 0 */
    efx_lighting_shade(&mat, &ls, world, n, cam, alb, NULL, out);
    if (!feq(out[0], 0)) return fail("atten zero at range");
    ls.points[0].pos[2] = 2;   /* distance 2, range 4 -> atten 0.5 */
    efx_lighting_shade(&mat, &ls, world, n, cam, alb, NULL, out);
    if (!feq(out[0], 0.5f)) return fail("atten half at range/2");

    /* directional-only: dir travels -Z, so L = +Z */
    efx_light_set dls;
    memset(&dls, 0, sizeof(dls));
    dls.directional.enabled = 1;
    dls.directional.dir[0] = 0; dls.directional.dir[1] = 0; dls.directional.dir[2] = -1;
    dls.directional.color[0] = 1; dls.directional.color[1] = 0; dls.directional.color[2] = 0;
    efx_lighting_shade(&mat, &dls, world, n, cam, alb, NULL, out);
    if (!feq(out[0], 1) || !feq(out[1], 0)) return fail("directional");
    return 0;
}

/* F4b: default/explicit/copied map handles (design D5) */
static int material_maps(void) {
    static const uint8_t px[4] = {255, 255, 255, 255};
    float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    uint32_t idx[3] = {0, 1, 2};

    efx_material m;
    efx_material_default(&m);
    if (m.ambient_map || m.diffuse_map || m.specular_map || m.emissive_map ||
        m.alpha_mask)
        return fail("default has no maps");

    install_mock_sink();
    uint64_t tex = efx_render_texture_create(1, 1, px, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    if (!tex) return fail("texture create");
    m.diffuse_map = tex;
    m.alpha_mask = tex;
    efx_material copy = m;
    if (copy.diffuse_map != tex || copy.alpha_mask != tex)
        return fail("copy preserves map handles");

    efx_surface_src s;
    memset(&s, 0, sizeof(s));
    s.positions_len = 9; s.positions = pos;
    s.indices_len = 3; s.indices = idx;
    int err = 0;
    efx_meshdata *md = efx_meshdata_create(&s, 1, &err);
    if (!md) return fail("fixture");
    efx_meshdata_set_material(md, 0, &m, 1);
    if (md->surfaces[0].material.diffuse_map != tex ||
        md->surfaces[0].material.alpha_mask != tex)
        return fail("meshdata records map handles");

    uint64_t h = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    if (!h) return fail("mesh create");
    efx_material got;
    if (efx_render_mesh_surface_material(h, 0, &got) != 1 ||
        got.diffuse_map != tex || got.alpha_mask != tex)
        return fail("mesh carries map handles");
    efx_render_mesh_destroy(h);
    efx_render_texture_destroy(tex);
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

/* F4b: bound maps retain their texture; release drains the count (design D6) */
static int map_retention(void) {
    static const uint8_t px[4] = {255, 255, 255, 255};
    float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    uint32_t idx[3] = {0, 1, 2};

    install_mock_sink();
    uint64_t tex = efx_render_texture_create(1, 1, px, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    if (!tex) return fail("texture create");
    if (efx_render_texture_ref_count(tex) != 0) return fail("fresh ref count");

    efx_surface_src s;
    memset(&s, 0, sizeof(s));
    s.positions_len = 9; s.positions = pos;
    s.indices_len = 3; s.indices = idx;
    int err = 0;
    efx_meshdata *md = efx_meshdata_create(&s, 1, &err);
    if (!md) return fail("fixture");
    efx_material m;
    efx_material_default(&m);
    m.diffuse_map = tex;
    efx_meshdata_set_material(md, 0, &m, 1);
    if (efx_render_texture_ref_count(tex) != 1)
        return fail("meshdata retains map");

    uint64_t h = efx_render_mesh_create(md);
    if (!h) return fail("mesh create");
    if (efx_render_texture_ref_count(tex) != 2)
        return fail("mesh retains map");
    efx_meshdata_destroy(md);
    if (efx_render_texture_ref_count(tex) != 1)
        return fail("meshdata release on destroy");

    /* destroy while bound: script handle dead, native still resolvable */
    if (efx_render_texture_destroy(tex) != EFX_RENDER_OK)
        return fail("destroy while bound");
    if (efx_render_texture_alive(tex)) return fail("dead script handle");
    if (!efx_render_texture_native(tex)) return fail("retained native");
    if (g_tex_destroyed != 0) return fail("native not released yet");

    /* unbind: count drains, deferred native release fires at frame end */
    efx_material plain;
    efx_material_default(&plain);
    if (efx_render_mesh_set_material(h, 0, &plain, 1) != EFX_RENDER_OK)
        return fail("rebind");
    if (efx_render_texture_ref_count(tex) != 0) return fail("ref count drains");
    efx_render_mesh_destroy(h);
    efx_render_end_frame();
    if (g_tex_destroyed != 1) return fail("native released after unbind");
    efx_render_shutdown();
    return 0;
}

/* F4b: CPU reference map modulation and alpha-mask boundary (design D8) */
static int lighting_maps(void) {
    efx_material mat;
    efx_material_default(&mat); /* white diffuse, black ambient/spec/emissive */
    efx_light_set ls;
    memset(&ls, 0, sizeof(ls));
    ls.points[0].enabled = 1;
    ls.points[0].pos[0] = 0; ls.points[0].pos[1] = 0; ls.points[0].pos[2] = 1;
    ls.points[0].color[0] = 1; ls.points[0].color[1] = 1; ls.points[0].color[2] = 1;
    ls.points[0].range = 0;
    float n[3] = {0, 0, 1};
    float world[3] = {0, 0, 0};
    float cam[3] = {0, 0, 5};
    float alb[4] = {1, 1, 1, 1};
    float out[4];
    efx_map_samples neutral;
    for (int c = 0; c < 3; c++) {
        neutral.ambient[c] = 1.0f;
        neutral.diffuse[c] = 1.0f;
        neutral.specular[c] = 1.0f;
        neutral.emissive[c] = 1.0f;
    }
    neutral.mask_alpha = 1.0f;
    neutral.has_mask = 0;

    /* neutral explicit samples equal the NULL (F4a) path */
    float ref[4];
    if (efx_lighting_shade(&mat, &ls, world, n, cam, alb, NULL, ref) != 0)
        return fail("NULL maps not discarded");
    if (efx_lighting_shade(&mat, &ls, world, n, cam, alb, &neutral, out) != 0)
        return fail("neutral maps not discarded");
    if (!feq(out[0], ref[0]) || !feq(out[1], ref[1]) || !feq(out[2], ref[2]))
        return fail("neutral maps equal NULL");

    /* a diffuse map scales exactly the diffuse channel */
    efx_map_samples maps = neutral;
    maps.diffuse[0] = 0.25f; maps.diffuse[1] = 0.5f; maps.diffuse[2] = 0.75f;
    efx_lighting_shade(&mat, &ls, world, n, cam, alb, &maps, out);
    if (!feq(out[0], 0.25f) || !feq(out[1], 0.5f) || !feq(out[2], 0.75f))
        return fail("diffuse map scales diffuse");

    /* an emissive map scales exactly emissive (unmodulated by albedo) */
    efx_material em;
    efx_material_default(&em);
    em.diffuse[0] = em.diffuse[1] = em.diffuse[2] = 0;
    em.emissive[0] = 1.0f;
    efx_light_set none;
    memset(&none, 0, sizeof(none));
    efx_map_samples emap = neutral;
    emap.emissive[0] = 0.5f;
    efx_lighting_shade(&em, &none, world, n, cam, alb, &emap, out);
    if (!feq(out[0], 0.5f)) return fail("emissive map scales emissive");

    /* ambient map scales ambient */
    efx_material am;
    efx_material_default(&am);
    am.ambient[0] = am.ambient[1] = am.ambient[2] = 1.0f;
    efx_map_samples amap = neutral;
    amap.ambient[0] = 0.25f;
    efx_lighting_shade(&am, &none, world, n, cam, alb, &amap, out);
    if (!feq(out[0], 0.25f) || !feq(out[1], 1.0f)) return fail("ambient map scales ambient");

    /* specular map scales exactly the specular peak */
    efx_material sp;
    efx_material_default(&sp);
    sp.diffuse[0] = sp.diffuse[1] = sp.diffuse[2] = 0;
    sp.specular[0] = sp.specular[1] = sp.specular[2] = 1.0f;
    efx_map_samples smap = neutral;
    smap.specular[0] = 0.5f;
    efx_lighting_shade(&sp, &ls, world, n, cam, alb, &smap, out);
    if (!feq(out[0], 0.5f)) return fail("specular map scales specular");

    /* alpha-mask boundary: < 0.5 discards, >= 0.5 keeps the albedo alpha */
    efx_map_samples mask = neutral;
    mask.has_mask = 1;
    mask.mask_alpha = 0.49f;
    if (efx_lighting_shade(&mat, &ls, world, n, cam, alb, &mask, out) != 1)
        return fail("mask below 0.5 discards");
    mask.mask_alpha = 0.5f;
    float half_alpha[4] = {1, 1, 1, 0.5f};
    if (efx_lighting_shade(&mat, &ls, world, n, cam, half_alpha, &mask, out) != 0)
        return fail("mask at 0.5 keeps");
    if (!feq(out[3], 0.5f)) return fail("mask keeps albedo alpha");
    return 0;
}

/* ------------------------------------------------- F5a render targets */

static int render_target_lifecycle(void) {
    install_mock_sink();
    efx_render_set_viewport(640, 480);
    /* validation: 0 / negative / oversized rejected */
    if (efx_render_target_create(0, 64) != 0) return fail("zero width accepted");
    if (efx_render_target_create(64, -1) != 0) return fail("negative height accepted");
    if (efx_render_target_create(EFX_RENDER_MAX_TARGET_SIZE + 1, 8) != 0)
        return fail("oversize accepted");
    uint64_t rt = efx_render_target_create(512, 256);
    if (!rt) return fail("create");
    if (!efx_render_target_alive(rt)) return fail("alive after create");
    int w = 0, h = 0;
    efx_render_target_size(rt, &w, &h);
    if (w != 512 || h != 256) return fail("size");
    if (!efx_render_sample_alive(rt)) return fail("sample alive");
    efx_render_sample_size(rt, &w, &h);
    if (w != 512 || h != 256) return fail("sample size");
    if (efx_render_sample_native(rt) == NULL) return fail("sample native");
    /* destroyed handles and unknown handles do not resolve */
    if (efx_render_target_alive(0xdeadbeef)) return fail("stale handle alive");
    if (efx_render_target_destroy(rt) != EFX_RENDER_OK) return fail("destroy");
    if (efx_render_target_alive(rt)) return fail("alive after destroy");
    if (efx_render_target_destroy(rt) != EFX_RENDER_OK) return fail("destroy idempotent");
    efx_render_end_frame();
    if (g_rt_created != 1 || g_rt_destroyed != 1) return fail("sink create/destroy counts");
    efx_render_shutdown();
    return 0;
}

static int target_deferred_release(void) {
    install_mock_sink();
    uint64_t rt = efx_render_target_create(64, 64);
    uint64_t tex = efx_render_texture_create(4, 4, NULL, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    if (!rt || !tex) return fail("fixtures");
    /* binding an RT as a material map retains it (F4b rule extended) */
    efx_meshdata *md = make_two_surface_mesh();
    uint64_t mesh = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    if (!mesh) return fail("mesh");
    efx_material mat;
    efx_material_default(&mat);
    mat.diffuse_map = rt;
    if (efx_render_mesh_set_material(mesh, 0, &mat, 1) != EFX_RENDER_OK)
        return fail("bind");
    if (efx_render_target_ref_count(rt) != 1) return fail("bind ref count");
    /* destroy while bound: the script handle dies immediately, but the
       native storage is retained until the binding releases (the F4b
       texture rule, extended) */
    if (efx_render_target_destroy(rt) != EFX_RENDER_OK) return fail("destroy bound");
    if (efx_render_target_alive(rt)) return fail("alive after destroy");
    if (efx_render_target_ref_count(rt) != 1) return fail("ref count lost");
    if (g_rt_destroyed != 0) return fail("native released while bound");
    /* unbinding releases; native release lands at frame end */
    if (efx_render_mesh_set_material(mesh, 0, NULL, 0) != EFX_RENDER_OK)
        return fail("unbind");
    if (efx_render_target_ref_count(rt) != 0) return fail("ref count after unbind");
    efx_render_end_frame();
    if (g_rt_destroyed != 1) return fail("native release after unbind");
    efx_render_shutdown();
    return 0;
}

static int segmentation(void) {
    install_mock_sink();
    efx_render_set_viewport(640, 480);
    uint64_t rt = efx_render_target_create(64, 64);
    uint64_t tex = efx_render_texture_create(4, 4, NULL, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    if (!rt || !tex) return fail("fixtures");
    /* screen quad, target segment, screen quad again */
    efx_render_quad(0, 0, 8, 8, tex, NULL, 0, 1, NULL, 0, 4, 4);
    if (efx_render_begin_target(rt) != EFX_RENDER_OK) return fail("begin");
    efx_render_quad(0, 0, 8, 8, tex, NULL, 0, 1, NULL, 0, 4, 4);
    if (efx_render_end_target() != EFX_RENDER_OK) return fail("end");
    efx_render_quad(0, 0, 8, 8, tex, NULL, 0, 1, NULL, 0, 4, 4);

    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    if (count != 5) return fail("record count");
    if (recs[0].type != EFX_RECORD_QUAD || recs[0].target != 0)
        return fail("screen quad before segment");
    if (recs[1].type != EFX_RECORD_BEGIN_TARGET || recs[1].target != rt)
        return fail("begin control record");
    if (recs[2].type != EFX_RECORD_QUAD || recs[2].target != rt)
        return fail("quad carries target tag");
    if (recs[3].type != EFX_RECORD_END_TARGET || recs[3].target != rt)
        return fail("end control record");
    if (recs[4].type != EFX_RECORD_QUAD || recs[4].target != 0)
        return fail("screen quad after segment");
    /* keys equal record order */
    for (int i = 0; i < count; i++) {
        if (recs[i].sort_key != (uint32_t)i) return fail("sort keys");
    }
    /* active target follows begin/end */
    if (efx_render_active_target() != 0) return fail("active after end");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int target_redirection(void) {
    install_mock_sink();
    efx_render_set_viewport(1024, 600);
    uint64_t rt = efx_render_target_create(256, 128);
    if (!rt) return fail("create");
    /* the default camera frame follows the active target */
    if (efx_render_begin_target(rt) != EFX_RENDER_OK) return fail("begin");
    efx_render_quad(0, 0, 8, 8, 0, NULL, 0, 1, NULL, 0, 4, 4);
    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    if (recs[count - 1].u.quad.frame_w != 256 ||
        recs[count - 1].u.quad.frame_h != 128)
        return fail("default frame follows target");
    /* the BEGIN record value-snapshots the clear color (design D3) */
    float blue[4] = {0, 0, 1, 1};
    efx_render_set_clear_color(blue);
    if (efx_render_end_target() != EFX_RENDER_OK) return fail("end");
    if (efx_render_begin_target(rt) != EFX_RENDER_OK) return fail("begin 2");
    float red[4] = {1, 0, 0, 1};
    efx_render_set_clear_color(red); /* later change must not leak backward */
    if (efx_render_end_target() != EFX_RENDER_OK) return fail("end 2");
    recs = efx_render_records(&count);
    int begins = 0;
    for (int i = 0; i < count; i++) {
        if (recs[i].type == EFX_RECORD_BEGIN_TARGET) {
            const float *c = recs[i].u.begin_target.clear;
            if (begins == 1 && !feq(c[2], 1.0f)) return fail("first clear snapshot");
            if (begins == 2 && !feq(c[0], 1.0f)) return fail("second clear snapshot");
            begins++;
        }
    }
    if (begins != 2) return fail("begin count");
    /* error states: nesting and unbalanced end */
    if (efx_render_begin_target(rt) != EFX_RENDER_OK) return fail("begin 3");
    if (efx_render_begin_target(rt) != EFX_RENDER_ERR_NESTED) return fail("nested accepted");
    if (efx_render_end_target() != EFX_RENDER_OK) return fail("end 3");
    if (efx_render_end_target() != EFX_RENDER_ERR_STATE) return fail("unbalanced accepted");
    /* handle validation */
    if (efx_render_begin_target(0x1234) != EFX_RENDER_ERR_HANDLE)
        return fail("bad handle accepted");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int feedback_guard(void) {
    install_mock_sink();
    uint64_t rt = efx_render_target_create(64, 64);
    uint64_t tex = efx_render_texture_create(4, 4, NULL, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    if (!rt || !tex) return fail("fixtures");
    efx_meshdata *md = make_two_surface_mesh();
    uint64_t mesh = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    if (!mesh) return fail("mesh");
    /* off-target draws are unaffected */
    if (efx_render_quad(0, 0, 8, 8, rt, NULL, 0, 1, NULL, 0, 4, 4) != EFX_RENDER_OK)
        return fail("sampling a non-active target must succeed");
    /* quad sampling the active target is rejected at record time */
    if (efx_render_begin_target(rt) != EFX_RENDER_OK) return fail("begin");
    int before = 0;
    efx_render_records(&before);
    if (efx_render_quad(0, 0, 8, 8, rt, NULL, 0, 1, NULL, 0, 4, 4) !=
        EFX_RENDER_ERR_FEEDBACK)
        return fail("quad feedback accepted");
    /* a mesh whose maps sample the active target is rejected too */
    efx_material mat;
    efx_material_default(&mat);
    mat.diffuse_map = rt;
    efx_render_mesh_set_material(mesh, 0, &mat, 1);
    if (efx_render_mesh(mesh, NULL, NULL, 0) != EFX_RENDER_ERR_FEEDBACK)
        return fail("mesh feedback accepted");
    int after = 0;
    efx_render_records(&after);
    if (before != after) return fail("rejected draw recorded something");
    if (efx_render_end_target() != EFX_RENDER_OK) return fail("end");
    /* outside the segment the same draws record */
    if (efx_render_quad(0, 0, 8, 8, rt, NULL, 0, 1, NULL, 0, 4, 4) != EFX_RENDER_OK)
        return fail("sampling after end must succeed");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int post_registry(void) {
    install_mock_sink();
    efx_render_set_viewport(640, 480);
    efx_post_entry e;
    memset(&e, 0, sizeof(e));
    e.effect = EFX_POST_COLOR_FILTER;
    e.mix = 1.0f;
    e.u.color_filter.brightness = 1.0f;
    e.u.color_filter.contrast = 1.0f;
    e.u.color_filter.saturation = 1.0f;
    e.u.color_filter.tint[0] = 1.0f;
    e.u.color_filter.tint[1] = 1.0f;
    e.u.color_filter.tint[2] = 1.0f;
    e.u.color_filter.tint[3] = 1.0f;
    if (efx_render_set_post_effects(&e, 1) != EFX_POST_OK)
        return fail("valid colorFilter rejected");
    efx_post_entry got[EFX_POST_MAX_ENTRIES];
    int n = 0;
    efx_render_post_effects(got, &n);
    if (n != 1 || got[0].effect != EFX_POST_COLOR_FILTER)
        return fail("stored chain");
    /* unknown effect is rejected and leaves the previous chain */
    efx_post_entry bad = e;
    bad.effect = 99;
    if (efx_render_set_post_effects(&bad, 1) != EFX_POST_ERR_UNKNOWN)
        return fail("unknown effect accepted");
    efx_render_post_effects(got, &n);
    if (n != 1 || got[0].effect != EFX_POST_COLOR_FILTER)
        return fail("previous chain lost on unknown effect");
    /* count cap */
    efx_post_entry many[EFX_POST_MAX_ENTRIES + 1];
    for (int i = 0; i <= EFX_POST_MAX_ENTRIES; i++) many[i] = e;
    if (efx_render_set_post_effects(many, EFX_POST_MAX_ENTRIES + 1) !=
        EFX_POST_ERR_COUNT)
        return fail("over-long chain accepted");
    /* option bounds */
    efx_post_entry blur;
    memset(&blur, 0, sizeof(blur));
    blur.effect = EFX_POST_BLUR;
    blur.mix = 1.0f;
    blur.u.blur.radius = 0.0f;
    if (efx_render_set_post_effects(&blur, 1) != EFX_POST_ERR_RANGE)
        return fail("radius 0 accepted");
    blur.u.blur.radius = 65.0f;
    if (efx_render_set_post_effects(&blur, 1) != EFX_POST_ERR_RANGE)
        return fail("radius 65 accepted");
    blur.u.blur.radius = 4.0f;
    if (efx_render_set_post_effects(&blur, 1) != EFX_POST_OK)
        return fail("valid blur rejected");
    efx_post_entry bloom;
    memset(&bloom, 0, sizeof(bloom));
    bloom.effect = EFX_POST_BLOOM;
    bloom.mix = 1.0f;
    bloom.u.bloom.threshold = 1.5f;
    bloom.u.bloom.strength = 0.5f;
    if (efx_render_set_post_effects(&bloom, 1) != EFX_POST_ERR_RANGE)
        return fail("threshold 1.5 accepted");
    bloom.u.bloom.threshold = 0.8f;
    bloom.u.bloom.strength = 1.5f;
    if (efx_render_set_post_effects(&bloom, 1) != EFX_POST_ERR_RANGE)
        return fail("strength 1.5 accepted");
    efx_post_entry neg = e;
    neg.u.color_filter.brightness = -1.0f;
    if (efx_render_set_post_effects(&neg, 1) != EFX_POST_ERR_RANGE)
        return fail("negative brightness accepted");
    neg = e;
    neg.u.color_filter.tint[0] = 2.0f;
    if (efx_render_set_post_effects(&neg, 1) != EFX_POST_ERR_RANGE)
        return fail("tint > 1 accepted");
    neg = e;
    neg.mix = 1.5f;
    if (efx_render_set_post_effects(&neg, 1) != EFX_POST_ERR_RANGE)
        return fail("mix > 1 accepted");
    /* snapshot: the caller's buffer is copied in, not referenced */
    e.u.color_filter.brightness = 1.0f;
    efx_render_set_post_effects(&e, 1);
    e.u.color_filter.brightness = 0.25f;
    efx_render_post_effects(got, &n);
    if (!feq(got[0].u.color_filter.brightness, 1.0f))
        return fail("entry not snapshotted");
    /* clear */
    if (efx_render_set_post_effects(NULL, 0) != EFX_POST_OK)
        return fail("clear failed");
    efx_render_post_effects(got, &n);
    if (n != 0) return fail("chain not cleared");
    efx_render_shutdown();
    return 0;
}

static int post_fast_path(void) {
    install_mock_sink();
    efx_render_set_viewport(640, 480);
    /* nothing set: inactive, and no scene target is ever allocated */
    if (efx_render_post_active()) return fail("active with defaults");
    if (efx_render_post_scene_handle() != 0)
        return fail("scene target allocated on the fast path");
    efx_post_entry e;
    memset(&e, 0, sizeof(e));
    e.effect = EFX_POST_BLUR;
    e.mix = 1.0f;
    e.u.blur.radius = 2.0f;
    if (efx_render_set_post_effects(&e, 1) != EFX_POST_OK)
        return fail("set blur");
    if (!efx_render_post_active()) return fail("chain did not engage");
    /* the scene target is allocated lazily by the resolve (here, directly) */
    uint64_t scene = efx_render_post_scene_target(320, 240);
    if (!scene) return fail("scene target create");
    if (efx_render_post_scene_handle() != scene)
        return fail("scene handle mismatch");
    /* clearing the chain returns to the fast path (scale still 1) */
    efx_render_set_post_effects(NULL, 0);
    if (efx_render_post_active()) return fail("active after clear");
    /* a render scale alone engages the scene target */
    if (efx_render_set_render_scale(0.5f, EFX_FILTER_NEAREST) != EFX_POST_OK)
        return fail("set scale");
    if (!efx_render_post_active()) return fail("scale did not engage");
    efx_render_set_render_scale(1.0f, EFX_FILTER_LINEAR);
    if (efx_render_post_active()) return fail("scale 1 still active");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int post_render_scale(void) {
    install_mock_sink();
    if (efx_render_set_render_scale(0.0f, EFX_FILTER_LINEAR) !=
        EFX_POST_ERR_RANGE)
        return fail("scale 0 accepted");
    if (efx_render_set_render_scale(2.5f, EFX_FILTER_LINEAR) !=
        EFX_POST_ERR_RANGE)
        return fail("scale 2.5 accepted");
    if (efx_render_set_render_scale(-1.0f, EFX_FILTER_LINEAR) !=
        EFX_POST_ERR_RANGE)
        return fail("negative scale accepted");
    if (efx_render_set_render_scale(INFINITY, EFX_FILTER_LINEAR) !=
        EFX_POST_ERR_RANGE)
        return fail("infinite scale accepted");
    if (efx_render_set_render_scale(1.0f, 99) != EFX_POST_ERR_FILTER)
        return fail("unknown filter accepted");
    if (efx_render_set_render_scale(0.5f, EFX_FILTER_NEAREST) != EFX_POST_OK)
        return fail("valid scale rejected");
    float sc = 0;
    int f = -1;
    efx_render_render_scale(&sc, &f);
    if (!feq(sc, 0.5f) || f != EFX_FILTER_NEAREST)
        return fail("scale state");
    /* a rejected call leaves the previous scale in effect */
    if (efx_render_set_render_scale(0.0f, EFX_FILTER_LINEAR) !=
        EFX_POST_ERR_RANGE)
        return fail("second scale 0 accepted");
    efx_render_render_scale(&sc, &f);
    if (!feq(sc, 0.5f) || f != EFX_FILTER_NEAREST)
        return fail("scale state changed on failure");
    efx_render_shutdown();
    return 0;
}

static int post_surface_size(void) {
    install_mock_sink();
    efx_render_set_viewport(640, 480);
    int w = 0, h = 0;
    efx_render_surface_size(&w, &h);
    if (w != 640 || h != 480) return fail("default surface size");
    efx_render_set_render_scale(0.5f, EFX_FILTER_LINEAR);
    efx_render_surface_size(&w, &h);
    if (w != 320 || h != 240) return fail("half surface size");
    efx_render_set_render_scale(1.5f, EFX_FILTER_LINEAR);
    efx_render_surface_size(&w, &h);
    if (w != 960 || h != 720) return fail("upscaled surface size");
    /* a chain alone (scale 1) keeps the surface size */
    efx_render_set_render_scale(1.0f, EFX_FILTER_LINEAR);
    efx_post_entry e;
    memset(&e, 0, sizeof(e));
    e.effect = EFX_POST_BLUR;
    e.mix = 1.0f;
    e.u.blur.radius = 3.0f;
    efx_render_set_post_effects(&e, 1);
    efx_render_surface_size(&w, &h);
    if (w != 640 || h != 480) return fail("chain changed surface size");
    /* the active render target still wins (segments render raw) */
    uint64_t rt = efx_render_target_create(100, 50);
    efx_render_begin_target(rt);
    efx_render_surface_size(&w, &h);
    if (w != 100 || h != 50) return fail("active target surface size");
    efx_render_end_target();
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int post_user_target_raw(void) {
    install_mock_sink();
    efx_render_set_viewport(640, 480);
    uint64_t rt = efx_render_target_create(64, 64);
    uint64_t tex = efx_render_texture_create(4, 4, NULL, EFX_TEX_WRAP_REPEAT, EFX_FILTER_LINEAR, 0);
    if (!rt || !tex) return fail("fixtures");
    efx_post_entry e;
    memset(&e, 0, sizeof(e));
    e.effect = EFX_POST_BLUR;
    e.mix = 1.0f;
    e.u.blur.radius = 4.0f;
    if (efx_render_set_post_effects(&e, 1) != EFX_POST_OK)
        return fail("set chain");
    if (!efx_render_post_active()) return fail("chain not active");
    /* a chained frame: a screen quad, a user-target segment, then a screen
       sample of that target */
    efx_render_quad(0, 0, 8, 8, tex, NULL, 0, 1, NULL, 0, 4, 4);
    if (efx_render_begin_target(rt) != EFX_RENDER_OK) return fail("begin");
    efx_render_quad(0, 0, 8, 8, tex, NULL, 0, 1, NULL, 0, 4, 4);
    if (efx_render_end_target() != EFX_RENDER_OK) return fail("end");
    efx_render_quad(0, 0, 64, 64, rt, NULL, 0, 1, NULL, 0, 32, 32);

    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    int saw_begin = 0, saw_end = 0, seg_quads = 0, screen_quads = 0;
    for (int i = 0; i < count; i++) {
        if (recs[i].type == EFX_RECORD_BEGIN_TARGET) {
            if (recs[i].target != rt) return fail("begin target tag");
            saw_begin = 1;
        } else if (recs[i].type == EFX_RECORD_END_TARGET) {
            if (recs[i].target != rt) return fail("end target tag");
            saw_end = 1;
        } else if (recs[i].type == EFX_RECORD_QUAD) {
            if (recs[i].target == rt) {
                seg_quads++;
            } else if (recs[i].target == 0) {
                screen_quads++;
            } else {
                return fail("unexpected quad target");
            }
        }
    }
    if (!saw_begin || !saw_end) return fail("segment controls missing");
    /* the user segment renders raw (its own records, its own target); only the
       two default-surface records go through the scene-target resolve */
    if (seg_quads != 1) return fail("segment quad not tagged to its target");
    if (screen_quads != 2) return fail("default-surface quads");
    /* the chain is engine state and the default surface stays the scene size */
    int w = 0, h = 0;
    efx_render_surface_size(&w, &h);
    if (w != 640 || h != 480) return fail("chained default surface size");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

/* ------------------------------------------------------- F7 skinning */

/* design D1: bind-local reconstruction from inverse bind matrices */
static int skin_bind_local(void) {
    int nodes[2] = {1, 2};
    int parents[2] = {-1, 0};
    float ib[32];
    memset(ib, 0, sizeof(ib));
    ib[0] = ib[5] = ib[10] = ib[15] = 1.0f;
    ib[13] = -1.0f; /* joint 0 world bind T(0,1,0) */
    ib[16] = ib[21] = ib[26] = ib[31] = 1.0f;
    ib[16 + 13] = -2.0f; /* joint 1 world bind T(0,2,0) */
    efx_rig rig;
    memset(&rig, 0, sizeof(rig));
    rig.joint_count = 2;
    rig.joint_nodes = nodes;
    rig.joint_parents = parents;
    rig.inverse_bind = ib;

    float local[32];
    if (efx_skin_bind_local(&rig, local) != 0) return fail("bind local");
    if (!feq(local[13], 1.0f) || !feq(local[16 + 13], 1.0f))
        return fail("bind local translations");
    if (!feq(local[0], 1.0f) || !feq(local[16 + 0], 1.0f))
        return fail("bind local identity");

    /* a singular inverse_bind falls back to identity for that joint */
    memset(ib, 0, 16 * sizeof(float));
    ib[16] = ib[21] = ib[26] = ib[31] = 1.0f;
    ib[16 + 13] = -2.0f;
    if (efx_skin_bind_local(&rig, local) != 0) return fail("bind local 2");
    if (!feq(local[0], 1.0f) || !feq(local[13], 0.0f))
        return fail("singular identity fallback");
    if (!feq(local[16 + 13], 2.0f))
        return fail("singular parent fallback");
    return 0;
}

/* design D2/D5: LINEAR/STEP sampling with time wrapping */
static int skin_sampling(void) {
    int nodes[1] = {0};
    int parents[1] = {-1};
    float ib[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    static float ttimes[3] = {0.0f, 0.5f, 1.0f};
    static float tvals[9] = {0, 0, 0, 0, 2, 0, 0, 4, 0};
    efx_anim_channel tch;
    memset(&tch, 0, sizeof(tch));
    tch.target_node = 0;
    tch.path = EFX_ANIM_PATH_TRANSLATION;
    tch.interpolation = EFX_ANIM_INTERP_LINEAR;
    tch.components = 3;
    tch.times_len = 3;
    tch.values_len = 9;
    tch.times = ttimes;
    tch.values = tvals;
    efx_animation_clip clip = {"move", 1, &tch};
    efx_rig rig;
    memset(&rig, 0, sizeof(rig));
    rig.joint_count = 1;
    rig.joint_nodes = nodes;
    rig.joint_parents = parents;
    rig.inverse_bind = ib;
    rig.clip_count = 1;
    rig.clips = &clip;

    float palette[16];
    efx_pose_sample s = {0, 0.25f, 1.0f};
    if (efx_skin_evaluate(&rig, &s, 1, palette) != 0) return fail("eval");
    if (!feq(palette[13], 1.0f)) return fail("linear midpoint");
    s.time = 0.75f;
    efx_skin_evaluate(&rig, &s, 1, palette);
    if (!feq(palette[13], 3.0f)) return fail("linear three-quarter");
    s.time = 1.25f; /* wraps to 0.25 */
    efx_skin_evaluate(&rig, &s, 1, palette);
    if (!feq(palette[13], 1.0f)) return fail("time wrap");
    s.time = 1.0f; /* exactly the clip length wraps back to 0 */
    efx_skin_evaluate(&rig, &s, 1, palette);
    if (!feq(palette[13], 0.0f)) return fail("clip length wraps");

    /* STEP holds the previous keyframe until the next one; the clip length is
       the max over channels, so the rotation channel's second key is reachable
       within [0.5, 1) */
    static float rtimes[2] = {0.0f, 0.5f};
    static float rvals[8] = {0, 0, 0, 1, 0, 0, 0.70710678f, 0.70710678f};
    efx_anim_channel rch;
    memset(&rch, 0, sizeof(rch));
    rch.target_node = 0;
    rch.path = EFX_ANIM_PATH_ROTATION;
    rch.interpolation = EFX_ANIM_INTERP_STEP;
    rch.components = 4;
    rch.times_len = 2;
    rch.values_len = 8;
    rch.times = rtimes;
    rch.values = rvals;
    efx_anim_channel both[2] = {tch, rch};
    efx_animation_clip turn = {"turn", 2, both};
    rig.clips = &turn;
    s.time = 0.25f;
    efx_skin_evaluate(&rig, &s, 1, palette);
    if (!feq(palette[0], 1.0f) || !feq(palette[1], 0.0f))
        return fail("step hold");
    s.time = 0.75f;
    efx_skin_evaluate(&rig, &s, 1, palette);
    /* 90 degrees about z: column 0 = (0,1,0), column 1 = (-1,0,0) */
    if (!feq(palette[0], 0.0f) || !feq(palette[1], 1.0f) ||
        !feq(palette[4], -1.0f) || !feq(palette[5], 0.0f))
        return fail("step next keyframe");
    return 0;
}

/* design D3/D4: LBS with per-vertex weight normalization */
static int skin_lbs(void) {
    float bind[12] = {1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1};
    float out[12];
    uint32_t joints[4] = {0, 1, 0, 0};
    float palette[32];
    memset(palette, 0, sizeof(palette));
    palette[0] = palette[5] = palette[10] = palette[15] = 1.0f;
    palette[12] = 1.0f; /* joint 0: T(1,0,0) */
    palette[16] = palette[21] = palette[26] = palette[31] = 1.0f;
    palette[16 + 13] = 2.0f; /* joint 1: T(0,2,0) */

    float w[4] = {0.25f, 0.75f, 0.0f, 0.0f};
    efx_skin_surface(bind, 1, joints, w, palette, 2, out);
    if (!feq(out[0], 1.25f) || !feq(out[1], 2.5f) || !feq(out[2], 1.0f))
        return fail("weighted position");
    if (!feq(out[3], 0.0f) || !feq(out[4], 1.0f) || !feq(out[5], 0.0f))
        return fail("weighted normal");

    /* zero-sum vertex stays at bind */
    float z[4] = {0, 0, 0, 0};
    efx_skin_surface(bind, 1, joints, z, palette, 2, out);
    if (!feq(out[0], 1.0f) || !feq(out[1], 1.0f)) return fail("zero-sum bind");

    /* weights normalize: {1,1} behaves like {0.5,0.5} */
    float w2[4] = {1.0f, 1.0f, 0.0f, 0.0f};
    efx_skin_surface(bind, 1, joints, w2, palette, 2, out);
    if (!feq(out[0], 1.5f) || !feq(out[1], 2.0f))
        return fail("weight normalization");
    return 0;
}

/* design D5: single sample vs weighted blend, negative weight rejected */
static int skin_pose_blend(void) {
    int nodes[1] = {0};
    int parents[1] = {-1};
    float ib[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    static float t0[3] = {0, 1, 0};
    static float t1[3] = {0, 3, 0};
    static float times[1] = {0.0f};
    efx_anim_channel c0, c1;
    memset(&c0, 0, sizeof(c0));
    memset(&c1, 0, sizeof(c1));
    c0.target_node = c1.target_node = 0;
    c0.path = c1.path = EFX_ANIM_PATH_TRANSLATION;
    c0.components = c1.components = 3;
    c0.times_len = c1.times_len = 1;
    c0.values_len = c1.values_len = 3;
    c0.times = c1.times = times;
    c0.values = t0;
    c1.values = t1;
    efx_animation_clip clips[2] = {{"a", 1, &c0}, {"b", 1, &c1}};
    efx_rig rig;
    memset(&rig, 0, sizeof(rig));
    rig.joint_count = 1;
    rig.joint_nodes = nodes;
    rig.joint_parents = parents;
    rig.inverse_bind = ib;
    rig.clip_count = 2;
    rig.clips = clips;

    float palette[16];
    efx_pose_sample samples[2] = {{0, 0.0f, 1.0f}, {1, 0.0f, 3.0f}};
    if (efx_skin_evaluate(&rig, samples, 2, palette) != 0)
        return fail("blend eval");
    if (!feq(palette[13], 2.5f)) return fail("weighted blend value");

    /* a single sample ignores its weight */
    samples[0].weight = 100.0f;
    efx_skin_evaluate(&rig, &samples[0], 1, palette);
    if (!feq(palette[13], 1.0f)) return fail("single sample weight ignored");

    /* negative weight rejected */
    efx_pose_sample bad[2] = {{0, 0.0f, 1.0f}, {1, 0.0f, -0.5f}};
    if (efx_skin_evaluate(&rig, bad, 2, palette) != -3)
        return fail("negative weight accepted");
    return 0;
}

/* --------------------------------------------- F7 renderer integration */

static efx_rig *make_test_rig(void) {
    efx_rig *rig = calloc(1, sizeof(efx_rig));
    if (!rig) return NULL;
    rig->joint_count = 1;
    rig->joint_nodes = malloc(sizeof(int));
    rig->joint_parents = malloc(sizeof(int));
    rig->inverse_bind = calloc(16, sizeof(float));
    if (!rig->joint_nodes || !rig->joint_parents || !rig->inverse_bind) {
        efx_rig_free(rig);
        return NULL;
    }
    rig->joint_nodes[0] = 0;
    rig->joint_parents[0] = -1;
    rig->inverse_bind[0] = rig->inverse_bind[5] = 1.0f;
    rig->inverse_bind[10] = rig->inverse_bind[15] = 1.0f;

    float *times = malloc(sizeof(float));
    float *vals = malloc(3 * sizeof(float));
    efx_anim_channel *ch = calloc(1, sizeof(efx_anim_channel));
    rig->clips = calloc(1, sizeof(efx_animation_clip));
    if (!times || !vals || !ch || !rig->clips) {
        free(times);
        free(vals);
        free(ch);
        efx_rig_free(rig);
        return NULL;
    }
    times[0] = 0.0f;
    vals[0] = 0;
    vals[1] = 1;
    vals[2] = 0;
    ch->target_node = 0;
    ch->path = EFX_ANIM_PATH_TRANSLATION;
    ch->interpolation = EFX_ANIM_INTERP_LINEAR;
    ch->components = 3;
    ch->times_len = 1;
    ch->values_len = 3;
    ch->times = times;
    ch->values = vals;
    rig->clip_count = 1;
    rig->clips[0].name = malloc(5);
    memcpy(rig->clips[0].name, "move", 5);
    rig->clips[0].channel_count = 1;
    rig->clips[0].channels = ch;
    return rig;
}

static efx_meshdata *make_skinned_mesh(void) {
    static const float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    static const uint32_t joints[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    static const float weights[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    static const uint32_t idx[3] = {0, 1, 2};
    efx_surface_src s;
    memset(&s, 0, sizeof(s));
    s.positions_len = 9;
    s.positions = pos;
    s.joints_len = 12;
    s.joints = joints;
    s.weights_len = 12;
    s.weights = weights;
    s.indices_len = 3;
    s.indices = idx;
    int err = 0;
    efx_meshdata *md = efx_meshdata_create(&s, 1, &err);
    if (!md) return NULL;
    efx_rig *rig = make_test_rig();
    if (!rig) {
        efx_meshdata_destroy(md);
        return NULL;
    }
    efx_meshdata_set_rig(md, rig);
    return md;
}

/* design D4: posed buffers exist only for skinned meshes */
static int mesh_skin_buffers(void) {
    install_mock_sink();
    efx_meshdata *md = make_skinned_mesh();
    if (!md) return fail("skinned fixture");
    uint64_t m = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    if (!m) return fail("skinned mesh create");
    if (!efx_render_mesh_skinned(m)) return fail("skinned flag");
    if (!efx_render_mesh_posed(m, 0)) return fail("posed array");
    if (efx_render_mesh_posed(m, 5)) return fail("posed out of range");
    if (g_last_mesh_skinned_total != 1) return fail("gpu skinned surface flag");

    efx_meshdata *smd = make_two_surface_mesh();
    uint64_t sm = efx_render_mesh_create(smd);
    efx_meshdata_destroy(smd);
    if (!sm) return fail("static create");
    if (efx_render_mesh_skinned(sm)) return fail("static marked skinned");
    if (efx_render_mesh_posed(sm, 0)) return fail("static posed array");
    if (g_last_mesh_skinned_total != 0) return fail("static gpu flag");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

/* design D4/D5: posing writes only the posed array; bind data is retained */
static int mesh_pose_repose(void) {
    install_mock_sink();
    efx_meshdata *md = make_skinned_mesh();
    if (!md) return fail("skinned fixture");
    uint64_t m = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    if (!m) return fail("create");
    const float *bind = efx_render_mesh_posed(m, 0);
    if (!bind || !feq(bind[1], 0.0f)) return fail("initial bind copy");

    uint32_t rev0 = efx_render_mesh_pose_revision(m);
    efx_pose_sample s = {0, 0.0f, 1.0f};
    if (efx_render_mesh_pose(m, &s, 1) != EFX_RENDER_OK) return fail("pose");
    const float *posed = efx_render_mesh_posed(m, 0);
    if (!feq(posed[1], 1.0f)) return fail("posed vertex");
    if (efx_render_mesh_pose_revision(m) == rev0) return fail("revision bumped");
    /* an empty pose returns to the retained bind pose */
    if (efx_render_mesh_pose(m, NULL, 0) != EFX_RENDER_OK) return fail("repose");
    posed = efx_render_mesh_posed(m, 0);
    if (!feq(posed[1], 0.0f)) return fail("bind retained");

    /* rig-less mesh rejects posing and skinned drawing */
    efx_meshdata *smd = make_two_surface_mesh();
    uint64_t sm = efx_render_mesh_create(smd);
    efx_meshdata_destroy(smd);
    if (efx_render_mesh_pose(sm, &s, 1) != EFX_RENDER_ERR_HANDLE)
        return fail("rig-less pose accepted");
    if (efx_render_mesh_skinned(sm)) return fail("rig-less marked skinned");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

/* design D4: the skinned flag is snapshotted per record and validated */
static int mesh_skinned_flag(void) {
    install_mock_sink();
    efx_meshdata *md = make_skinned_mesh();
    if (!md) return fail("skinned fixture");
    uint64_t m = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    efx_meshdata *smd = make_two_surface_mesh();
    uint64_t sm = efx_render_mesh_create(smd);
    efx_meshdata_destroy(smd);

    if (efx_render_mesh(sm, NULL, NULL, 1) != EFX_RENDER_ERR_RIG)
        return fail("static skinned draw accepted");
    int count = 0;
    efx_render_records(&count);
    if (count != 0) return fail("rejected draw recorded");

    if (efx_render_mesh(m, NULL, NULL, 1) != EFX_RENDER_OK)
        return fail("skinned draw");
    if (efx_render_mesh(m, NULL, NULL, 0) != EFX_RENDER_OK)
        return fail("bind draw");
    const efx_record *recs = efx_render_records(&count);
    if (count != 2) return fail("record count");
    if (recs[0].u.mesh.skinned != 1 || recs[1].u.mesh.skinned != 0)
        return fail("skinned record flag");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

/* design D4: destruction releases the posed buffer and later use throws */
static int mesh_skin_lifecycle(void) {
    install_mock_sink();
    efx_meshdata *md = make_skinned_mesh();
    if (!md) return fail("skinned fixture");
    uint64_t m = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    efx_pose_sample s = {0, 0.0f, 1.0f};
    if (efx_render_mesh_pose(m, &s, 1) != EFX_RENDER_OK) return fail("pose");
    if (efx_render_mesh_destroy(m) != EFX_RENDER_OK) return fail("destroy");
    if (efx_render_mesh_alive(m)) return fail("alive after destroy");
    if (efx_render_mesh_posed(m, 0) != NULL) return fail("posed after destroy");
    if (efx_render_mesh_pose(m, &s, 1) != EFX_RENDER_ERR_HANDLE)
        return fail("pose after destroy");
    efx_render_end_frame();
    if (g_mesh_destroyed != 1) return fail("native not released");
    efx_render_shutdown();
    return 0;
}

/* ---------------------------------------------------- F11 particles/billboards */

static uint64_t make_test_texture(void) {
    static const uint8_t px[16] = {255, 0, 0, 255, 0, 255, 0, 255,
                                   0, 0, 255, 255, 255, 255, 255, 255};
    return efx_render_texture_create(2, 2, px, EFX_TEX_WRAP_CLAMP,
                                     EFX_FILTER_NEAREST, 0);
}

static void base_config(efx_particle_config *c, uint64_t tex) {
    memset(c, 0, sizeof(*c));
    c->texture = tex;
    c->max = 64;
    c->space = EFX_SPACE_WORLD;
    c->facing = EFX_FACING_VIEW;
    c->blend = EFX_BLEND_ALPHA;
    c->normal[1] = 1.0f;
    c->life_min = c->life_max = 1.0f;
    c->emitter_lifetime = -1.0f;
    c->direction[1] = 1.0f;
    c->size_count = 1;
    c->sizes[0] = 1.0f;
    c->color_count = 1;
    c->colors[0][0] = c->colors[0][1] = c->colors[0][2] = c->colors[0][3] = 1.0f;
    c->insert_mode = EFX_INSERT_TOP;
    c->speed_scale = 1.0f;
    c->shape = EFX_SHAPE_POINT;
}

static int billboard_basis(void) {
    efx_camera3d cam = {{0, 0, 5}, {0, 0, 0}, 60, 0.1f, 100};
    float r[3], u[3];
    efx_render_billboard_basis(&cam, EFX_FACING_VIEW, NULL, r, u);
    if (!feq(r[0], 1) || !feq(r[1], 0) || !feq(r[2], 0)) return fail("view right");
    if (!feq(u[0], 0) || !feq(u[1], 1) || !feq(u[2], 0)) return fail("view up");
    efx_render_billboard_basis(&cam, EFX_FACING_Y, NULL, r, u);
    if (!feq(u[0], 0) || !feq(u[1], 1) || !feq(u[2], 0)) return fail("y up");
    if (fabsf(r[1]) > 0.001f) return fail("y right horizontal");
    float n[3] = {0, 1, 0};
    efx_render_billboard_basis(&cam, EFX_FACING_PLANE, n, r, u);
    /* plane basis must span the plane: cross(right, up) == normal */
    float cx = r[1] * u[2] - r[2] * u[1];
    float cy = r[2] * u[0] - r[0] * u[2];
    float cz = r[0] * u[1] - r[1] * u[0];
    if (!feq(cx, 0) || !feq(cy, 1) || !feq(cz, 0)) return fail("plane normal");
    return 0;
}

static int particle_config_validation(void) {
    install_mock_sink();
    uint64_t tex = make_test_texture();
    efx_particle_config c;
    int err = 0;
    base_config(&c, tex);
    uint64_t h = efx_render_particles_create(&c, &err);
    if (!h || err != EFX_RENDER_OK) return fail("valid config rejected");
    if (efx_render_particles_count(h) != 0) return fail("fresh count");
    /* a live system retains its texture (ADR 0027 mechanism) */
    if (efx_render_texture_ref_count(tex) != 1) return fail("texture not retained");
    efx_render_particles_destroy(h);
    efx_render_end_frame();
    if (efx_render_texture_ref_count(tex) != 0) return fail("texture not released");

    base_config(&c, 0);
    if (efx_render_particles_create(&c, &err) != 0 || err == EFX_RENDER_OK)
        return fail("missing texture accepted");
    base_config(&c, tex);
    c.max = 0;
    if (efx_render_particles_create(&c, &err) != 0) return fail("max 0 accepted");
    base_config(&c, tex);
    c.space = EFX_SPACE_SCREEN;
    c.facing = EFX_FACING_PLANE;
    if (efx_render_particles_create(&c, &err) != 0)
        return fail("screen+plane accepted");
    base_config(&c, tex);
    c.size_count = 9;
    if (efx_render_particles_create(&c, &err) != 0) return fail("size count");
    base_config(&c, tex);
    c.size_variation = 2.0f;
    if (efx_render_particles_create(&c, &err) != 0) return fail("size var");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int particle_emit_step(void) {
    install_mock_sink();
    uint64_t tex = make_test_texture();
    efx_particle_config c;
    base_config(&c, tex);
    int err = 0;
    uint64_t h = efx_render_particles_create(&c, &err);
    if (!h) return fail("create");
    if (efx_render_particles_emit(h, 10) != EFX_RENDER_OK) return fail("emit");
    if (efx_render_particles_count(h) != 10) return fail("emit count");

    /* deterministic: a twin system steps identically */
    efx_particle_config c2;
    base_config(&c2, tex);
    uint64_t h2 = efx_render_particles_create(&c2, &err);
    efx_render_particles_emit(h2, 10);
    efx_render_particles_step(0.1f);
    int n1 = 0, n2 = 0;
    const efx_particle_view *v1 = efx_render_particles_views(h, &n1);
    const efx_particle_view *v2 = efx_render_particles_views(h2, &n2);
    if (n1 != n2) return fail("determinism count");
    for (int i = 0; i < n1; i++) {
        if (!feq(v1[i].pos[0], v2[i].pos[0]) ||
            !feq(v1[i].pos[1], v2[i].pos[1])) {
            return fail("determinism position");
        }
    }

    /* gravity moves particles down */
    efx_render_particles_reset(h);
    base_config(&c, tex);
    c.gravity[1] = -10.0f;
    c.life_min = c.life_max = 10.0f;
    if (efx_render_particles_set(h, &c) != EFX_RENDER_OK) return fail("set");
    efx_render_particles_emit(h, 1);
    efx_render_particles_step(0.5f);
    const efx_particle_view *v = efx_render_particles_views(h, &n1);
    if (n1 != 1 || !(v[0].pos[1] < -0.5f)) return fail("gravity");

    /* lifetime retirement */
    c.life_min = c.life_max = 0.1f;
    c.gravity[1] = 0.0f;
    efx_render_particles_set(h, &c);
    efx_render_particles_reset(h);
    efx_render_particles_emit(h, 5);
    efx_render_particles_step(0.2f);
    if (efx_render_particles_count(h) != 0) return fail("lifetime retire");

    /* pause suspends aging; reset clears */
    c.life_min = c.life_max = 5.0f;
    efx_render_particles_set(h, &c);
    /* an invalid set leaves the configuration unchanged (atomic) */
    efx_particle_config bad;
    base_config(&bad, tex);
    bad.max = 0;
    if (efx_render_particles_set(h, &bad) == EFX_RENDER_OK)
        return fail("invalid set accepted");
    if (!feq(efx_render_particles_speed_scale(h), 1.0f))
        return fail("config changed after failed set");
    efx_render_particles_emit(h, 3);
    efx_render_particles_pause(h);
    efx_render_particles_step(1.0f);
    if (efx_render_particles_count(h) != 3) return fail("pause");
    efx_render_particles_start(h);
    efx_render_particles_reset(h);
    if (efx_render_particles_count(h) != 0) return fail("reset");

    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int particle_interpolation(void) {
    install_mock_sink();
    uint64_t tex = make_test_texture();
    efx_particle_config c;
    base_config(&c, tex);
    c.life_min = c.life_max = 10.0f;
    c.size_count = 2;
    c.sizes[0] = 1.0f;
    c.sizes[1] = 3.0f;
    c.color_count = 2;
    c.colors[0][0] = c.colors[0][1] = c.colors[0][2] = 1.0f;
    c.colors[0][3] = 1.0f;
    c.colors[1][0] = 1.0f;
    c.colors[1][1] = c.colors[1][2] = 0.0f;
    c.colors[1][3] = 0.0f;
    int err = 0;
    uint64_t h = efx_render_particles_create(&c, &err);
    efx_render_particles_emit(h, 1);
    int n = 0;
    const efx_particle_view *v = efx_render_particles_views(h, &n);
    if (n != 1 || !feq(v[0].size, 1.0f) || !feq(v[0].color[0], 1.0f))
        return fail("t=0 interpolation");
    efx_render_particles_step(5.0f);
    v = efx_render_particles_views(h, &n);
    if (n != 1 || !feq(v[0].size, 2.0f) || !feq(v[0].color[0], 1.0f) ||
        !feq(v[0].color[1], 0.5f)) {
        return fail("t=0.5 interpolation");
    }
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int billboard_record_fields(void) {
    install_mock_sink();
    uint64_t tex = make_test_texture();
    efx_camera3d cam = {{0, 2, 5}, {0, 0, 0}, 60, 0.1f, 100};
    efx_render_set_camera3d(&cam);
    efx_render_set_blend(EFX_BLEND_ADDITIVE);
    float pos[3] = {1, 2, 3};
    float color[4] = {0.5f, 0.25f, 0.1f, 0.8f};
    float src[4] = {0, 0, 1, 1};
    if (efx_render_billboard(tex, pos, 2.0f, 3.0f, color, 45.0f, EFX_FACING_Y,
                             NULL, 1, src, 1) != EFX_RENDER_OK) {
        return fail("billboard record");
    }
    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    if (count != 1 || recs[0].type != EFX_RECORD_BILLBOARD)
        return fail("billboard record type");
    const efx_billboard_record *b = &recs[0].u.billboard;
    if (!feq(b->pos[0], 1) || !feq(b->pos[2], 3)) return fail("billboard pos");
    if (!feq(b->w, 2) || !feq(b->h, 3)) return fail("billboard size");
    if (b->facing != EFX_FACING_Y || b->depth_test != 1 ||
        b->blend != EFX_BLEND_ADDITIVE)
        return fail("billboard flags");
    if (!feq(b->color[1], 0.25f)) return fail("billboard color");
    if (!feq(b->tw, 2) || !feq(b->sh, 1)) return fail("billboard src");
    /* invalid size rejected */
    if (efx_render_billboard(tex, pos, 0.0f, 1.0f, NULL, 0, EFX_FACING_VIEW,
                             NULL, 1, NULL, 0) == EFX_RENDER_OK)
        return fail("billboard zero size accepted");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static const efx_test_case cases[] = {
    EFX_CASE(compose_camera),
    EFX_CASE(compose_quad),
    EFX_CASE(value_snapshot),
    EFX_CASE(default_camera_viewport),
    EFX_CASE(blend_snapshot),
    EFX_CASE(record_budget),
    EFX_CASE(texture_lifecycle),
    EFX_CASE(texture_queued_handles),
    EFX_CASE(texture_mipmaps),
    EFX_CASE(record_fields),
    EFX_CASE(batching),
    EFX_CASE(meshdata_validation),
    EFX_CASE(meshdata_skinning),
    EFX_CASE(mesh_lifecycle),
    EFX_CASE(mesh_pending_upload),
    EFX_CASE(mesh_record_fields),
    EFX_CASE(mesh_record_order),
    EFX_CASE(mesh_record_budget),
    EFX_CASE(lights_state),
    EFX_CASE(light_snapshot),
    EFX_CASE(material_binding),
    EFX_CASE(lighting_reference),
    EFX_CASE(material_maps),
    EFX_CASE(map_retention),
    EFX_CASE(lighting_maps),
    EFX_CASE(render_target_lifecycle),
    EFX_CASE(target_deferred_release),
    EFX_CASE(segmentation),
    EFX_CASE(target_redirection),
    EFX_CASE(feedback_guard),
    EFX_CASE(post_registry),
    EFX_CASE(post_fast_path),
    EFX_CASE(post_render_scale),
    EFX_CASE(post_surface_size),
    EFX_CASE(post_user_target_raw),
    EFX_CASE(skin_bind_local),
    EFX_CASE(skin_sampling),
    EFX_CASE(skin_lbs),
    EFX_CASE(skin_pose_blend),
    EFX_CASE(mesh_skin_buffers),
    EFX_CASE(mesh_pose_repose),
    EFX_CASE(mesh_skinned_flag),
    EFX_CASE(mesh_skin_lifecycle),
    EFX_CASE(billboard_basis),
    EFX_CASE(particle_config_validation),
    EFX_CASE(particle_emit_step),
    EFX_CASE(particle_interpolation),
    EFX_CASE(billboard_record_fields),
};

int main(int argc, char **argv) {
    return efx_test_main(cases, sizeof(cases) / sizeof(cases[0]), argc, argv);
}
