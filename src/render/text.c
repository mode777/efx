/*
 * Font + text module implementation (F8a). See text.h for the contract.
 *
 * Pipeline: efx_text_fontdata_load (read + stbtt parse) ->
 * efx_text_font_create (rasterize the baked set, bake outline/shadow
 * variants, pack with stb_rect_pack, upload the RGBA8 atlas as a Texture) ->
 * efx_text_measure / efx_text_draw (layout + display-list quads).
 *
 * Determinism: codepoints are sorted/deduped, packed in that order with the
 * vendored packer, and all layout math is plain float/integer arithmetic, so
 * the atlas and bounds are reproducible across targets.
 */

#include "render/text.h"

#include "render/render.h"

#include "stb_rect_pack.h"
#include "stb_truetype.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define EFX_TEXT_ATLAS_MIN 128
#define EFX_TEXT_ATLAS_MAX 4096

typedef struct efx_glyph_variant {
    int present;
    int tx, ty, tw, th;  /* atlas rect in texels */
    float bx, by;        /* bitmap top-left offset from pen (px, y-down) */
} efx_glyph_variant;

typedef struct efx_glyph {
    uint32_t cp;
    float advance;       /* px at the baked size */
    efx_glyph_variant fill;
    efx_glyph_variant outline;
    efx_glyph_variant shadow;
} efx_glyph;

typedef struct efx_kern_pair {
    uint32_t a, b;
    float kern;          /* px at the baked size */
} efx_kern_pair;

struct efx_text_fontdata {
    uint8_t *bytes;
    size_t size;
    stbtt_fontinfo info;
    int alive;
};

struct efx_text_font {
    uint64_t texture;
    float size;
    float fscale;        /* stbtt scale (font units -> px) */
    float line_height;
    float ascent;
    float descent;
    efx_glyph *glyphs;
    int glyph_count;
    efx_kern_pair *kerns;
    int kern_count;
    int has_outline;
    int has_shadow;
    float shadow_offset[2];
    int alive;
};

/* --------------------------------------------------------------- helpers */

static int cmp_u32(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static int utf8_next(const char *s, size_t *i, size_t len, uint32_t *cp);

/* malloc'd array of the default (printable Latin-1) codepoints; returns the
 * count, or -1 on OOM. Caller frees. */
static int efx_text_default_codepoints(uint32_t **out) {
    int n = 0;
    for (uint32_t c = 0x20; c <= 0x7E; c++) n++;
    for (uint32_t c = 0xA0; c <= 0xFF; c++) n++;
    uint32_t *arr = malloc((size_t)n * sizeof(uint32_t));
    if (!arr) return -1;
    int i = 0;
    for (uint32_t c = 0x20; c <= 0x7E; c++) arr[i++] = c;
    for (uint32_t c = 0xA0; c <= 0xFF; c++) arr[i++] = c;
    *out = arr;
    return n;
}

int efx_text_codepoints(const char *utf8, uint32_t **out) {
    if (!utf8) {
        *out = NULL;
        return 0;
    }
    size_t len = strlen(utf8);
    uint32_t *arr = malloc((len + 1) * sizeof(uint32_t));
    if (!arr) return -1;
    size_t i = 0;
    int n = 0;
    uint32_t cp;
    while (utf8_next(utf8, &i, len, &cp)) arr[n++] = cp;
    *out = arr;
    return n;
}

/* UTF-8 decode; returns 1 and advances *i, or 0 at the end of string. Invalid
 * or truncated bytes decode to U+FFFD (one byte consumed). */
static int utf8_next(const char *s, size_t *i, size_t len, uint32_t *cp) {
    if (*i >= len) return 0;
    unsigned char c = (unsigned char)s[*i];
    if (c < 0x80) {
        *cp = c;
        (*i)++;
        return 1;
    }
    int need;
    uint32_t v, min;
    if ((c & 0xE0) == 0xC0) { need = 1; v = c & 0x1Fu; min = 0x80u; }
    else if ((c & 0xF0) == 0xE0) { need = 2; v = c & 0x0Fu; min = 0x800u; }
    else if ((c & 0xF8) == 0xF0) { need = 3; v = c & 0x07u; min = 0x10000u; }
    else { *cp = 0xFFFD; (*i)++; return 1; }
    if (*i + (size_t)need >= len) { *cp = 0xFFFD; (*i)++; return 1; }
    for (int k = 1; k <= need; k++) {
        unsigned char cc = (unsigned char)s[*i + (size_t)k];
        if ((cc & 0xC0) != 0x80) { *cp = 0xFFFD; (*i)++; return 1; }
        v = (v << 6) | (cc & 0x3Fu);
    }
    if (v < min || v > 0x10FFFFu || (v >= 0xD800u && v <= 0xDFFFu)) {
        *cp = 0xFFFD;
        (*i)++;
        return 1;
    }
    *i += (size_t)need + 1;
    *cp = v;
    return 1;
}

static uint8_t *dilate_alpha(const uint8_t *src, int w, int h, int radius,
                             int *ow, int *oh) {
    if (radius < 1) radius = 1;
    int m = radius;
    int W = w + 2 * m, H = h + 2 * m;
    uint8_t *dst = calloc((size_t)W * (size_t)H, 1);
    if (!dst) return NULL;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint8_t mx = 0;
            for (int dy = -m; dy <= m; dy++) {
                int sy = y - m + dy;
                if (sy < 0 || sy >= h) continue;
                for (int dx = -m; dx <= m; dx++) {
                    int sx = x - m + dx;
                    if (sx < 0 || sx >= w) continue;
                    uint8_t v = src[sy * w + sx];
                    if (v > mx) mx = v;
                }
            }
            dst[y * W + x] = mx;
        }
    }
    *ow = W;
    *oh = H;
    return dst;
}

static uint8_t *blur_alpha(const uint8_t *src, int w, int h, int radius,
                           int *ow, int *oh) {
    if (radius < 1) radius = 1;
    int m = radius;
    int W = w + 2 * m, H = h + 2 * m;
    uint8_t *dst = calloc((size_t)W * (size_t)H, 1);
    if (!dst) return NULL;
    int count = (2 * m + 1) * (2 * m + 1);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int sum = 0;
            for (int dy = -m; dy <= m; dy++) {
                int sy = y - m + dy;
                if (sy < 0 || sy >= h) continue;
                for (int dx = -m; dx <= m; dx++) {
                    int sx = x - m + dx;
                    if (sx < 0 || sx >= w) continue;
                    sum += src[sy * w + sx];
                }
            }
            dst[y * W + x] = (uint8_t)(sum / count);
        }
    }
    *ow = W;
    *oh = H;
    return dst;
}

/* ---------------------------------------------------------- font data */

efx_text_fontdata *efx_text_fontdata_load(efx_resource *res, const char *path,
                                          int *err) {
    if (err) *err = EFX_TEXT_OK;
    if (!res || !path) {
        if (err) *err = EFX_TEXT_ERR_OPEN;
        return NULL;
    }
    size_t size = 0;
    int rerr = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(res, path, &size, &rerr);
    if (!bytes) {
        if (err) *err = EFX_TEXT_ERR_OPEN;
        return NULL;
    }
    efx_text_fontdata *fd = calloc(1, sizeof(*fd));
    if (!fd) {
        efx_resource_free(bytes);
        if (err) *err = EFX_TEXT_ERR_NOMEM;
        return NULL;
    }
    fd->bytes = bytes;
    fd->size = size;
    int off = stbtt_GetFontOffsetForIndex(bytes, 0);
    if (off < 0 || !stbtt_InitFont(&fd->info, bytes, off)) {
        free(fd->bytes);
        free(fd);
        if (err) *err = EFX_TEXT_ERR_PARSE;
        return NULL;
    }
    fd->alive = 1;
    return fd;
}

void efx_text_fontdata_destroy(efx_text_fontdata *fd) {
    if (!fd || !fd->alive) return;
    fd->alive = 0;
    free(fd->bytes);
    fd->bytes = NULL;
    free(fd);
}

int efx_text_fontdata_alive(const efx_text_fontdata *fd) {
    return fd && fd->alive;
}

/* --------------------------------------------------------------- fonts */

static int find_glyph_index(const efx_text_font *f, uint32_t cp) {
    int lo = 0, hi = f->glyph_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (f->glyphs[mid].cp == cp) return mid;
        if (f->glyphs[mid].cp < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

static float kern_px(const efx_text_font *f, uint32_t a, uint32_t b) {
    for (int i = 0; i < f->kern_count; i++) {
        if (f->kerns[i].a == a && f->kerns[i].b == b) return f->kerns[i].kern;
    }
    return 0.0f;
}

typedef struct bake_item {
    int g;          /* glyph index */
    int variant;    /* 0 fill, 1 outline, 2 shadow */
    uint8_t *bits;
    int w, h;
} bake_item;

static void free_font(efx_text_font *f) {
    if (!f) return;
    free(f->glyphs);
    free(f->kerns);
    free(f);
}

/* resolve the requested codepoint set (explicit list deduped + sorted, or the
 * default printable set); returns 0 on allocation failure */
static int resolve_codepoints(const efx_font_opts *opts, uint32_t **out_cps,
                              int *out_ncp, int *err) {
    uint32_t *cps = NULL;
    int ncp = 0;
    if (opts->codepoints && opts->codepoint_count > 0) {
        cps = malloc((size_t)opts->codepoint_count * sizeof(uint32_t));
        if (!cps) {
            if (err) *err = EFX_TEXT_ERR_NOMEM;
            return 0;
        }
        memcpy(cps, opts->codepoints,
               (size_t)opts->codepoint_count * sizeof(uint32_t));
        ncp = opts->codepoint_count;
        qsort(cps, (size_t)ncp, sizeof(uint32_t), cmp_u32);
        int w = 0;
        for (int i = 0; i < ncp; i++) {
            if (w == 0 || cps[i] != cps[w - 1]) cps[w++] = cps[i];
        }
        ncp = w;
    } else {
        ncp = efx_text_default_codepoints(&cps);
        if (ncp < 0) {
            if (err) *err = EFX_TEXT_ERR_NOMEM;
            return 0;
        }
    }
    *out_cps = cps;
    *out_ncp = ncp;
    return 1;
}

/* rasterize each codepoint's fill bitmap (plus baked outline/shadow variants)
 * into `items`; sets f->glyph_count and returns the item count */
static void rasterize_glyphs(const efx_text_fontdata *fd,
                             const efx_font_opts *opts, const uint32_t *cps,
                             int ncp, efx_text_font *f, bake_item *items,
                             int *out_nitems) {
    int nitems = 0;
    int glyph_count = 0;
    for (int i = 0; i < ncp; i++) {
        uint32_t cp = cps[i];
        int gi = stbtt_FindGlyphIndex(&fd->info, (int)cp);
        if (gi == 0) continue;
        int adv_u = 0, lsb_u = 0;
        stbtt_GetGlyphHMetrics(&fd->info, gi, &adv_u, &lsb_u);
        efx_glyph *g = &f->glyphs[glyph_count];
        g->cp = cp;
        g->advance = (float)adv_u * f->fscale;

        int w = 0, h = 0, xoff = 0, yoff = 0;
        unsigned char *bits = stbtt_GetCodepointBitmap(
            &fd->info, f->fscale, f->fscale, (int)cp, &w, &h, &xoff, &yoff);
        if (bits && w > 0 && h > 0) {
            g->fill.present = 1;
            g->fill.tw = w;
            g->fill.th = h;
            g->fill.bx = (float)xoff;
            g->fill.by = (float)yoff;
            items[nitems].g = glyph_count;
            items[nitems].variant = 0;
            items[nitems].bits = (uint8_t *)bits;
            items[nitems].w = w;
            items[nitems].h = h;
            nitems++;

            if (f->has_outline) {
                int m = (int)ceilf(opts->effects.outline_width);
                int ow = 0, oh = 0;
                uint8_t *ob = dilate_alpha((const uint8_t *)bits, w, h, m, &ow,
                                           &oh);
                if (ob) {
                    g->outline.present = 1;
                    g->outline.tw = ow;
                    g->outline.th = oh;
                    g->outline.bx = (float)(xoff - m);
                    g->outline.by = (float)(yoff - m);
                    items[nitems].g = glyph_count;
                    items[nitems].variant = 1;
                    items[nitems].bits = ob;
                    items[nitems].w = ow;
                    items[nitems].h = oh;
                    nitems++;
                }
            }
            if (f->has_shadow) {
                int m = (int)ceilf(opts->effects.shadow_blur);
                int ow = 0, oh = 0;
                uint8_t *sb = blur_alpha((const uint8_t *)bits, w, h, m, &ow,
                                         &oh);
                if (sb) {
                    g->shadow.present = 1;
                    g->shadow.tw = ow;
                    g->shadow.th = oh;
                    g->shadow.bx = (float)(xoff - m);
                    g->shadow.by = (float)(yoff - m);
                    items[nitems].g = glyph_count;
                    items[nitems].variant = 2;
                    items[nitems].bits = sb;
                    items[nitems].w = ow;
                    items[nitems].h = oh;
                    nitems++;
                }
            }
        } else if (bits) {
            stbtt_FreeBitmap(bits, NULL);
        }
        glyph_count++;
    }
    f->glyph_count = glyph_count;
    *out_nitems = nitems;
}

/* build the kerning pair table (best-effort: stops growing on OOM) */
static void build_kerning(const efx_text_fontdata *fd, efx_text_font *f,
                          int kern_cap) {
    for (int a = 0; a < f->glyph_count; a++) {
        for (int b = 0; b < f->glyph_count; b++) {
            int k = stbtt_GetCodepointKernAdvance(&fd->info,
                                                  (int)f->glyphs[a].cp,
                                                  (int)f->glyphs[b].cp);
            if (k != 0) {
                if (f->kern_count == kern_cap) {
                    kern_cap *= 2;
                    efx_kern_pair *grown = realloc(
                        f->kerns, (size_t)kern_cap * sizeof(efx_kern_pair));
                    if (!grown) break;
                    f->kerns = grown;
                }
                f->kerns[f->kern_count].a = f->glyphs[a].cp;
                f->kerns[f->kern_count].b = f->glyphs[b].cp;
                f->kerns[f->kern_count].kern = (float)k * f->fscale;
                f->kern_count++;
            }
        }
    }
}

/* find the smallest power-of-two atlas that packs every item; returns 0 on
 * OOM or pack failure (rects freed internally on failure) */
static int pack_atlas(const bake_item *items, int nitems, int pad,
                      stbrp_rect **out_rects, int *out_atlas_size, int *err) {
    int atlas_size = 0;
    stbrp_rect *rects = malloc((size_t)nitems * sizeof(stbrp_rect));
    if (!rects) {
        if (err) *err = EFX_TEXT_ERR_NOMEM;
        return 0;
    }
    for (int size = EFX_TEXT_ATLAS_MIN; size <= EFX_TEXT_ATLAS_MAX; size *= 2) {
        stbrp_node *nodes = malloc((size_t)size * sizeof(stbrp_node));
        if (!nodes) break;
        stbrp_context ctx;
        stbrp_init_target(&ctx, size, size, nodes, size);
        for (int k = 0; k < nitems; k++) {
            rects[k].id = k;
            rects[k].w = (stbrp_coord)(items[k].w + 2 * pad);
            rects[k].h = (stbrp_coord)(items[k].h + 2 * pad);
        }
        int ok = stbrp_pack_rects(&ctx, rects, nitems);
        free(nodes);
        if (ok) {
            atlas_size = size;
            break;
        }
    }
    if (atlas_size == 0) {
        free(rects);
        if (err) *err = EFX_TEXT_ERR_ATLAS;
        return 0;
    }
    *out_rects = rects;
    *out_atlas_size = atlas_size;
    return 1;
}

/* blit the packed items into an RGBA8 atlas and upload it; consumes `items`
 * and `rects`; returns 0 on OOM (font cleanup is the caller's job) */
static int build_atlas(efx_text_font *f, bake_item *items, int nitems,
                       stbrp_rect *rects, int atlas_size, int pad, int filter,
                       int *err) {
    size_t atlas_bytes = (size_t)atlas_size * (size_t)atlas_size * 4u;
    uint8_t *atlas = calloc(atlas_bytes, 1);
    if (!atlas) {
        free(rects);
        for (int k = 0; k < nitems; k++) free(items[k].bits);
        free(items);
        if (err) *err = EFX_TEXT_ERR_NOMEM;
        return 0;
    }

    for (int k = 0; k < nitems; k++) {
        int tx = rects[k].x + pad;
        int ty = rects[k].y + pad;
        efx_glyph *g = &f->glyphs[items[k].g];
        efx_glyph_variant *v = items[k].variant == 0   ? &g->fill
                               : items[k].variant == 1 ? &g->outline
                                                       : &g->shadow;
        v->tx = tx;
        v->ty = ty;
        for (int y = 0; y < items[k].h; y++) {
            for (int x = 0; x < items[k].w; x++) {
                uint8_t a = items[k].bits[y * items[k].w + x];
                size_t idx = ((size_t)(ty + y) * (size_t)atlas_size +
                              (size_t)(tx + x)) * 4u;
                atlas[idx + 0] = 255;
                atlas[idx + 1] = 255;
                atlas[idx + 2] = 255;
                atlas[idx + 3] = a;
            }
        }
        free(items[k].bits);
    }
    free(items);
    free(rects);

    f->texture = efx_render_texture_create(atlas_size, atlas_size, atlas,
                                           EFX_TEX_WRAP_CLAMP, filter, 0);
    free(atlas);
    if (!f->texture) {
        if (err) *err = EFX_TEXT_ERR_NOMEM;
        return 0;
    }
    return 1;
}

efx_text_font *efx_text_font_create(const efx_text_fontdata *fd,
                                    const efx_font_opts *opts, int *err) {
    if (err) *err = EFX_TEXT_OK;
    if (!fd || !fd->alive || !opts) {
        if (err) *err = EFX_TEXT_ERR_HANDLE;
        return NULL;
    }
    if (!(opts->size > 0.0f)) {
        if (err) *err = EFX_TEXT_ERR_RANGE;
        return NULL;
    }
    if (opts->padding < 0) {
        if (err) *err = EFX_TEXT_ERR_RANGE;
        return NULL;
    }
    if (opts->effects.has_outline && !(opts->effects.outline_width > 0.0f)) {
        if (err) *err = EFX_TEXT_ERR_RANGE;
        return NULL;
    }
    if (opts->effects.has_shadow && !(opts->effects.shadow_blur > 0.0f)) {
        if (err) *err = EFX_TEXT_ERR_RANGE;
        return NULL;
    }

    uint32_t *cps = NULL;
    int ncp = 0;
    if (!resolve_codepoints(opts, &cps, &ncp, err)) {
        return NULL;
    }

    efx_text_font *f = calloc(1, sizeof(*f));
    if (!f) {
        free(cps);
        if (err) *err = EFX_TEXT_ERR_NOMEM;
        return NULL;
    }
    f->size = opts->size;
    f->fscale = stbtt_ScaleForPixelHeight(&fd->info, opts->size);
    int ascent_u = 0, descent_u = 0, linegap_u = 0;
    stbtt_GetFontVMetrics(&fd->info, &ascent_u, &descent_u, &linegap_u);
    f->ascent = (float)ascent_u * f->fscale;
    f->descent = (float)descent_u * f->fscale;
    f->line_height = (float)(ascent_u - descent_u + linegap_u) * f->fscale;
    if (f->line_height <= 0.0f) f->line_height = opts->size;
    f->has_outline = opts->effects.has_outline;
    f->has_shadow = opts->effects.has_shadow;
    f->shadow_offset[0] = opts->effects.shadow_offset[0];
    f->shadow_offset[1] = opts->effects.shadow_offset[1];

    f->glyphs = calloc((size_t)(ncp > 0 ? ncp : 1), sizeof(efx_glyph));
    if (!f->glyphs) {
        free(cps);
        free_font(f);
        if (err) *err = EFX_TEXT_ERR_NOMEM;
        return NULL;
    }
    int item_cap = (ncp > 0 ? ncp : 1) * 3;
    bake_item *items = calloc((size_t)item_cap, sizeof(bake_item));
    if (!items) {
        free(cps);
        free_font(f);
        if (err) *err = EFX_TEXT_ERR_NOMEM;
        return NULL;
    }
    int kern_cap = 64;
    f->kerns = malloc((size_t)kern_cap * sizeof(efx_kern_pair));
    if (!f->kerns) {
        free(items);
        free(cps);
        free_font(f);
        if (err) *err = EFX_TEXT_ERR_NOMEM;
        return NULL;
    }
    f->kern_count = 0;

    int nitems = 0;
    rasterize_glyphs(fd, opts, cps, ncp, f, items, &nitems);
    free(cps);

    if (nitems == 0) {
        free(items);
        free_font(f);
        if (err) *err = EFX_TEXT_ERR_ATLAS;
        return NULL;
    }

    build_kerning(fd, f, kern_cap);

    stbrp_rect *rects = NULL;
    int atlas_size = 0;
    if (!pack_atlas(items, nitems, opts->padding, &rects, &atlas_size, err)) {
        for (int k = 0; k < nitems; k++) free(items[k].bits);
        free(items);
        free_font(f);
        return NULL;
    }

    if (!build_atlas(f, items, nitems, rects, atlas_size, opts->padding,
                     opts->filter, err)) {
        free_font(f);
        return NULL;
    }

    f->alive = 1;
    return f;
}

void efx_text_font_destroy(efx_text_font *f) {
    if (!f || !f->alive) return;
    f->alive = 0;
    if (f->texture) efx_render_texture_destroy(f->texture);
    free_font(f);
}

float efx_text_font_size(const efx_text_font *f) {
    return (f && f->alive) ? f->size : 0.0f;
}
float efx_text_font_line_height(const efx_text_font *f) {
    return (f && f->alive) ? f->line_height : 0.0f;
}
float efx_text_font_ascent(const efx_text_font *f) {
    return (f && f->alive) ? f->ascent : 0.0f;
}
float efx_text_font_descent(const efx_text_font *f) {
    return (f && f->alive) ? f->descent : 0.0f;
}
uint64_t efx_text_font_texture(const efx_text_font *f) {
    return (f && f->alive) ? f->texture : 0;
}

/* -------------------------------------------------------------- layout */

typedef struct text_line {
    int start, end;
    float width;
    int space_count;
    float justify_extra;
} text_line;

typedef struct text_layout {
    uint32_t *cps;
    int n;
    float *advance;
    float *kern;
    text_line *lines;
    int line_count;
    float max_width;
} text_layout;

static void layout_free(text_layout *L) {
    if (!L) return;
    free(L->cps);
    free(L->advance);
    free(L->kern);
    free(L->lines);
}

static float range_width(const text_layout *L, int start, int end) {
    float w = 0.0f;
    for (int j = start; j < end; j++) {
        w += L->advance[j];
        if (j > start) w += L->kern[j];
    }
    return w;
}

static int last_space(const text_layout *L, int start, int end) {
    for (int j = end - 1; j >= start; j--) {
        if (L->cps[j] == ' ') return j;
    }
    return -1;
}

static void push_line(text_layout *L, int *lc, int s, int e) {
    while (e > s && L->cps[e - 1] == ' ') e--;
    text_line *ln = &L->lines[(*lc)++];
    ln->start = s;
    ln->end = e;
    ln->width = range_width(L, s, e);
    ln->space_count = 0;
    ln->justify_extra = 0.0f;
    for (int j = s; j < e; j++)
        if (L->cps[j] == ' ') ln->space_count++;
}

static int layout_lines(text_layout *L, float wrap, int has_width) {
    L->lines = malloc((size_t)(L->n + 2) * sizeof(text_line));
    if (!L->lines) return -1;
    int lc = 0;
    int ls = 0;
    for (int i = 0; i <= L->n; i++) {
        if (i == L->n) {
            if (L->n > ls || lc == 0) push_line(L, &lc, ls, L->n);
            break;
        }
        if (L->cps[i] == '\n') {
            push_line(L, &lc, ls, i);
            ls = i + 1;
            continue;
        }
        if (!has_width || wrap <= 0.0f) continue;
        float w = range_width(L, ls, i + 1);
        if (w > wrap && i > ls) {
            if (L->cps[i] == ' ') {
                push_line(L, &lc, ls, i);
                ls = i + 1;
            } else {
                int k = last_space(L, ls, i);
                if (k >= 0) {
                    push_line(L, &lc, ls, k);
                    ls = k + 1;
                } else {
                    push_line(L, &lc, ls, i);
                    ls = i;
                }
            }
        }
    }
    L->line_count = lc;
    L->max_width = 0.0f;
    for (int i = 0; i < lc; i++)
        if (L->lines[i].width > L->max_width) L->max_width = L->lines[i].width;
    return 0;
}

static int build_layout(const efx_text_font *f, const char *utf8,
                        const efx_text_layout_opts *opts, text_layout *L) {
    memset(L, 0, sizeof(*L));
    if (!f || !f->alive || !utf8 || !opts) return EFX_TEXT_ERR_HANDLE;
    if (opts->align == EFX_TEXT_ALIGN_JUSTIFY && !opts->has_width)
        return EFX_TEXT_ERR_RANGE;

    size_t len = strlen(utf8);
    L->cps = malloc((len + 1) * sizeof(uint32_t));
    if (!L->cps) return EFX_TEXT_ERR_NOMEM;
    size_t i = 0;
    int n = 0;
    uint32_t cp;
    while (utf8_next(utf8, &i, len, &cp)) L->cps[n++] = cp;
    L->n = n;

    L->advance = malloc((size_t)(n + 2) * sizeof(float));
    L->kern = malloc((size_t)(n + 2) * sizeof(float));
    if (!L->advance || !L->kern) {
        layout_free(L);
        return EFX_TEXT_ERR_NOMEM;
    }
    for (int j = 0; j < n; j++) {
        int gi = find_glyph_index(f, L->cps[j]);
        float adv = 0.0f;
        if (gi >= 0) {
            adv = f->glyphs[gi].advance;
        } else {
            int q = find_glyph_index(f, '?');
            if (q >= 0) adv = f->glyphs[q].advance;
        }
        L->advance[j] = adv;
        L->kern[j] = (j > 0) ? kern_px(f, L->cps[j - 1], L->cps[j]) : 0.0f;
    }

    float scale = opts->scale > 0.0f ? opts->scale : 1.0f;
    float wrap_baked = opts->has_width ? (opts->width / scale) : 0.0f;
    if (layout_lines(L, wrap_baked, opts->has_width) != 0) {
        layout_free(L);
        return EFX_TEXT_ERR_NOMEM;
    }

    if (opts->align == EFX_TEXT_ALIGN_JUSTIFY) {
        for (int li = 0; li < L->line_count - 1; li++) {
            text_line *ln = &L->lines[li];
            if (ln->space_count > 0)
                ln->justify_extra =
                    (wrap_baked - ln->width) / (float)ln->space_count;
            if (ln->justify_extra < 0.0f) ln->justify_extra = 0.0f;
        }
    }
    return EFX_TEXT_OK;
}

static void compute_bounds(const efx_text_font *f, const text_layout *L,
                           const efx_text_layout_opts *opts,
                           efx_text_bounds *out) {
    float scale = opts->scale > 0.0f ? opts->scale : 1.0f;
    float lh = (opts->has_line_height ? opts->line_height : f->line_height);
    out->width = L->max_width * scale;
    out->height = lh * scale * (float)L->line_count;
    out->lines = L->line_count;
}

int efx_text_measure(const efx_text_font *f, const char *utf8,
                     const efx_text_layout_opts *opts, efx_text_bounds *out) {
    if (!out) return EFX_TEXT_ERR_HANDLE;
    text_layout L;
    int rc = build_layout(f, utf8, opts, &L);
    if (rc != EFX_TEXT_OK) return rc;
    compute_bounds(f, &L, opts, out);
    layout_free(&L);
    return EFX_TEXT_OK;
}

static void emit_quad(const efx_text_font *f, const efx_glyph_variant *v,
                      float dest_x, float dest_y, float off_x, float off_y,
                      float scale, float rotation, float ax, float ay,
                      const float color[4]) {
    if (!v->present) return;
    float w = (float)v->tw * scale;
    float h = (float)v->th * scale;
    float ox = w * 0.5f, oy = h * 0.5f;
    float lx = dest_x + off_x;
    float ly = dest_y + off_y;
    if (rotation != 0.0f) {
        float rad = rotation * (float)M_PI / 180.0f;
        float cs = cosf(rad), sn = sinf(rad);
        float px = lx + ox, py = ly + oy;
        float dx = px - ax, dy = py - ay;
        float rx = ax + cs * dx - sn * dy;
        float ry = ay + sn * dx + cs * dy;
        lx = rx - ox;
        ly = ry - oy;
    }
    float src[4] = {(float)v->tx, (float)v->ty, (float)v->tw, (float)v->th};
    efx_render_quad(lx, ly, w, h, f->texture, color, rotation, 1.0f, src, 1,
                    ox, oy, EFX_BLEND_INHERIT);
}

int efx_text_draw(const efx_text_font *f, const char *utf8, float x, float y,
                  const efx_text_layout_opts *opts, const float color[4],
                  const float outline_color[4], const float shadow_color[4],
                  efx_text_bounds *out) {
    if (!f || !f->alive) return EFX_TEXT_ERR_HANDLE;
    if (!opts) return EFX_TEXT_ERR_HANDLE;
    if (opts->align == EFX_TEXT_ALIGN_JUSTIFY && !opts->has_width)
        return EFX_TEXT_ERR_RANGE;
    text_layout L;
    int rc = build_layout(f, utf8, opts, &L);
    if (rc != EFX_TEXT_OK) return rc;
    if (out) compute_bounds(f, &L, opts, out);

    float scale = opts->scale > 0.0f ? opts->scale : 1.0f;
    float lh = (opts->has_line_height ? opts->line_height : f->line_height) *
               scale;
    float ascent = f->ascent * scale;
    float block_h = lh * (float)L.line_count;
    float y0 = y;
    if (opts->valign == EFX_TEXT_VALIGN_MIDDLE) y0 = y - block_h * 0.5f;
    else if (opts->valign == EFX_TEXT_VALIGN_BOTTOM) y0 = y - block_h;

    static const float kWhite[4] = {1, 1, 1, 1};
    static const float kBlack[4] = {0, 0, 0, 1};
    const float *fc = color ? color : kWhite;
    const float *oc = outline_color ? outline_color : kBlack;
    const float *sc = shadow_color ? shadow_color : kBlack;

    for (int pass = 0; pass < 3; pass++) {
        if (pass == 0 && !f->has_shadow) continue;
        if (pass == 1 && !f->has_outline) continue;
        const float *col = pass == 0 ? sc : (pass == 1 ? oc : fc);
        float off_x = (pass == 0) ? f->shadow_offset[0] * scale : 0.0f;
        float off_y = (pass == 0) ? f->shadow_offset[1] * scale : 0.0f;
        for (int li = 0; li < L.line_count; li++) {
            const text_line *ln = &L.lines[li];
            float line_w = ln->width * scale;
            float start_x = x;
            if (opts->align == EFX_TEXT_ALIGN_CENTER) start_x = x - line_w * 0.5f;
            else if (opts->align == EFX_TEXT_ALIGN_RIGHT) start_x = x - line_w;
            float baseline = y0 + (float)li * lh + ascent;
            float pen = start_x;
            for (int j = ln->start; j < ln->end; j++) {
                if (j > ln->start) pen += L.kern[j] * scale;
                uint32_t cp = L.cps[j];
                float extra =
                    (opts->align == EFX_TEXT_ALIGN_JUSTIFY && cp == ' ')
                        ? ln->justify_extra * scale
                        : 0.0f;
                if (cp != ' ') {
                    int gi = find_glyph_index(f, cp);
                    if (gi < 0) gi = find_glyph_index(f, '?');
                    if (gi >= 0) {
                        const efx_glyph *g = &f->glyphs[gi];
                        const efx_glyph_variant *v = pass == 0   ? &g->shadow
                                                     : pass == 1 ? &g->outline
                                                                 : &g->fill;
                        if (v->present) {
                            float dx = pen + v->bx * scale;
                            float dy = baseline + v->by * scale;
                            emit_quad(f, v, dx, dy, off_x, off_y, scale,
                                      opts->rotation, x, y, col);
                        }
                    }
                }
                pen += L.advance[j] * scale + extra;
            }
        }
    }

    layout_free(&L);
    return EFX_TEXT_OK;
}
