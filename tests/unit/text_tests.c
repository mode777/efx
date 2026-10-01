/*
 * Headless font/text unit tests (F8a). Usage: efx_text_tests [<case>].
 * Desktop-only: they read a TTF from the resource fixture directory.
 */
#include "render/render.h"
#include "render/text.h"
#include "resource/resource.h"

#include <stdlib.h>

#include "../test_support.h"

#ifndef EFX_FONT_FIXTURE_DIR
#define EFX_FONT_FIXTURE_DIR "."
#endif

static efx_resource *open_root(void) {
    int err = 0;
    efx_resource *r = efx_resource_open(EFX_FONT_FIXTURE_DIR, &err);
    if (!r) fprintf(stderr, "fixture root open failed: %d\n", err);
    return r;
}

static efx_text_fontdata *load_fd(efx_resource *r) {
    int err = 0;
    efx_text_fontdata *fd = efx_text_fontdata_load(r, "font.ttf", &err);
    if (!fd) fprintf(stderr, "font load failed: %d\n", err);
    return fd;
}

static int fontdata_load(void) {
    efx_resource *r = open_root();
    if (!r) return fail("root");
    efx_text_fontdata *fd = load_fd(r);
    if (!fd) return fail("load");
    if (!efx_text_fontdata_alive(fd)) return fail("alive");
    efx_text_fontdata_destroy(fd);
    efx_resource_close(r);
    return 0;
}

static int font_bake(void) {
    efx_resource *r = open_root();
    if (!r) return fail("root");
    efx_text_fontdata *fd = load_fd(r);
    if (!fd) return fail("load");
    efx_font_opts o;
    memset(&o, 0, sizeof(o));
    o.size = 32.0f;
    o.padding = 1;
    o.filter = EFX_FILTER_LINEAR;
    int err = 0;
    efx_text_font *f = efx_text_font_create(fd, &o, &err);
    if (!f) return fail("bake");
    if (!(efx_text_font_size(f) == 32.0f)) return fail("size");
    if (!(efx_text_font_line_height(f) > 0.0f)) return fail("line height");
    if (!(efx_text_font_ascent(f) > 0.0f)) return fail("ascent");
    if (!(efx_text_font_descent(f) < 0.0f)) return fail("descent");
    if (!efx_text_font_texture(f)) return fail("texture");
    efx_text_font_destroy(f);
    efx_text_fontdata_destroy(fd);
    efx_resource_close(r);
    return 0;
}

static int font_bake_errors(void) {
    efx_resource *r = open_root();
    if (!r) return fail("root");
    efx_text_fontdata *fd = load_fd(r);
    if (!fd) return fail("load");
    efx_font_opts o;
    memset(&o, 0, sizeof(o));
    int err = 0;
    o.size = 0.0f;
    if (efx_text_font_create(fd, &o, &err) != NULL || err != EFX_TEXT_ERR_RANGE)
        return fail("size 0");
    o.size = 16.0f;
    o.padding = -1;
    if (efx_text_font_create(fd, &o, &err) != NULL || err != EFX_TEXT_ERR_RANGE)
        return fail("negative padding");
    o.padding = 0;
    o.effects.has_outline = 1;
    o.effects.outline_width = 0.0f;
    if (efx_text_font_create(fd, &o, &err) != NULL || err != EFX_TEXT_ERR_RANGE)
        return fail("outline width 0");
    efx_text_fontdata_destroy(fd);
    efx_resource_close(r);
    return 0;
}

static int layout_cases(void) {
    efx_resource *r = open_root();
    if (!r) return fail("root");
    efx_text_fontdata *fd = load_fd(r);
    if (!fd) return fail("load");
    efx_font_opts o;
    memset(&o, 0, sizeof(o));
    o.size = 24.0f;
    o.filter = EFX_FILTER_LINEAR;
    efx_text_font *f = efx_text_font_create(fd, &o, NULL);
    if (!f) return fail("bake");
    efx_text_layout_opts lo;
    efx_text_bounds b;
    memset(&lo, 0, sizeof(lo));
    lo.align = EFX_TEXT_ALIGN_LEFT;
    lo.valign = EFX_TEXT_VALIGN_TOP;
    lo.scale = 1.0f;

    if (efx_text_measure(f, "a\nb", &lo, &b) != EFX_TEXT_OK) return fail("nl");
    if (b.lines != 2) return fail("nl lines");
    if (!(b.width > 0.0f)) return fail("nl width");

    if (efx_text_measure(f, "one two three four", &lo, &b) != EFX_TEXT_OK)
        return fail("nowrap");
    if (b.lines != 1) return fail("nowrap lines");

    lo.has_width = 1;
    lo.width = 40.0f;
    if (efx_text_measure(f, "one two three four", &lo, &b) != EFX_TEXT_OK)
        return fail("wrap");
    if (b.lines < 2) return fail("wrap lines");
    if (!(b.width <= 40.5f)) return fail("wrap width");

    lo.align = EFX_TEXT_ALIGN_JUSTIFY;
    lo.has_width = 0;
    if (efx_text_measure(f, "x", &lo, &b) != EFX_TEXT_ERR_RANGE)
        return fail("justify without width");

    efx_text_font_destroy(f);
    efx_text_fontdata_destroy(fd);
    efx_resource_close(r);
    return 0;
}

static int measure_draw_agree(void) {
    efx_resource *r = open_root();
    if (!r) return fail("root");
    efx_text_fontdata *fd = load_fd(r);
    if (!fd) return fail("load");
    efx_font_opts o;
    memset(&o, 0, sizeof(o));
    o.size = 20.0f;
    o.filter = EFX_FILTER_LINEAR;
    efx_text_font *f = efx_text_font_create(fd, &o, NULL);
    if (!f) return fail("bake");
    efx_text_layout_opts lo;
    memset(&lo, 0, sizeof(lo));
    lo.align = EFX_TEXT_ALIGN_CENTER;
    lo.valign = EFX_TEXT_VALIGN_TOP;
    lo.scale = 1.0f;
    lo.has_width = 1;
    lo.width = 80.0f;
    efx_text_bounds mb, db;
    if (efx_text_measure(f, "hello world again", &lo, &mb) != EFX_TEXT_OK)
        return fail("measure");
    efx_render_begin_frame();
    if (efx_text_draw(f, "hello world again", 10, 10, &lo, NULL, NULL, NULL,
                      &db) != EFX_TEXT_OK)
        return fail("draw");
    if (mb.lines != db.lines || fabsf(mb.width - db.width) > 0.001f ||
        fabsf(mb.height - db.height) > 0.001f)
        return fail("bounds disagree");
    efx_render_end_frame();
    efx_text_font_destroy(f);
    efx_text_fontdata_destroy(fd);
    efx_resource_close(r);
    return 0;
}

static int draw_records(void) {
    efx_resource *r = open_root();
    if (!r) return fail("root");
    efx_text_fontdata *fd = load_fd(r);
    if (!fd) return fail("load");
    efx_font_opts o;
    memset(&o, 0, sizeof(o));
    o.size = 24.0f;
    o.filter = EFX_FILTER_LINEAR;
    efx_text_font *plain = efx_text_font_create(fd, &o, NULL);
    if (!plain) return fail("bake plain");
    efx_text_layout_opts lo;
    memset(&lo, 0, sizeof(lo));
    lo.align = EFX_TEXT_ALIGN_LEFT;
    lo.valign = EFX_TEXT_VALIGN_TOP;
    lo.scale = 1.0f;

    efx_render_begin_frame();
    if (efx_text_draw(plain, "AB", 0, 0, &lo, NULL, NULL, NULL, NULL) !=
        EFX_TEXT_OK)
        return fail("draw plain");
    int count = 0;
    const efx_record *recs = efx_render_records(&count);
    int fills = 0;
    for (int i = 0; i < count; i++)
        if (recs[i].type == EFX_RECORD_QUAD) fills++;
    if (fills != 2) return fail("plain record count");
    efx_render_end_frame();

    efx_font_opts oe;
    memset(&oe, 0, sizeof(oe));
    oe.size = 24.0f;
    oe.filter = EFX_FILTER_LINEAR;
    oe.effects.has_outline = 1;
    oe.effects.outline_width = 2.0f;
    oe.effects.has_shadow = 1;
    oe.effects.shadow_blur = 2.0f;
    oe.effects.shadow_offset[0] = 2.0f;
    oe.effects.shadow_offset[1] = 2.0f;
    efx_text_font *fx = efx_text_font_create(fd, &oe, NULL);
    if (!fx) return fail("bake effects");
    efx_render_begin_frame();
    if (efx_text_draw(fx, "AB", 0, 0, &lo, NULL, NULL, NULL, NULL) !=
        EFX_TEXT_OK)
        return fail("draw effects");
    recs = efx_render_records(&count);
    int quads = 0;
    for (int i = 0; i < count; i++)
        if (recs[i].type == EFX_RECORD_QUAD) quads++;
    if (quads != 6) return fail("effects record count (want 3 layers x 2)");
    efx_render_end_frame();

    efx_text_font_destroy(fx);
    efx_text_font_destroy(plain);
    efx_text_fontdata_destroy(fd);
    efx_resource_close(r);
    return 0;
}

static const efx_test_case cases[] = {
    EFX_CASE(fontdata_load),
    EFX_CASE(font_bake),
    EFX_CASE(font_bake_errors),
    EFX_CASE(layout_cases),
    EFX_CASE(measure_draw_agree),
    EFX_CASE(draw_records),
};

int main(int argc, char **argv) {
    return efx_test_main(cases, sizeof(cases) / sizeof(cases[0]), argc, argv);
}
