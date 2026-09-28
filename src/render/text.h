#ifndef EFX_TEXT_H
#define EFX_TEXT_H

/*
 * Font + text module (F8a): TrueType/OpenType loading, fixed glyph-atlas
 * baking, C-side typesetting, and opaque 2D text drawing through the display
 * list.
 *
 * Pure C, no sokol, no quickjs (ADR 0003 module walls). Font parsing and
 * glyph rasterization use the vendored stb_truetype; atlas packing uses the
 * vendored stb_rect_pack. The atlas is uploaded through the render module's
 * texture path and text records go through efx_render_quad.
 */

#include <stddef.h>
#include <stdint.h>

#include "resource/resource.h"

/* error codes */
#define EFX_TEXT_OK 0
#define EFX_TEXT_ERR_OPEN 1      /* root missing / font file unreadable */
#define EFX_TEXT_ERR_PARSE 2     /* not a parsable font */
#define EFX_TEXT_ERR_NOMEM 3
#define EFX_TEXT_ERR_RANGE 4     /* out-of-range numeric option */
#define EFX_TEXT_ERR_ATLAS 5     /* glyph set does not fit the atlas */
#define EFX_TEXT_ERR_HANDLE 6    /* bad/destroyed handle */

/* horizontal alignment */
#define EFX_TEXT_ALIGN_LEFT 0
#define EFX_TEXT_ALIGN_CENTER 1
#define EFX_TEXT_ALIGN_RIGHT 2
#define EFX_TEXT_ALIGN_JUSTIFY 3

/* vertical alignment */
#define EFX_TEXT_VALIGN_TOP 0
#define EFX_TEXT_VALIGN_MIDDLE 1
#define EFX_TEXT_VALIGN_BOTTOM 2

/* baked outline/shadow effects (geometry is baked into the atlas; colors are
 * supplied per draw). Absent effects are zeroed. */
typedef struct efx_text_effects {
    int has_outline;
    float outline_width;      /* px, > 0 */
    int has_shadow;
    float shadow_blur;        /* px, > 0 */
    float shadow_offset[2];   /* px, applied at draw */
} efx_text_effects;

/* createFont options (see docs/js-api.md) */
typedef struct efx_font_opts {
    float size;               /* required, > 0 */
    const uint32_t *codepoints; /* baked set; NULL => default Latin-1 */
    int codepoint_count;      /* 0 with NULL codepoints => default */
    int padding;              /* atlas gutter px, >= 0 */
    int filter;               /* EFX_FILTER_NEAREST / EFX_FILTER_LINEAR */
    efx_text_effects effects;
} efx_font_opts;

/* layout options shared by measure/draw (colors are draw-only) */
typedef struct efx_text_layout_opts {
    int align;                /* EFX_TEXT_ALIGN_* */
    int valign;               /* EFX_TEXT_VALIGN_* */
    int has_width;            /* wrap width supplied */
    float width;              /* wrap width px */
    int has_line_height;      /* override supplied */
    float line_height;        /* px (pre-scale) */
    float scale;              /* uniform draw scale, default 1 */
    float rotation;           /* degrees about the anchor, default 0 */
} efx_text_layout_opts;

typedef struct efx_text_bounds {
    float width;              /* widest line */
    float height;             /* lines * lineHeight * scale */
    int lines;
} efx_text_bounds;

typedef struct efx_text_fontdata efx_text_fontdata;
typedef struct efx_text_font efx_text_font;

/* ------------------------------------------------------------- font data */

/* Loads and parses a .ttf/.otf from the provider. NULL + *err on failure. */
efx_text_fontdata *efx_text_fontdata_load(efx_resource *res, const char *path,
                                          int *err);
void efx_text_fontdata_destroy(efx_text_fontdata *fd); /* NULL safe, idempotent */
int efx_text_fontdata_alive(const efx_text_fontdata *fd);

/* ---------------------------------------------------------------- fonts */

/* Bakes a fixed atlas from font data. NULL + *err on failure. */
efx_text_font *efx_text_font_create(const efx_text_fontdata *fd,
                                    const efx_font_opts *opts, int *err);
void efx_text_font_destroy(efx_text_font *f); /* NULL safe, idempotent */
int efx_text_font_alive(const efx_text_font *f);

/* read-only metrics (px, at the baked size; 0 for a dead font) */
float efx_text_font_size(const efx_text_font *f);
float efx_text_font_line_height(const efx_text_font *f);
float efx_text_font_ascent(const efx_text_font *f);
float efx_text_font_descent(const efx_text_font *f);

/* the atlas texture handle (0 for a dead font); engine-owned */
uint64_t efx_text_font_texture(const efx_text_font *f);

/* ------------------------------------------------------------- layout */

/* Lays out `utf8` and writes its bounds. Does not draw. */
int efx_text_measure(const efx_text_font *f, const char *utf8,
                     const efx_text_layout_opts *opts, efx_text_bounds *out);

/* Lays out and records the text as display-list quads. Returns the bounds. */
int efx_text_draw(const efx_text_font *f, const char *utf8, float x, float y,
                  const efx_text_layout_opts *opts, const float color[4],
                  const float outline_color[4], const float shadow_color[4],
                  efx_text_bounds *out);

/* --------------------------------------------------------- default set */

/* Writes a malloc'd array of the default (printable Latin-1) codepoints and
 * returns the count; caller frees. Returns -1 on OOM. */
int efx_text_default_codepoints(uint32_t **out);

/* Decodes UTF-8 into a malloc'd codepoint array (U+FFFD for invalid bytes);
 * returns the count or -1 on OOM. Caller frees. */
int efx_text_codepoints(const char *utf8, uint32_t **out);

#endif
