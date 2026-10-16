/*
 * Headless unit tests for the image heightfield builder (image-heightfield):
 * the pinned geometry contract (3d-core "Image heightfield primitive") —
 * grid layout, Rec. 709 luminance heights, region clamping + step
 * decimation, full-image uv, flat-field normals exactly (0, 1, 0), the
 * byte-identical determinism receipt, and the C-level error codes. The
 * JS-side TypeError/RangeError taxonomy lives in the shared prelude.
 *
 * Usage: efx_heightfield_tests [<case>] ; exit 0 = pass.
 */
#include "render/render_heightfield.h"

#include <stdlib.h>
#include <string.h>

#include "../test_support.h"

/* exact-bit equality: flat-field normals and the determinism contract are
 * bit-identity, not tolerance */
static int beq(float a, float b) {
    return memcmp(&a, &b, sizeof(float)) == 0;
}

/* synthetic RGBA8 image, R=G=B=fn(x,y), fully opaque */
static efx_image make_img(int w, int h, uint8_t (*fn)(int, int)) {
    efx_image img;
    img.width = w;
    img.height = h;
    img.pixels = malloc((size_t)w * (size_t)h * 4u);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t v = fn ? fn(x, y) : 0;
            uint8_t *px = img.pixels + ((size_t)y * (size_t)w + (size_t)x) * 4u;
            px[0] = v;
            px[1] = v;
            px[2] = v;
            px[3] = 255;
        }
    }
    return img;
}

static void free_img(efx_image *img) {
    free(img->pixels);
    img->pixels = NULL;
}

static uint8_t ramp_x(int x, int y) {
    (void)y;
    return (uint8_t)((x * 255) / 15); /* 16-wide: 0..255 */
}

static uint8_t ramp_xy(int x, int y) {
    return (uint8_t)(((x * 17) + (y * 7)) % 256);
}

static uint8_t flat200(int x, int y) {
    (void)x;
    (void)y;
    return 200;
}

static efx_heightfield_opts full_opts(int w, int h) {
    efx_heightfield_opts o;
    memset(&o, 0, sizeof(o));
    o.region_x = 0;
    o.region_y = 0;
    o.region_w = w;
    o.region_h = h;
    o.step = 1;
    o.height = 1.0f;
    o.size = 0.0f; /* default: 1 world unit per pixel */
    return o;
}

/* the test-side reference luminance (same formula as the builder) */
static float ref_luma(uint8_t v) {
    float c = v / 255.0f;
    return 0.2126f * c + 0.7152f * c + 0.0722f * c;
}

static int default_grid_layout(void) {
    efx_image img = make_img(4, 4, ramp_xy);
    efx_heightfield_opts o = full_opts(4, 4);
    efx_meshdata *md = efx_heightfield_build(&img, &o, NULL);
    if (!md) return fail("4x4 default build failed");
    if (md->surface_count != 1) return fail("expected a single surface");
    const efx_surface *s = &md->surfaces[0];
    if (s->vertex_count != 16) return fail("expected a 4x4 vertex grid");
    if (s->index_count != 54) return fail("expected 18 triangles");
    if (!s->normals || !s->uvs || s->colors) {
        return fail("expected normals+uvs and no colors");
    }
    /* centered extents: xspan = zspan = 4 (1 unit/pixel), dx = dz = 4/3 */
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            int v = j * 4 + i;
            float x = s->positions[v * 3];
            float y = s->positions[v * 3 + 1];
            float z = s->positions[v * 3 + 2];
            if (!feq(x, -2.0f + i * (4.0f / 3.0f))) {
                return fail("x grid position mismatch");
            }
            if (!feq(z, -2.0f + j * (4.0f / 3.0f))) {
                return fail("z grid position mismatch");
            }
            uint8_t val = ramp_xy(i, j);
            if (!feq(y, ref_luma(val))) {
                return fail("y must be the pixel luminance");
            }
            if (!feq(s->uvs[v * 2], i / 3.0f) ||
                !feq(s->uvs[v * 2 + 1], j / 3.0f)) {
                return fail("uv must span 0..1 over the full image");
            }
        }
    }
    /* CCW quad pattern (a, d, e, a, e, b), row-major over cols=4 */
    if (s->indices[0] != 0 || s->indices[1] != 4 || s->indices[2] != 5 ||
        s->indices[3] != 0 || s->indices[4] != 5 || s->indices[5] != 1) {
        return fail("first quad indices must match the makePlane pattern");
    }
    efx_meshdata_destroy(md);
    free_img(&img);
    return 0;
}

static int region_step_sampling(void) {
    /* spec scenario: 16x12 image, region [4,2,10,6], step 2 -> 5x3 grid */
    efx_image img = make_img(16, 12, ramp_xy);
    efx_heightfield_opts o = full_opts(16, 12);
    o.region_x = 4;
    o.region_y = 2;
    o.region_w = 10;
    o.region_h = 6;
    o.step = 2;
    efx_meshdata *md = efx_heightfield_build(&img, &o, NULL);
    if (!md) return fail("region/step build failed");
    const efx_surface *s = &md->surfaces[0];
    if (s->vertex_count != 15 || s->index_count != 4 * 2 * 6) {
        return fail("expected a 5x3 grid (24 triangles)");
    }
    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 5; i++) {
            int v = j * 5 + i;
            int px = 4 + i * 2;
            int py = 2 + j * 2;
            if (!feq(s->positions[v * 3 + 1], ref_luma(ramp_xy(px, py)))) {
                return fail("sampled heights must come from the region");
            }
            /* uv maps the sampled pixel over the FULL image (15/11) */
            if (!feq(s->uvs[v * 2], px / 15.0f) ||
                !feq(s->uvs[v * 2 + 1], py / 11.0f)) {
                return fail("uv must be the full-image sub-rect");
            }
        }
    }
    /* centered extents: xspan = 10 (default), zspan = 10*6/10 = 6 */
    float xmin = s->positions[0], zmin = s->positions[2];
    if (!feq(xmin, -5.0f)) return fail("x must be centered on the region span");
    if (!feq(zmin, -3.0f)) return fail("z must follow the region aspect");
    efx_meshdata_destroy(md);
    free_img(&img);
    return 0;
}

static int height_size_scaling(void) {
    /* spec scenario: 64x32 image, height 5, size 20 -> y 0..5, x 20, z 10 */
    efx_image img = make_img(64, 32, ramp_x);
    efx_heightfield_opts o = full_opts(64, 32);
    o.height = 5.0f;
    o.size = 20.0f;
    efx_meshdata *md = efx_heightfield_build(&img, &o, NULL);
    if (!md) return fail("scaled build failed");
    const efx_surface *s = &md->surfaces[0];
    float ymin = 1e9f, ymax = -1e9f, xmin = 1e9f, xmax = -1e9f;
    float zmin = 1e9f, zmax = -1e9f;
    for (int v = 0; v < s->vertex_count; v++) {
        float x = s->positions[v * 3];
        float y = s->positions[v * 3 + 1];
        float z = s->positions[v * 3 + 2];
        if (y < ymin) ymin = y;
        if (y > ymax) ymax = y;
        if (x < xmin) xmin = x;
        if (x > xmax) xmax = x;
        if (z < zmin) zmin = z;
        if (z > zmax) zmax = z;
    }
    if (!feq(ymin, 0.0f) || !feq(ymax, 5.0f)) {
        return fail("height must map luminance 0..1 to 0..height");
    }
    if (!feq(xmax - xmin, 20.0f)) return fail("x extent must equal size");
    if (!feq(zmax - zmin, 10.0f)) {
        return fail("z extent must equal size * regionH/regionW");
    }
    efx_meshdata_destroy(md);
    free_img(&img);
    return 0;
}

static int flat_normals_up(void) {
    efx_image img = make_img(8, 8, flat200);
    efx_heightfield_opts o = full_opts(8, 8);
    efx_meshdata *md = efx_heightfield_build(&img, &o, NULL);
    if (!md) return fail("flat build failed");
    const efx_surface *s = &md->surfaces[0];
    for (int v = 0; v < s->vertex_count; v++) {
        if (!beq(s->normals[v * 3], 0.0f) ||
            !beq(s->normals[v * 3 + 1], 1.0f) ||
            !beq(s->normals[v * 3 + 2], 0.0f)) {
            return fail("flat field normals must be exactly (0, 1, 0)");
        }
    }
    efx_meshdata_destroy(md);
    free_img(&img);
    return 0;
}

static int slope_normals_tilt(void) {
    /* ascending x-ramp: dh/dx > 0, so nx < 0 and the vector stays unit */
    efx_image img = make_img(16, 8, ramp_x);
    efx_heightfield_opts o = full_opts(16, 8);
    efx_meshdata *md = efx_heightfield_build(&img, &o, NULL);
    if (!md) return fail("ramp build failed");
    const efx_surface *s = &md->surfaces[0];
    for (int v = 0; v < s->vertex_count; v++) {
        float nx = s->normals[v * 3];
        float ny = s->normals[v * 3 + 1];
        float nz = s->normals[v * 3 + 2];
        if (!(nx < 0.0f)) return fail("ascending slope must tilt -x");
        if (nz != 0.0f) return fail("x-only ramp must keep z normals flat");
        float len = nx * nx + ny * ny + nz * nz;
        if (!feq(len, 1.0f)) return fail("normals must be normalized");
    }
    efx_meshdata_destroy(md);
    free_img(&img);
    return 0;
}

static int deterministic_streams(void) {
    efx_image img = make_img(16, 12, ramp_xy);
    efx_heightfield_opts o = full_opts(16, 12);
    o.region_x = 3;
    o.region_y = 2;
    o.region_w = 11;
    o.region_h = 7;
    o.step = 2;
    o.height = 3.5f;
    o.size = 9.0f;
    efx_meshdata *a = efx_heightfield_build(&img, &o, NULL);
    efx_meshdata *b = efx_heightfield_build(&img, &o, NULL);
    if (!a || !b) return fail("determinism builds failed");
    const efx_surface *sa = &a->surfaces[0];
    const efx_surface *sb = &b->surfaces[0];
    size_t vb = (size_t)sa->vertex_count * 3 * sizeof(float);
    size_t ub = (size_t)sa->vertex_count * 2 * sizeof(float);
    size_t ib = (size_t)sa->index_count * sizeof(uint32_t);
    if (memcmp(sa->positions, sb->positions, vb) != 0 ||
        memcmp(sa->normals, sb->normals, vb) != 0 ||
        memcmp(sa->uvs, sb->uvs, ub) != 0 ||
        memcmp(sa->indices, sb->indices, ib) != 0) {
        return fail("same inputs must give byte-identical streams");
    }
    efx_meshdata_destroy(a);
    efx_meshdata_destroy(b);
    free_img(&img);
    return 0;
}

static int region_clamp_degenerate(void) {
    efx_image img = make_img(16, 12, ramp_xy);

    /* a region hanging past the right/bottom edges clamps to the image */
    efx_heightfield_opts o = full_opts(16, 12);
    o.region_x = 12;
    o.region_y = 2;
    o.region_w = 100;
    o.region_h = 100;
    efx_meshdata *md = efx_heightfield_build(&img, &o, NULL);
    if (!md) return fail("out-of-bounds region must clamp, not fail");
    if (md->surfaces[0].vertex_count != 4 * 10) {
        return fail("clamped region must give the clamped grid");
    }
    efx_meshdata_destroy(md);

    /* fully outside: degenerate after clamping */
    int err = 0;
    o.region_x = 20;
    o.region_y = 0;
    o.region_w = 4;
    o.region_h = 4;
    if (efx_heightfield_build(&img, &o, &err) || err != EFX_HF_ERR_RANGE) {
        return fail("empty clamped region must be EFX_HF_ERR_RANGE");
    }

    /* single column: fewer than 2 samples */
    o.region_x = 0;
    o.region_w = 1;
    err = 0;
    if (efx_heightfield_build(&img, &o, &err) || err != EFX_HF_ERR_RANGE) {
        return fail("a 1-wide region must be EFX_HF_ERR_RANGE");
    }

    /* step larger than the region: fewer than 2 samples */
    o = full_opts(16, 12);
    o.step = 16;
    err = 0;
    if (efx_heightfield_build(&img, &o, &err) || err != EFX_HF_ERR_RANGE) {
        return fail("oversized step must be EFX_HF_ERR_RANGE");
    }
    free_img(&img);
    return 0;
}

static int bad_args_range(void) {
    efx_image img = make_img(4, 4, ramp_xy);
    efx_heightfield_opts o = full_opts(4, 4);
    int err = 0;

    if (efx_heightfield_build(NULL, &o, &err) || err != EFX_HF_ERR_RANGE) {
        return fail("NULL image must be EFX_HF_ERR_RANGE");
    }
    if (efx_heightfield_build(&img, NULL, &err) || err != EFX_HF_ERR_RANGE) {
        return fail("NULL opts must be EFX_HF_ERR_RANGE");
    }
    o.step = 0;
    err = 0;
    if (efx_heightfield_build(&img, &o, &err) || err != EFX_HF_ERR_RANGE) {
        return fail("step 0 must be EFX_HF_ERR_RANGE");
    }
    o.step = 1;
    efx_image empty = make_img(0, 0, NULL);
    err = 0;
    if (efx_heightfield_build(&empty, &o, &err) || err != EFX_HF_ERR_RANGE) {
        return fail("empty image must be EFX_HF_ERR_RANGE");
    }
    free_img(&empty);
    free_img(&img);
    return 0;
}

static const efx_test_case cases[] = {
    EFX_CASE(default_grid_layout),
    EFX_CASE(region_step_sampling),
    EFX_CASE(height_size_scaling),
    EFX_CASE(flat_normals_up),
    EFX_CASE(slope_normals_tilt),
    EFX_CASE(deterministic_streams),
    EFX_CASE(region_clamp_degenerate),
    EFX_CASE(bad_args_range),
};

int main(int argc, char **argv) {
    return efx_test_main(cases, sizeof(cases) / sizeof(cases[0]), argc, argv);
}
