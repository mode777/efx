/*
 * Headless unit tests for the F6a resource provider (directory backend and
 * path safety). Usage: efx_resource_tests <case-name> ; exit 0 = pass.
 */
#include "resource/gltf.h"
#include "resource/image.h"
#include "resource/resource.h"
#include "render/render.h"
#include "render/skin.h"

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

static int dir_read(void) {
    int err = -1;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r || err != EFX_RESOURCE_OK) return fail("open fixtures dir");
    if (!efx_resource_exists(r, "hello.txt")) {
        efx_resource_close(r);
        return fail("hello.txt should exist");
    }
    size_t size = 0;
    uint8_t *buf = efx_resource_read(r, "hello.txt", &size, &err);
    if (!buf || err != EFX_RESOURCE_OK) {
        efx_resource_close(r);
        return fail("read hello.txt");
    }
    if (size != 10 || memcmp(buf, "hello efx\n", 10) != 0) {
        efx_resource_free(buf);
        efx_resource_close(r);
        return fail("hello.txt contents/size");
    }
    efx_resource_free(buf);
    efx_resource_close(r);
    return 0;
}

static int dir_nested(void) {
    int err = -1;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    size_t size = 0;
    uint8_t *buf = efx_resource_read(r, "data/nested.txt", &size, &err);
    efx_resource_close(r);
    if (!buf || err != EFX_RESOURCE_OK) return fail("read nested");
    int ok = (size == 7 && memcmp(buf, "nested\n", 7) == 0);
    efx_resource_free(buf);
    return ok ? 0 : fail("nested contents/size");
}

static int dir_missing(void) {
    int err = -1;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    uint8_t *buf = efx_resource_read(r, "nope.txt", NULL, &err);
    efx_resource_close(r);
    if (buf) return fail("missing file returned data");
    return err == EFX_RESOURCE_ERR_NOTFOUND ? 0 : fail("missing err code");
}

static int dir_traversal(void) {
    int err = -1;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    const char *bad[] = {"../hello.txt", "/etc/passwd", "a/../../b",
                         "data/../../hello.txt", "..", ""};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        err = EFX_RESOURCE_OK;
        uint8_t *buf = efx_resource_read(r, bad[i], NULL, &err);
        if (buf) {
            efx_resource_free(buf);
            efx_resource_close(r);
            return fail("escaping path was read");
        }
        if (err != EFX_RESOURCE_ERR_PATH) {
            efx_resource_close(r);
            return fail("escaping path err code");
        }
    }
    efx_resource_close(r);
    return 0;
}

static int dir_bad_root(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES "/no_such_dir", &err);
    if (r) {
        efx_resource_close(r);
        return fail("missing root opened");
    }
    return err == EFX_RESOURCE_ERR_OPEN ? 0 : fail("bad root err code");
}

/* read a fixture and decode it as an image */
static efx_image *load_image(efx_resource *r, const char *path, int *out_err) {
    size_t size = 0;
    int err = EFX_RESOURCE_OK;
    uint8_t *buf = efx_resource_read(r, path, &size, &err);
    if (!buf) {
        *out_err = err;
        return NULL;
    }
    efx_image *img = efx_image_decode(buf, size, out_err);
    efx_resource_free(buf);
    return img;
}

static int image_png(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    efx_image *img = load_image(r, "test_rgba.png", &err);
    efx_resource_close(r);
    if (!img || err != EFX_IMAGE_OK) return fail("decode png");
    static const uint8_t expect[24] = {
        255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0,
        255, 255, 0, 255, 0, 255, 255, 255, 255, 0, 255, 64};
    int ok = (img->width == 3 && img->height == 2 &&
              memcmp(img->pixels, expect, sizeof(expect)) == 0);
    efx_image_free(img);
    return ok ? 0 : fail("png pixels/size");
}

static int image_jpeg(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    efx_image *img = load_image(r, "test_opaque.jpg", &err);
    efx_resource_close(r);
    if (!img || err != EFX_IMAGE_OK) return fail("decode jpeg");
    int ok = (img->width == 4 && img->height == 2);
    if (ok) {
        for (int i = 0; i < img->width * img->height; i++) {
            if (img->pixels[i * 4 + 3] != 255) {
                ok = 0;
                break;
            }
        }
    }
    efx_image_free(img);
    return ok ? 0 : fail("jpeg size/opaque alpha");
}

static int image_corrupt(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES, &err);
    if (!r) return fail("open fixtures dir");
    efx_image *img = load_image(r, "corrupt.png", &err);
    efx_resource_close(r);
    if (img) {
        efx_image_free(img);
        return fail("corrupt image decoded");
    }
    return err == EFX_IMAGE_ERR_DECODE ? 0 : fail("corrupt err code");
}

static efx_resource *open_pack(int *err) {
    return efx_resource_open(EFX_RES_FIXTURES "/pack.zip", err);
}

static int zip_read(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = open_pack(&err);
    if (!r) return fail("open pack.zip");
    if (!efx_resource_is_zip(r)) {
        efx_resource_close(r);
        return fail("pack.zip not detected as zip");
    }
    size_t size = 0;
    uint8_t *buf = efx_resource_read(r, "hello.txt", &size, &err);
    if (!buf || err != EFX_RESOURCE_OK) {
        efx_resource_close(r);
        return fail("zip read hello.txt");
    }
    int ok = (size == 10 && memcmp(buf, "hello efx\n", 10) == 0);
    efx_resource_free(buf);
    efx_resource_close(r);
    return ok ? 0 : fail("zip hello contents/size");
}

static int zip_nested(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = open_pack(&err);
    if (!r) return fail("open pack.zip");
    size_t size = 0;
    uint8_t *buf = efx_resource_read(r, "data/nested.txt", &size, &err);
    efx_resource_close(r);
    if (!buf) return fail("zip read nested");
    int ok = (size == 7 && memcmp(buf, "nested\n", 7) == 0);
    efx_resource_free(buf);
    return ok ? 0 : fail("zip nested contents/size");
}

static int zip_image(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = open_pack(&err);
    if (!r) return fail("open pack.zip");
    efx_image *img = load_image(r, "img/test_rgba.png", &err);
    efx_resource_close(r);
    if (!img || err != EFX_IMAGE_OK) return fail("zip decode image");
    int ok = (img->width == 3 && img->height == 2);
    efx_image_free(img);
    return ok ? 0 : fail("zip image dims");
}

static int zip_missing(void) {
    int err = EFX_RESOURCE_OK;
    efx_resource *r = open_pack(&err);
    if (!r) return fail("open pack.zip");
    uint8_t *buf = efx_resource_read(r, "nope.txt", NULL, &err);
    efx_resource_close(r);
    if (buf) return fail("zip missing returned data");
    return err == EFX_RESOURCE_ERR_NOTFOUND ? 0 : fail("zip missing err code");
}

static int zip_bad_root(void) {
    int err = EFX_RESOURCE_OK;
    /* corrupt.png is not a zip archive */
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES "/corrupt.png", &err);
    if (r) {
        efx_resource_close(r);
        return fail("non-zip file opened as root");
    }
    return err == EFX_RESOURCE_ERR_OPEN ? 0 : fail("bad zip err code");
}

/* ---------------------------------------------------------- F6b glTF import */

static void *gltf_mock_create(void *ud, int w, int h, const uint8_t *rgba,
                              int wrap, int filter, int mipmaps) {
    (void)ud; (void)w; (void)h; (void)rgba; (void)wrap; (void)filter;
    (void)mipmaps;
    return malloc(8);
}

static void gltf_mock_destroy(void *ud, void *native) {
    (void)ud;
    free(native);
}

static const efx_render_sink gltf_sink = {
    NULL, gltf_mock_create, gltf_mock_destroy, NULL, NULL, NULL, NULL, NULL,
};

static efx_resource *gltf_root(int *err) {
    return efx_resource_open(EFX_RES_FIXTURES "/gltf", err);
}

static efx_meshdata *gltf_import(efx_resource *r, const char *file,
                                 const efx_gltf_mesh_opts *opts, int *err) {
    efx_render_install_sink(&gltf_sink);
    return efx_gltf_load_meshdata(r, file, opts, err);
}

static int feq(float a, float b) {
    return (a - b) < 0.001f && (b - a) < 0.001f;
}

static int gltf_triangle(void) {
    int err = 0;
    efx_resource *r = gltf_root(&err);
    if (!r) return fail("open gltf dir");
    efx_meshdata *md = gltf_import(r, "triangle.gltf", NULL, &err);
    efx_resource_close(r);
    if (!md || err != EFX_GLTF_OK) return fail("import triangle.gltf");
    int ok = md->surface_count == 1 &&
             md->surfaces[0].vertex_count == 3 &&
             md->surfaces[0].index_count == 3 &&
             md->surfaces[0].has_material == 0 &&
             feq(md->surfaces[0].positions[0], -1) &&
             feq(md->surfaces[0].positions[3], 1) &&
             feq(md->surfaces[0].positions[7], 1) &&
             md->surfaces[0].indices[2] == 2;
    efx_meshdata_destroy(md);
    efx_render_shutdown();
    return ok ? 0 : fail("triangle geometry/surface");
}

static int gltf_zip(void) {
    int err = 0;
    efx_resource *r = efx_resource_open(EFX_RES_FIXTURES "/gltf_pack.zip",
                                        &err);
    if (!r) return fail("open gltf_pack.zip");
    efx_meshdata *md = gltf_import(r, "triangle.gltf", NULL, &err);
    if (!md || err != EFX_GLTF_OK) {
        efx_resource_close(r);
        return fail("import triangle.gltf from zip");
    }
    int ok = md->surface_count == 1 && md->surfaces[0].vertex_count == 3;
    efx_meshdata_destroy(md);
    /* external image inside the zip */
    efx_meshdata *md2 = gltf_import(r, "textured.gltf", NULL, &err);
    efx_resource_close(r);
    if (!md2 || err != EFX_GLTF_OK) return fail("import textured.gltf from zip");
    ok = ok && md2->surfaces[0].has_material &&
         md2->surfaces[0].material.diffuse_map != 0;
    efx_meshdata_destroy(md2);
    efx_render_shutdown();
    return ok ? 0 : fail("zip import results");
}

static int gltf_transform(void) {
    int err = 0;
    efx_resource *r = gltf_root(&err);
    if (!r) return fail("open gltf dir");
    efx_meshdata *md = gltf_import(r, "transform.gltf", NULL, &err);
    efx_resource_close(r);
    if (!md || err != EFX_GLTF_OK) return fail("import transform.gltf");
    /* the node translation/scale is not baked in */
    int ok = feq(md->surfaces[0].positions[0], -1) &&
             feq(md->surfaces[0].positions[3], 1) &&
             feq(md->surfaces[0].positions[7], 1);
    efx_meshdata_destroy(md);
    efx_render_shutdown();
    return ok ? 0 : fail("node transform applied");
}

static int gltf_materials(void) {
    int err = 0;
    efx_resource *r = gltf_root(&err);
    if (!r) return fail("open gltf dir");
    efx_meshdata *md = gltf_import(r, "materials.gltf", NULL, &err);
    efx_resource_close(r);
    if (!md || err != EFX_GLTF_OK) return fail("import materials.gltf");
    const efx_surface *m = &md->surfaces[0];
    int ok = md->surface_count == 2 && m->has_material &&
             feq(m->material.diffuse[0], 0.2f) &&
             feq(m->material.diffuse[1], 0.4f) &&
             feq(m->material.diffuse[2], 0.6f) &&
             feq(m->material.diffuse[3], 1.0f) &&
             feq(m->material.specular[0], 0.1f) &&
             feq(m->material.specular[1], 0.2f) &&
             feq(m->material.specular[2], 0.3f) &&
             feq(m->material.shininess, 32.0f) &&
             feq(m->material.emissive[0], 0.1f) &&
             feq(m->material.emissive[1], 0.2f) &&
             feq(m->material.emissive[2], 0.3f) &&
             !md->surfaces[1].has_material;
    efx_meshdata_destroy(md);
    efx_render_shutdown();
    return ok ? 0 : fail("PBR->Phong conversion");
}

static int gltf_accessors(void) {
    int err = 0;
    efx_resource *r = gltf_root(&err);
    if (!r) return fail("open gltf dir");
    efx_meshdata *md = gltf_import(r, "accessors.gltf", NULL, &err);
    efx_resource_close(r);
    if (!md || err != EFX_GLTF_OK) return fail("import accessors.gltf");
    int ok = md->surface_count == 4;
    const float *s0 = md->surfaces[0].positions;
    const float *s1 = md->surfaces[1].positions;
    const float *s2 = md->surfaces[2].positions;
    const float *s3 = md->surfaces[3].positions;
    ok = ok && feq(s0[0], -1) && feq(s0[3], 1) && md->surfaces[0].index_count == 3;
    ok = ok && feq(s1[3], 100) && feq(s1[7], 200);
    ok = ok && feq(s2[3], 1.0f) && feq(s2[7], 128.0f / 255.0f);
    ok = ok && feq(s3[3], 5) && feq(s3[7], 6);
    efx_meshdata_destroy(md);
    efx_render_shutdown();
    return ok ? 0 : fail("accessor normalization/sparse");
}

static int gltf_glb(void) {
    int err = 0;
    efx_resource *r = gltf_root(&err);
    if (!r) return fail("open gltf dir");
    efx_meshdata *md = gltf_import(r, "quad.glb", NULL, &err);
    efx_resource_close(r);
    if (!md || err != EFX_GLTF_OK) return fail("import quad.glb");
    const efx_surface *s0 = &md->surfaces[0];
    const efx_surface *s1 = &md->surfaces[1];
    int wrap = -1, filter = -1;
    efx_render_texture_sampler(s0->material.diffuse_map, &wrap, &filter, NULL);
    int ok = md->surface_count == 2 && s0->has_material && s1->has_material &&
             s0->material.diffuse_map != 0 && s0->material.alpha_mask != 0 &&
             s0->material.alpha_mask == s0->material.diffuse_map &&
             s1->material.diffuse_map == 0 && s1->material.emissive_map == 0 &&
             feq(s1->material.emissive[1], 1.0f) &&
             wrap == EFX_TEX_WRAP_CLAMP && filter == EFX_FILTER_NEAREST;
    efx_meshdata_destroy(md);
    efx_render_shutdown();
    return ok ? 0 : fail("glb import / material / sampler");
}

static int gltf_dedup(void) {
    int err = 0;
    efx_resource *r = gltf_root(&err);
    if (!r) return fail("open gltf dir");
    efx_meshdata *md = gltf_import(r, "dedup.gltf", NULL, &err);
    efx_resource_close(r);
    if (!md || err != EFX_GLTF_OK) return fail("import dedup.gltf");
    uint64_t h0 = md->surfaces[0].material.diffuse_map;
    uint64_t h1 = md->surfaces[1].material.diffuse_map;
    uint64_t h2 = md->surfaces[2].material.diffuse_map;
    int wrap0 = -1, wrap2 = -1;
    efx_render_texture_sampler(h0, &wrap0, NULL, NULL);
    efx_render_texture_sampler(h2, &wrap2, NULL, NULL);
    int ok = h0 != 0 && h0 == h1 && h2 != h0 &&
             efx_render_texture_ref_count(h0) == 2 &&
             wrap0 == EFX_TEX_WRAP_REPEAT && wrap2 == EFX_TEX_WRAP_REPEAT;
    /* h0 uses the linear sampler, h2 the nearest one */
    int f0 = -1, f2 = -1;
    efx_render_texture_sampler(h0, NULL, &f0, NULL);
    efx_render_texture_sampler(h2, NULL, &f2, NULL);
    ok = ok && f0 == EFX_FILTER_LINEAR && f2 == EFX_FILTER_NEAREST;
    efx_meshdata_destroy(md);
    efx_render_shutdown();
    return ok ? 0 : fail("texture dedup / sampler mapping");
}

static int gltf_errors(void) {
    int err = 0;
    efx_resource *r = gltf_root(&err);
    if (!r) return fail("open gltf dir");
    efx_meshdata *md;
    int ok = 1;
    md = gltf_import(r, "corrupt.gltf", NULL, &err);
    ok = ok && !md && err == EFX_GLTF_ERR_PARSE;
    efx_meshdata_destroy(md);
    md = gltf_import(r, "required_ext.gltf", NULL, &err);
    ok = ok && !md && err == EFX_GLTF_ERR_UNSUPPORTED;
    efx_meshdata_destroy(md);
    md = gltf_import(r, "bad_image.gltf", NULL, &err);
    ok = ok && !md && err == EFX_GLTF_ERR_IMAGE;
    efx_meshdata_destroy(md);
    md = gltf_import(r, "cap.gltf", NULL, &err);
    ok = ok && !md && err == EFX_GLTF_ERR_CAP;
    efx_meshdata_destroy(md);
    md = gltf_import(r, "nope.gltf", NULL, &err);
    ok = ok && !md && err == EFX_GLTF_ERR_IO;
    efx_meshdata_destroy(md);
    efx_gltf_mesh_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.has_mesh = 1;
    opts.is_name = 1;
    opts.mesh_name = "NoSuchMesh";
    md = gltf_import(r, "triangle.gltf", &opts, &err);
    ok = ok && !md && err == EFX_GLTF_ERR_SELECTION;
    efx_meshdata_destroy(md);
    memset(&opts, 0, sizeof(opts));
    opts.has_mesh = 1;
    opts.mesh_index = 5;
    md = gltf_import(r, "triangle.gltf", &opts, &err);
    ok = ok && !md && err == EFX_GLTF_ERR_SELECTION;
    efx_meshdata_destroy(md);
    /* by-name success and the default selection */
    memset(&opts, 0, sizeof(opts));
    opts.has_mesh = 1;
    opts.is_name = 1;
    opts.mesh_name = "m";
    md = gltf_import(r, "triangle.gltf", &opts, &err);
    ok = ok && md && md->surface_count == 1;
    efx_meshdata_destroy(md);
    efx_resource_close(r);
    efx_render_shutdown();
    return ok ? 0 : fail("glTF error taxonomy");
}

static int gltf_skin(void) {
    int err = 0;
    efx_resource *r = gltf_root(&err);
    if (!r) return fail("open gltf dir");
    efx_meshdata *md = gltf_import(r, "skin.gltf", NULL, &err);
    efx_resource_close(r);
    if (!md || err != EFX_GLTF_OK) return fail("import skin.gltf");
    int ok = md->surface_count == 1;
    const efx_surface *s = &md->surfaces[0];
    ok = ok && s->vertex_count == 4 && s->joints && s->weights;
    ok = ok && s->joints[0] == 0 && s->joints[4] == 1 &&
         feq(s->weights[0], 1.0f) && feq(s->weights[1], 0.0f);
    efx_rig *rig = md->rig;
    ok = ok && rig && rig->joint_count == 2;
    ok = ok && rig->joint_nodes[0] == 1 && rig->joint_nodes[1] == 2;
    /* Mid is a joint-hierarchy root (its parent, Root, is not a joint) */
    ok = ok && rig->joint_parents[0] == -1 && rig->joint_parents[1] == 0;
    /* Tip's inverse bind matrix translation is (0,-2,0) column-major */
    ok = ok && feq(rig->inverse_bind[16 + 13], -2.0f) &&
         feq(rig->inverse_bind[16 + 15], 1.0f);
    /* clips: "move" LINEAR, "turn" STEP, unnamed CUBICSPLINE -> "clip2" */
    ok = ok && rig->clip_count == 3;
    const efx_animation_clip *move = &rig->clips[0];
    ok = ok && move->name && strcmp(move->name, "move") == 0 &&
         move->channel_count == 1;
    const efx_anim_channel *mch = &move->channels[0];
    ok = ok && mch->path == EFX_ANIM_PATH_TRANSLATION &&
         mch->interpolation == EFX_ANIM_INTERP_LINEAR && mch->components == 3 &&
         mch->times_len == 3 && mch->values_len == 9;
    ok = ok && feq(mch->times[1], 0.5f) && feq(mch->values[3], 0.0f) &&
         feq(mch->values[4], 2.0f);
    const efx_animation_clip *turn = &rig->clips[1];
    ok = ok && turn->name && strcmp(turn->name, "turn") == 0 &&
         turn->channel_count == 1;
    const efx_anim_channel *tch = &turn->channels[0];
    ok = ok && tch->path == EFX_ANIM_PATH_ROTATION &&
         tch->interpolation == EFX_ANIM_INTERP_STEP && tch->components == 4 &&
         tch->values_len == 12;
    const efx_animation_clip *cub = &rig->clips[2];
    ok = ok && cub->name && strcmp(cub->name, "clip2") == 0 &&
         cub->channel_count == 1;
    const efx_anim_channel *cch = &cub->channels[0];
    /* CUBICSPLINE -> LINEAR: middle value per keyframe, tangents dropped */
    ok = ok && cch->interpolation == EFX_ANIM_INTERP_LINEAR &&
         cch->times_len == 2 && cch->values_len == 6;
    ok = ok && feq(cch->values[0], 0.0f) && feq(cch->values[1], 1.0f) &&
         feq(cch->values[3], 0.0f) && feq(cch->values[4], 2.0f);

    /* payload carry: create the Mesh, destroy the MeshData, rig survives */
    uint64_t mesh = efx_render_mesh_create(md);
    efx_meshdata_destroy(md);
    const efx_rig *mrig = efx_render_mesh_rig(mesh);
    ok = ok && mrig && mrig->joint_count == 2 && mrig->clip_count == 3 &&
         feq(mrig->inverse_bind[16 + 13], -2.0f);
    efx_render_mesh_destroy(mesh);
    efx_render_end_frame();
    efx_render_shutdown();
    return ok ? 0 : fail("skin import / rig payload");
}

static int gltf_skin_u8(void) {
    int err = 0;
    efx_resource *r = gltf_root(&err);
    if (!r) return fail("open gltf dir");
    efx_meshdata *md = gltf_import(r, "skin_u8.gltf", NULL, &err);
    efx_resource_close(r);
    if (!md || err != EFX_GLTF_OK) return fail("import skin_u8.gltf");
    const efx_surface *s = &md->surfaces[0];
    int ok = s->joints && s->joints[4] == 1 && s->joints[12] == 1 &&
             feq(s->weights[0], 1.0f);
    efx_meshdata_destroy(md);
    efx_render_shutdown();
    return ok ? 0 : fail("u8 joints normalization");
}

static int gltf_skin_errors(void) {
    int err = 0;
    efx_resource *r = gltf_root(&err);
    if (!r) return fail("open gltf dir");
    int ok = 1;
    efx_meshdata *md = gltf_import(r, "skin_unpaired.gltf", NULL, &err);
    ok = ok && !md && err == EFX_GLTF_ERR_PARSE;
    efx_meshdata_destroy(md);
    md = gltf_import(r, "skin_mismatch.gltf", NULL, &err);
    ok = ok && !md && err == EFX_GLTF_ERR_PARSE;
    efx_meshdata_destroy(md);
    /* a static asset carries no rig payload */
    md = gltf_import(r, "triangle.gltf", NULL, &err);
    ok = ok && md && md->rig == NULL;
    efx_meshdata_destroy(md);
    efx_resource_close(r);
    efx_render_shutdown();
    return ok ? 0 : fail("skin error taxonomy / static rig");
}

/* F7 design D7: independent CPU reference over the skin.gltf fixture — the
 * expected posed vertices are hand-derived from the fixture's bind transforms
 * and clips, then compared against efx_skin_evaluate + efx_skin_surface. */
static int skin_pose_reference(void) {
    int err = 0;
    efx_resource *r = gltf_root(&err);
    if (!r) return fail("open gltf dir");
    efx_meshdata *md = gltf_import(r, "skin.gltf", NULL, &err);
    efx_resource_close(r);
    if (!md || err != EFX_GLTF_OK) return fail("import skin.gltf");
    const efx_surface *s = &md->surfaces[0];
    const efx_rig *rig = md->rig;
    if (!rig || !s->joints || !s->weights || s->vertex_count != 4)
        return fail("fixture rig");

    /* interleaved bind layout matches the renderer (pos3 normal3 uv2 color4) */
    float bind[4 * 12];
    for (int v = 0; v < s->vertex_count; v++) {
        float *d = bind + v * 12;
        for (int k = 0; k < 3; k++) {
            d[k] = s->positions[v * 3 + k];
            d[3 + k] = s->normals[v * 3 + k];
        }
        d[6] = d[7] = 0.0f;
        d[8] = d[9] = d[10] = d[11] = 1.0f;
    }
    float out[4 * 12];
    float palette[2 * 16];

    /* "move" at t=0.25: Mid translates to y=1.5, so every vertex shifts +0.5 */
    efx_pose_sample move = {0, 0.25f, 1.0f};
    if (efx_skin_evaluate(rig, &move, 1, palette) != 0)
        return fail("evaluate move");
    efx_skin_surface(bind, s->vertex_count, s->joints, s->weights, palette,
                     rig->joint_count, out);
    if (!feq(out[0], -1.0f) || !feq(out[1], -0.5f) || !feq(out[2], 0.0f))
        return fail("move vertex 0");
    if (!feq(out[36], 1.0f) || !feq(out[37], 1.5f) || !feq(out[38], 0.0f))
        return fail("move vertex 3");
    if (!feq(out[3], 0.0f) || !feq(out[4], 0.0f) || !feq(out[5], 1.0f))
        return fail("move normal");

    /* "turn" at t=0.75: Tip rotates 90 deg about z; its bound vertex moves
       from (1,1,0) to (1,3,0) while Mid-bound vertices stay put */
    efx_pose_sample turn = {1, 0.75f, 1.0f};
    if (efx_skin_evaluate(rig, &turn, 1, palette) != 0)
        return fail("evaluate turn");
    efx_skin_surface(bind, s->vertex_count, s->joints, s->weights, palette,
                     rig->joint_count, out);
    if (!feq(out[0], -1.0f) || !feq(out[1], -1.0f)) return fail("turn v0");
    if (!feq(out[36], 1.0f) || !feq(out[37], 3.0f) || !feq(out[38], 0.0f))
        return fail("turn v3");
    if (!feq(out[39], 0.0f) || !feq(out[40], 0.0f) || !feq(out[41], 1.0f))
        return fail("turn normal");

    efx_meshdata_destroy(md);
    efx_render_shutdown();
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: efx_resource_tests <case>\n");
        return 2;
    }
    const char *c = argv[1];
    if (!strcmp(c, "dir_read")) return dir_read();
    if (!strcmp(c, "dir_nested")) return dir_nested();
    if (!strcmp(c, "dir_missing")) return dir_missing();
    if (!strcmp(c, "dir_traversal")) return dir_traversal();
    if (!strcmp(c, "dir_bad_root")) return dir_bad_root();
    if (!strcmp(c, "image_png")) return image_png();
    if (!strcmp(c, "image_jpeg")) return image_jpeg();
    if (!strcmp(c, "image_corrupt")) return image_corrupt();
    if (!strcmp(c, "zip_read")) return zip_read();
    if (!strcmp(c, "zip_nested")) return zip_nested();
    if (!strcmp(c, "zip_image")) return zip_image();
    if (!strcmp(c, "zip_missing")) return zip_missing();
    if (!strcmp(c, "zip_bad_root")) return zip_bad_root();
    if (!strcmp(c, "gltf_triangle")) return gltf_triangle();
    if (!strcmp(c, "gltf_zip")) return gltf_zip();
    if (!strcmp(c, "gltf_transform")) return gltf_transform();
    if (!strcmp(c, "gltf_materials")) return gltf_materials();
    if (!strcmp(c, "gltf_accessors")) return gltf_accessors();
    if (!strcmp(c, "gltf_glb")) return gltf_glb();
    if (!strcmp(c, "gltf_dedup")) return gltf_dedup();
    if (!strcmp(c, "gltf_errors")) return gltf_errors();
    if (!strcmp(c, "gltf_skin")) return gltf_skin();
    if (!strcmp(c, "gltf_skin_u8")) return gltf_skin_u8();
    if (!strcmp(c, "gltf_skin_errors")) return gltf_skin_errors();
    if (!strcmp(c, "skin_pose_reference")) return skin_pose_reference();
    fprintf(stderr, "unknown case: %s\n", c);
    return 2;
}
