#include "bridge_internal.h"


/* --------------------------------------------------- F8a font + text */

typedef struct {
    efx_text_fontdata *fd;
    int alive;
} fd_slot;

typedef struct {
    efx_text_font *font;
    int alive;
} font_slot;

static struct {
    fd_slot *slots;
    int count;
    int cap;
} FDS;

static struct {
    font_slot *slots;
    int count;
    int cap;
} FONTS;

static fd_slot *fd_get(int id) {
    if (id <= 0 || id > FDS.count) return NULL;
    return &FDS.slots[id - 1];
}

static font_slot *font_get(int id) {
    if (id <= 0 || id > FONTS.count) return NULL;
    return &FONTS.slots[id - 1];
}

static int fd_alloc(void) {
    if (FDS.count >= FDS.cap) {
        int cap = FDS.cap ? FDS.cap * 2 : 16;
        fd_slot *grown = realloc(FDS.slots, (size_t)cap * sizeof(fd_slot));
        if (!grown) return 0;
        FDS.slots = grown;
        FDS.cap = cap;
    }
    return ++FDS.count;
}

static int font_alloc(void) {
    if (FONTS.count >= FONTS.cap) {
        int cap = FONTS.cap ? FONTS.cap * 2 : 16;
        font_slot *grown = realloc(FONTS.slots, (size_t)cap * sizeof(font_slot));
        if (!grown) return 0;
        FONTS.slots = grown;
        FONTS.cap = cap;
    }
    return ++FONTS.count;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_load_fontdata(const char *path) {
    if (!W.resource || !path) return 0;
    int e = EFX_TEXT_OK;
    efx_text_fontdata *fd = efx_text_fontdata_load(W.resource, path, &e);
    if (!fd) return 0;
    int id = fd_alloc();
    if (!id) {
        efx_text_fontdata_destroy(fd);
        return 0;
    }
    FDS.slots[id - 1].fd = fd;
    FDS.slots[id - 1].alive = 1;
    return id;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_fontdata_destroy(int id) {
    fd_slot *s = fd_get(id);
    if (s && s->alive) {
        s->alive = 0;
        efx_text_fontdata_destroy(s->fd);
        s->fd = NULL;
    }
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_create_font(
    int fd_id, float size, const char *glyphs, int padding, int filter,
    int has_outline, float outline_width, int has_shadow, float shadow_blur,
    float off_x, float off_y) {
    fd_slot *s = fd_get(fd_id);
    if (!s || !s->alive) return 0;
    efx_font_opts o;
    memset(&o, 0, sizeof(o));
    o.size = size;
    o.padding = padding;
    o.filter = filter;
    uint32_t *cps = NULL;
    int ncp = 0;
    if (glyphs && glyphs[0]) {
        ncp = efx_text_codepoints(glyphs, &cps);
        if (ncp <= 0) {
            free(cps);
            return 0;
        }
        o.codepoints = cps;
        o.codepoint_count = ncp;
    }
    o.effects.has_outline = has_outline;
    o.effects.outline_width = outline_width;
    o.effects.has_shadow = has_shadow;
    o.effects.shadow_blur = shadow_blur;
    o.effects.shadow_offset[0] = off_x;
    o.effects.shadow_offset[1] = off_y;
    int e = EFX_TEXT_OK;
    efx_text_font *f = efx_text_font_create(s->fd, &o, &e);
    free(cps);
    if (!f) return 0;
    int id = font_alloc();
    if (!id) {
        efx_text_font_destroy(f);
        return 0;
    }
    FONTS.slots[id - 1].font = f;
    FONTS.slots[id - 1].alive = 1;
    return id;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_font_destroy(int id) {
    font_slot *s = font_get(id);
    if (s && s->alive) {
        s->alive = 0;
        efx_text_font_destroy(s->font);
        s->font = NULL;
    }
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_font_size(int id) {
    font_slot *s = font_get(id);
    return (s && s->alive) ? efx_text_font_size(s->font) : 0.0f;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_font_line_height(int id) {
    font_slot *s = font_get(id);
    return (s && s->alive) ? efx_text_font_line_height(s->font) : 0.0f;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_font_ascent(int id) {
    font_slot *s = font_get(id);
    return (s && s->alive) ? efx_text_font_ascent(s->font) : 0.0f;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_font_descent(int id) {
    font_slot *s = font_get(id);
    return (s && s->alive) ? efx_text_font_descent(s->font) : 0.0f;
}

static void fill_layout(efx_text_layout_opts *lo, int align, int valign,
                        int has_width, float width, int has_lh, float lh,
                        float scale, float rotation) {
    memset(lo, 0, sizeof(*lo));
    lo->align = align;
    lo->valign = valign;
    lo->has_width = has_width;
    lo->width = width;
    lo->has_line_height = has_lh;
    lo->line_height = lh;
    lo->scale = scale;
    lo->rotation = rotation;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_text_measure(
    const char *text, int font_id, int align, int valign, int has_width,
    float width, int has_lh, float lh, float scale, float rotation,
    float *out) {
    font_slot *s = font_get(font_id);
    if (!s || !s->alive || !text || !out) return EFX_TEXT_ERR_HANDLE;
    efx_text_layout_opts lo;
    fill_layout(&lo, align, valign, has_width, width, has_lh, lh, scale,
                rotation);
    efx_text_bounds b;
    int rc = efx_text_measure(s->font, text, &lo, &b);
    if (rc != EFX_TEXT_OK) return rc;
    out[0] = b.width;
    out[1] = b.height;
    out[2] = (float)b.lines;
    return EFX_TEXT_OK;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_text_draw(
    const char *text, int font_id, float x, float y, int align, int valign,
    int has_width, float width, int has_lh, float lh, float scale,
    float rotation, float cr, float cg, float cb, float ca, float orr,
    float og, float ob, float oa, float sr, float sg, float sb, float sa,
    float *out) {
    font_slot *s = font_get(font_id);
    if (!s || !s->alive || !text || !out) return EFX_TEXT_ERR_HANDLE;
    efx_text_layout_opts lo;
    fill_layout(&lo, align, valign, has_width, width, has_lh, lh, scale,
                rotation);
    float color[4] = {cr, cg, cb, ca};
    float outline[4] = {orr, og, ob, oa};
    float shadow[4] = {sr, sg, sb, sa};
    efx_text_bounds b;
    int rc = efx_text_draw(s->font, text, x, y, &lo, color, outline, shadow,
                           &b);
    if (rc != EFX_TEXT_OK) return rc;
    out[0] = b.width;
    out[1] = b.height;
    out[2] = (float)b.lines;
    return EFX_TEXT_OK;
}

