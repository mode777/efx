#include <emscripten.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/platform.h"
#include "platform/audio_backend.h"
#include "audio/audio.h"
#include "physics/physics.h"
#include "physics/broadphase.h"
#include "render/render.h"
#include "input/efx_input.h"
#include "input/efx_gamepad.h"
#include "prelude/prelude.h"
#include "resource/gltf.h"
#include "resource/image.h"
#include "resource/resource.h"
#include "render/text.h"
#include "web/web.h"

#define EFX_WEB_ROOT_MAX 512

static struct {
    int quit_requested;
    int quit_code;
    int in_error;
    char **args;
    int arg_count;
    char root[EFX_WEB_ROOT_MAX];
    int dom;
    int golden_mode;
    int repl_requested;
    efx_platform_capture capture;
    double frame_last_now;
    int frame_have_now;
    efx_resource *resource; /* F6a provider for the current root */
    efx_physics_world *physics; /* F12 single world */
} W;

EM_JS(int, efx_web_has_dom_js, (void), {
    return (typeof document !== 'undefined') ? 1 : 0;
});

EM_JS(int, efx_web_call_hook_js, (int which, double dt), {
    return globalThis.__efxDispatchHook(which, dt);
});

EM_JS(void, efx_web_publish_exit, (int code), {
    Module['efxExitCode'] = code;
    Module['efxRunEnded'] = true;
});

static char *dup_string(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) {
        memcpy(p, s, n);
    }
    return p;
}

static void set_args(char *const *args, int count) {
    if (count > 0 && args) {
        W.args = calloc((size_t)count, sizeof(char *));
        if (!W.args) {
            return;
        }
        for (int i = 0; i < count; i++) {
            W.args[i] = dup_string(args[i]);
        }
        W.arg_count = count;
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_quit(int code) {
    W.quit_requested = 1;
    W.quit_code = code;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_error(void) {
    W.in_error = 1;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_fail(void) {
    W.in_error = 1;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_exit_code(void) {
    if (W.in_error) {
        return 1;
    }
    if (W.quit_requested) {
        return W.quit_code;
    }
    return 0;
}

/* F6d: the interactive console has no stdin on the web build; the boot JS
 * checks this to report unavailability (and a non-zero exit) instead of
 * silently ignoring `--repl`. */
EMSCRIPTEN_KEEPALIVE int efx_bridge_repl_requested(void) {
    return W.repl_requested;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_arg_count(void) {
    return W.arg_count;
}

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_arg(int i) {
    if (i < 0 || i >= W.arg_count || !W.args) {
        return "";
    }
    return W.args[i];
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_clear_color(float r, float g, float b, float a) {
    float c[4] = {r, g, b, a};
    efx_render_set_clear_color(c);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_camera(float fw, float fh, float x, float y,
                                                float zoom, float rotation) {
    efx_camera2d cam;
    memset(&cam, 0, sizeof(cam));
    cam.frame_w = fw;
    cam.frame_h = fh;
    cam.x = x;
    cam.y = y;
    cam.zoom = zoom;
    cam.rotation = rotation;
    efx_render_set_camera(&cam);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_blend(int mode) {
    efx_render_set_blend(mode);
}

typedef struct {
    uint8_t *pixels;
    int w, h;
    int alive;
} img_slot;

static struct {
    img_slot *slots;
    int count, cap;
} IMG;

static img_slot *img_get(int id) {
    if (id <= 0 || id > IMG.count) {
        return NULL;
    }
    return &IMG.slots[id - 1];
}

EMSCRIPTEN_KEEPALIVE uint8_t *efx_bridge_imagedata_alloc(int bytes) {
    if (bytes <= 0) {
        return NULL;
    }
    return (uint8_t *)malloc((size_t)bytes);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_mem_free(void *p) {
    free(p);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_imagedata_commit(int w, int h, uint8_t *pixels) {
    if (IMG.count >= IMG.cap) {
        int cap = IMG.cap ? IMG.cap * 2 : 64;
        img_slot *grown = realloc(IMG.slots, (size_t)cap * sizeof(img_slot));
        if (!grown) {
            return 0;
        }
        IMG.slots = grown;
        IMG.cap = cap;
    }
    img_slot *s = &IMG.slots[IMG.count];
    memset(s, 0, sizeof(*s));
    s->pixels = pixels;
    s->w = w;
    s->h = h;
    s->alive = 1;
    IMG.count++;
    return IMG.count; /* 1-based id; never reused (wrapper identity stays valid) */
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_imagedata_destroy(int id) {
    img_slot *s = img_get(id);
    if (!s || !s->alive) {
        return;
    }
    s->alive = 0;
    free(s->pixels);
    s->pixels = NULL;
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_texture_create(int id, int wrap,
                                                      int filter,
                                                      int mipmaps) {
    img_slot *s = img_get(id);
    if (!s || !s->alive) {
        return 0;
    }
    return (double)efx_render_texture_create(s->w, s->h, s->pixels, wrap,
                                             filter, mipmaps);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_texture_destroy(double handle) {
    efx_render_texture_destroy((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_texture_alive(double handle) {
    return efx_render_texture_alive((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_texture_width(double handle) {
    int w = 0, h = 0;
    efx_render_texture_size((uint64_t)handle, &w, &h);
    return w;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_texture_height(double handle) {
    int w = 0, h = 0;
    efx_render_texture_size((uint64_t)handle, &w, &h);
    return h;
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_white_texture(void) {
    return (double)efx_render_white_texture();
}

/* ------------------------------------------------ F6a resource loading */

static void web_open_root(void) {
    if (W.resource) {
        efx_resource_close(W.resource);
        W.resource = NULL;
    }
    if (W.root[0]) {
        int e = EFX_RESOURCE_OK;
        W.resource = efx_resource_open(W.root, &e);
    }
}

/* Point the provider at a new root (directory or mounted zip). Returns 1 on
 * success, 0 when the root cannot be opened. Used by entry.js after it has
 * written a fetched asset archive into the filesystem. */
EMSCRIPTEN_KEEPALIVE int efx_bridge_set_root(const char *path) {
    snprintf(W.root, sizeof(W.root), "%s", path ? path : "");
    web_open_root();
    return W.resource ? 1 : 0;
}

/* Returns a malloc'd NUL-terminated string the JS side frees with
 * _efx_bridge_mem_free, or NULL on failure. */
EMSCRIPTEN_KEEPALIVE const char *efx_bridge_load_text(const char *path) {
    if (!W.resource) {
        return NULL;
    }
    int e = EFX_RESOURCE_OK;
    return efx_resource_read_text(W.resource, path, &e);
}

/* Decodes an image into an ImageData slot; returns the 1-based id or 0. */
EMSCRIPTEN_KEEPALIVE int efx_bridge_load_image(const char *path) {
    if (!W.resource) {
        return 0;
    }
    size_t n = 0;
    int e = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(W.resource, path, &n, &e);
    if (!bytes) {
        return 0;
    }
    int ie = EFX_IMAGE_OK;
    efx_image *img = efx_image_decode(bytes, n, &ie);
    efx_resource_free(bytes);
    if (!img) {
        return 0;
    }
    size_t sz = (size_t)img->width * (size_t)img->height * 4u;
    uint8_t *px = malloc(sz ? sz : 1);
    if (!px) {
        efx_image_free(img);
        return 0;
    }
    memcpy(px, img->pixels, sz);
    int w = img->width;
    int h = img->height;
    efx_image_free(img);
    return efx_bridge_imagedata_commit(w, h, px);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_imagedata_width(int id) {
    img_slot *s = img_get(id);
    return s ? s->w : 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_imagedata_height(int id) {
    img_slot *s = img_get(id);
    return s ? s->h : 0;
}

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

EMSCRIPTEN_KEEPALIVE int efx_bridge_fontdata_alive(int id) {
    fd_slot *s = fd_get(id);
    return s && s->alive;
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

EMSCRIPTEN_KEEPALIVE int efx_bridge_font_alive(int id) {
    font_slot *s = font_get(id);
    return s && s->alive;
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

/* ------------------------------------------------- F5a (render targets) */

EMSCRIPTEN_KEEPALIVE double efx_bridge_target_create(int w, int h) {
    return (double)efx_render_target_create(w, h);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_target_destroy(double handle) {
    efx_render_target_destroy((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_target_alive(double handle) {
    return efx_render_target_alive((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_target_width(double handle) {
    int w = 0, h = 0;
    efx_render_target_size((uint64_t)handle, &w, &h);
    return w;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_target_height(double handle) {
    int w = 0, h = 0;
    efx_render_target_size((uint64_t)handle, &w, &h);
    return h;
}

/* returns an EFX_RENDER_* code; the JS wrapper maps it to exceptions */
EMSCRIPTEN_KEEPALIVE int efx_bridge_target_begin(double handle) {
    return efx_render_begin_target((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_target_end(void) {
    return efx_render_end_target();
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_draw_quad(double handle, float x, float y, float w, float h,
                                              float cr, float cg, float cb, float ca,
                                              float rotation, float scale,
                                              float sx, float sy, float sw, float sh,
                                              int has_src, float origin_x, float origin_y) {
    float color[4] = {cr, cg, cb, ca};
    float src[4] = {sx, sy, sw, sh};
    return efx_render_quad(x, y, w, h, (uint64_t)handle, color, rotation, scale,
                           src, has_src, origin_x, origin_y);
}

/* ------------------------------------------- F11 (billboards + particles) */

/* particle wire layout (floats); kept in sync with src/web/entry.js:
 *   0 max, 1 space, 2 facing, 3 blend, 4 lifeMin, 5 lifeMax, 6 emissionRate,
 *   7 emitterLifetime, 8 speedScale, 9 spread, 10 sizeCount, 11 sizeVariation,
 *   12 colorCount, 13 relativeRotation, 14 shape, 15 quadCount,
 *   16 rotMin, 17 rotMax, 18 spinStart, 19 spinEnd, 20 spinVariation,
 *   21..23 position, 24..26 direction, 27 speedMin, 28 speedMax,
 *   29..31 gravity, 32..34 linAccMin, 35..37 linAccMax,
 *   38 radialMin, 39 radialMax, 40 tangMin, 41 tangMax, 42 dampMin, 43 dampMax,
 *   44..51 sizes[8], 52..83 colors[8][4], 84..86 shapeSize, 87..342 quads[64][4],
 *   343..345 normal, 346 insertMode */
#define EFX_PART_WIRE_LEN 352

static void web_particles_read(const float *w, double texture,
                               efx_particle_config *c) {
    memset(c, 0, sizeof(*c));
    c->texture = (uint64_t)texture;
    c->max = (int)w[0];
    c->space = (int)w[1];
    c->facing = (int)w[2];
    c->blend = (int)w[3];
    c->life_min = w[4];
    c->life_max = w[5];
    c->emission_rate = w[6];
    c->emitter_lifetime = w[7];
    c->speed_scale = w[8];
    c->spread = w[9];
    c->size_count = (int)w[10];
    c->size_variation = w[11];
    c->color_count = (int)w[12];
    c->relative_rotation = (int)w[13];
    c->shape = (int)w[14];
    c->quad_count = (int)w[15];
    c->rotation_min = w[16];
    c->rotation_max = w[17];
    c->spin_start = w[18];
    c->spin_end = w[19];
    c->spin_variation = w[20];
    for (int i = 0; i < 3; i++) c->position[i] = w[21 + i];
    for (int i = 0; i < 3; i++) c->direction[i] = w[24 + i];
    c->speed_min = w[27];
    c->speed_max = w[28];
    for (int i = 0; i < 3; i++) c->gravity[i] = w[29 + i];
    for (int i = 0; i < 3; i++) c->lin_acc_min[i] = w[32 + i];
    for (int i = 0; i < 3; i++) c->lin_acc_max[i] = w[35 + i];
    c->radial_acc_min = w[38];
    c->radial_acc_max = w[39];
    c->tangential_acc_min = w[40];
    c->tangential_acc_max = w[41];
    c->damping_min = w[42];
    c->damping_max = w[43];
    for (int i = 0; i < 8; i++) c->sizes[i] = w[44 + i];
    for (int i = 0; i < 8; i++) {
        for (int k = 0; k < 4; k++) c->colors[i][k] = w[52 + i * 4 + k];
    }
    for (int i = 0; i < 3; i++) c->shape_size[i] = w[84 + i];
    for (int i = 0; i < 64; i++) {
        for (int k = 0; k < 4; k++) c->quads[i][k] = w[87 + i * 4 + k];
    }
    for (int i = 0; i < 3; i++) c->normal[i] = w[343 + i];
    c->insert_mode = (int)w[346];
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_particles_create(const float *wire,
                                                        double texture) {
    if (!wire) {
        return 0;
    }
    efx_particle_config c;
    web_particles_read(wire, texture, &c);
    return (double)efx_render_particles_create(&c, NULL);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_particles_set(double handle,
                                                  const float *wire,
                                                  double texture) {
    if (!wire) {
        return EFX_RENDER_ERR_SIZE;
    }
    efx_particle_config c;
    web_particles_read(wire, texture, &c);
    return efx_render_particles_set((uint64_t)handle, &c);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_destroy(double handle) {
    efx_render_particles_destroy((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_particles_alive(double handle) {
    return efx_render_particles_alive((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_particles_count(double handle) {
    return efx_render_particles_count((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_particles_emit(double handle, int n) {
    return efx_render_particles_emit((uint64_t)handle, n);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_start(double handle) {
    efx_render_particles_start((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_stop(double handle) {
    efx_render_particles_stop((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_pause(double handle) {
    efx_render_particles_pause((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_reset(double handle) {
    efx_render_particles_reset((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_particles_speed_scale(double handle) {
    return efx_render_particles_speed_scale((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_set_speed_scale(double handle,
                                                               float s) {
    efx_render_particles_set_speed_scale((uint64_t)handle, s);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_particles_draw(double handle) {
    return efx_render_particles_draw((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_draw_billboard(double texture,
                                                   const float *pos, float w,
                                                   float h, float cr, float cg,
                                                   float cb, float ca,
                                                   float rotation, int facing,
                                                   const float *normal,
                                                   int depth_test, float sx,
                                                   float sy, float sw, float sh,
                                                   int has_src) {
    if (!pos || !normal) {
        return EFX_RENDER_ERR_HANDLE;
    }
    float color[4] = {cr, cg, cb, ca};
    float src[4] = {sx, sy, sw, sh};
    return efx_render_billboard((uint64_t)texture, pos, w, h, color, rotation,
                                facing, normal, depth_test, src, has_src);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_draw_sprite(double texture, float x,
                                                float y, float w, float h,
                                                float cr, float cg, float cb,
                                                float ca, float rotation,
                                                float scale, float sx, float sy,
                                                float sw, float sh, int has_src,
                                                float ox, float oy) {
    float color[4] = {cr, cg, cb, ca};
    float src[4] = {sx, sy, sw, sh};
    return efx_render_quad(x, y, w, h, (uint64_t)texture, color, rotation, scale,
                           src, has_src, ox, oy);
}

/* ------------------------------------------------- F5b (post effects) */

/* wire layout: 9 floats per entry —
 *   [0] effect, [1] mix,
 *   colorFilter: [2] brightness [3] contrast [4] saturation [5..8] tint
 *   blur:        [2] radius
 *   bloom:       [2] threshold [3] strength
 * The JS layer validates fields/values (desktop parity); this maps the wire
 * into the engine snapshot and returns an EFX_POST_* code. */
#define EFX_POST_WIRE_STRIDE 9

EMSCRIPTEN_KEEPALIVE int efx_bridge_set_post_effects(const float *wire,
                                                     int count) {
    if (count < 0 || count > EFX_POST_MAX_ENTRIES) {
        return EFX_POST_ERR_COUNT;
    }
    if (count > 0 && !wire) {
        return EFX_POST_ERR_COUNT;
    }
    efx_post_entry entries[EFX_POST_MAX_ENTRIES];
    memset(entries, 0, sizeof(entries));
    for (int i = 0; i < count; i++) {
        const float *e = wire + (size_t)i * EFX_POST_WIRE_STRIDE;
        entries[i].effect = (int)e[0];
        entries[i].mix = e[1];
        switch (entries[i].effect) {
        case EFX_POST_COLOR_FILTER:
            entries[i].u.color_filter.brightness = e[2];
            entries[i].u.color_filter.contrast = e[3];
            entries[i].u.color_filter.saturation = e[4];
            for (int k = 0; k < 4; k++) {
                entries[i].u.color_filter.tint[k] = e[5 + k];
            }
            break;
        case EFX_POST_BLUR:
            entries[i].u.blur.radius = e[2];
            break;
        case EFX_POST_BLOOM:
            entries[i].u.bloom.threshold = e[2];
            entries[i].u.bloom.strength = e[3];
            break;
        default:
            return EFX_POST_ERR_UNKNOWN;
        }
    }
    return efx_render_set_post_effects(entries, count);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_set_render_scale(float scale, int filter) {
    return efx_render_set_render_scale(scale, filter);
}

/* ------------------------------------------------- F3 (3D core) */

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_js_prelude(void) {
    return EFX_JS_PRELUDE; /* NUL-terminated; EFX_JS_PRELUDE_LEN bytes */
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_camera3d(float px, float py, float pz,
                                                  float tx, float ty, float tz,
                                                  float fov, float near_z,
                                                  float far_z) {
    efx_camera3d cam;
    memset(&cam, 0, sizeof(cam));
    cam.pos[0] = px;
    cam.pos[1] = py;
    cam.pos[2] = pz;
    cam.target[0] = tx;
    cam.target[1] = ty;
    cam.target[2] = tz;
    cam.fov = fov;
    cam.near_z = near_z;
    cam.far_z = far_z;
    efx_render_set_camera3d(&cam);
}

/* MeshData slot table (mirrors the desktop api.c wrapper ownership).
   Surfaces arrive one at a time from JS: they are validated as a probe
   MeshData, then their storage moves into the staging array; commit
   assembles the final efx_meshdata. */
typedef struct {
    efx_surface *staged; /* surface_count entries; storage NULL until filled */
    int filled;
    efx_meshdata *md;    /* set by commit */
    int alive;
    int surface_count;
} wmd_slot;

static struct {
    wmd_slot *slots;
    int count, cap;
} WMD;

static wmd_slot *wmd_get(int id) {
    if (id <= 0 || id > WMD.count) {
        return NULL;
    }
    return &WMD.slots[id - 1];
}

static void wmd_release(wmd_slot *s) {
    if (s->staged) {
        for (int i = 0; i < s->surface_count; i++) {
            efx_surface *sf = &s->staged[i];
            free(sf->positions);
            free(sf->normals);
            free(sf->uvs);
            free(sf->colors);
            free(sf->joints);
            free(sf->weights);
            free(sf->indices);
        }
        free(s->staged);
        s->staged = NULL;
    }
    efx_meshdata_destroy(s->md);
    s->md = NULL;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_create(int surface_count) {
    if (surface_count < 1 || surface_count > EFX_MESH_MAX_SURFACES) {
        return 0;
    }
    if (WMD.count >= WMD.cap) {
        int cap = WMD.cap ? WMD.cap * 2 : 16;
        wmd_slot *grown = realloc(WMD.slots, (size_t)cap * sizeof(wmd_slot));
        if (!grown) {
            return 0;
        }
        WMD.slots = grown;
        WMD.cap = cap;
    }
    wmd_slot *s = &WMD.slots[WMD.count];
    memset(s, 0, sizeof(*s));
    s->staged = calloc((size_t)surface_count, sizeof(efx_surface));
    if (!s->staged) {
        return 0;
    }
    s->surface_count = surface_count;
    s->alive = 1;
    WMD.count++;
    return WMD.count;
}

/* fill one staged surface; returns 0 ok, EFX_MESHERR_* on validation
   failure (the staged MeshData is released; the JS layer throws) */
EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_surface(int id, int index,
                                                     const float *positions,
                                                     int positions_len,
                                                     const float *normals,
                                                     int normals_len,
                                                     const float *uvs,
                                                     int uvs_len,
                                                     const float *colors,
                                                     int colors_len,
                                                     const uint32_t *joints,
                                                     int joints_len,
                                                     const float *weights,
                                                     int weights_len,
                                                     const uint32_t *indices,
                                                     int indices_len) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || index < 0 || index >= s->surface_count) {
        return EFX_MESHERR_COUNT;
    }
    efx_surface_src src;
    memset(&src, 0, sizeof(src));
    src.positions = positions;
    src.positions_len = positions_len;
    src.normals = normals;
    src.normals_len = normals_len;
    src.uvs = uvs;
    src.uvs_len = uvs_len;
    src.colors = colors;
    src.colors_len = colors_len;
    src.joints = joints;
    src.joints_len = joints_len;
    src.weights = weights;
    src.weights_len = weights_len;
    src.indices = indices;
    src.indices_len = indices_len;
    int err = 0;
    efx_meshdata *probe = efx_meshdata_create(&src, 1, &err);
    if (!probe) {
        wmd_release(s);
        s->alive = 0;
        return err;
    }
    /* transfer the validated surface storage into the staging slot */
    s->staged[index] = probe->surfaces[0];
    free(probe->surfaces);
    free(probe);
    s->filled++;
    return EFX_MESHERR_OK;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_surface_count(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || !s->md) {
        return -1;
    }
    return s->md->surface_count;
}

/* assemble the final MeshData; returns 0 ok, EFX_MESHERR_* on failure */
EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_commit(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || !s->staged) {
        return EFX_MESHERR_COUNT;
    }
    if (s->filled != s->surface_count) {
        wmd_release(s);
        s->alive = 0;
        return EFX_MESHERR_LEN;
    }
    s->md = malloc(sizeof(efx_meshdata));
    if (!s->md) {
        wmd_release(s);
        s->alive = 0;
        return EFX_MESHERR_NOMEM;
    }
    s->md->surface_count = s->surface_count;
    s->md->surfaces = s->staged;
    s->md->rig = NULL; /* script-built meshes carry no rig payload */
    s->staged = NULL; /* ownership moved into the meshdata */
    return EFX_MESHERR_OK;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_meshdata_destroy(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive) {
        return;
    }
    s->alive = 0;
    wmd_release(s);
}

/* F6b: import a glTF mesh straight into a MeshData slot (same slot table as
 * createMeshData, so createMesh/surfaceCount/destroy are unchanged). Returns
 * the 1-based slot id, or 0 on failure. */
EMSCRIPTEN_KEEPALIVE int efx_bridge_load_meshdata(const char *path, int has_mesh,
                                                  int is_name, int index,
                                                  const char *name) {
    if (!W.resource || !path) {
        return 0;
    }
    efx_gltf_mesh_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.has_mesh = has_mesh;
    opts.is_name = is_name;
    opts.mesh_index = index;
    opts.mesh_name = name;
    int e = EFX_GLTF_OK;
    efx_meshdata *md = efx_gltf_load_meshdata(W.resource, path, &opts, &e);
    if (!md) {
        return 0;
    }
    if (WMD.count >= WMD.cap) {
        int cap = WMD.cap ? WMD.cap * 2 : 16;
        wmd_slot *grown = realloc(WMD.slots, (size_t)cap * sizeof(wmd_slot));
        if (!grown) {
            efx_meshdata_destroy(md);
            return 0;
        }
        WMD.slots = grown;
        WMD.cap = cap;
    }
    wmd_slot *s = &WMD.slots[WMD.count];
    memset(s, 0, sizeof(*s));
    s->md = md;
    s->alive = 1;
    s->surface_count = md->surface_count;
    WMD.count++;
    return WMD.count;
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_mesh_create(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || !s->md) {
        return 0;
    }
    return (double)efx_render_mesh_create(s->md);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_mesh_destroy(double handle) {
    efx_render_mesh_destroy((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_mesh_alive(double handle) {
    return efx_render_mesh_alive((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_mesh_surface_count(double handle) {
    return efx_render_mesh_surface_count((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_draw_mesh(double handle,
                                              const float *transform,
                                              const float *color,
                                              int skinned) {
    return efx_render_mesh((uint64_t)handle, transform, color, skinned);
}

/* F7: one wire pose sample: [clip_index, time, weight] per entry */
EMSCRIPTEN_KEEPALIVE int efx_bridge_pose_mesh(double handle,
                                              const float *wire, int count) {
    if (count < 0) {
        return EFX_RENDER_ERR_INDEX;
    }
    efx_pose_sample *samples = NULL;
    if (count > 0) {
        samples = malloc((size_t)count * sizeof(*samples));
        if (!samples) {
            return EFX_RENDER_ERR_NOMEM;
        }
        for (int i = 0; i < count; i++) {
            samples[i].clip = (int)wire[i * 3];
            samples[i].time = wire[i * 3 + 1];
            samples[i].weight = wire[i * 3 + 2];
        }
    }
    int rc = efx_render_mesh_pose((uint64_t)handle, samples, count);
    free(samples);
    return rc;
}

/* resolve a clip name to its index for the web binding's name lookup */
EMSCRIPTEN_KEEPALIVE int efx_bridge_find_clip(double handle, const char *name) {
    return efx_render_mesh_find_clip((uint64_t)handle, name);
}

/* ------------------------------------------------- F4a (lighting) */

/* mat layout: ambient[4], diffuse[4], specular[4], emissive[4], shininess;
 * maps (F4b): [ambient, diffuse, specular, emissive, alphaMask] as doubles
 * (a float would truncate a 64-bit texture handle) */
static void bridge_mat_from_wire(efx_material *m, const float *f,
                                 const double *maps) {
    if (!f) {
        efx_material_default(m);
        return;
    }
    for (int i = 0; i < 4; i++) {
        m->ambient[i] = f[i];
        m->diffuse[i] = f[4 + i];
        m->specular[i] = f[8 + i];
        m->emissive[i] = f[12 + i];
    }
    m->shininess = f[16];
    if (maps) {
        m->ambient_map = (uint64_t)maps[0];
        m->diffuse_map = (uint64_t)maps[1];
        m->specular_map = (uint64_t)maps[2];
        m->emissive_map = (uint64_t)maps[3];
        m->alpha_mask = (uint64_t)maps[4];
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_point_light(int slot, int enabled,
                                                     float px, float py,
                                                     float pz, float r, float g,
                                                     float b, float a,
                                                     float range) {
    (void)a; /* alpha ignored by lighting */
    if (!enabled) {
        efx_render_set_point_light(slot, NULL);
        return;
    }
    efx_point_light l;
    memset(&l, 0, sizeof(l));
    l.enabled = 1;
    l.pos[0] = px; l.pos[1] = py; l.pos[2] = pz;
    l.color[0] = r; l.color[1] = g; l.color[2] = b; l.color[3] = 1;
    l.range = range;
    efx_render_set_point_light(slot, &l);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_directional_light(int enabled,
                                                           float dx, float dy,
                                                           float dz, float r,
                                                           float g, float b,
                                                           float a) {
    (void)a;
    if (!enabled) {
        efx_render_set_directional_light(NULL);
        return;
    }
    efx_dir_light l;
    memset(&l, 0, sizeof(l));
    l.enabled = 1;
    l.dir[0] = dx; l.dir[1] = dy; l.dir[2] = dz;
    l.color[0] = r; l.color[1] = g; l.color[2] = b; l.color[3] = 1;
    efx_render_set_directional_light(&l);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_mesh_set_material(double handle, int index,
                                                      const float *mat,
                                                      const double *maps,
                                                      int has) {
    efx_material m;
    if (has) {
        bridge_mat_from_wire(&m, mat, maps);
    } else {
        efx_material_default(&m);
    }
    return efx_render_mesh_set_material((uint64_t)handle, index,
                                        has ? &m : NULL, has);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_set_material(int id, int index,
                                                          const float *mat,
                                                          const double *maps,
                                                          int has) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || !s->md) {
        return -1;
    }
    efx_material m;
    if (has) {
        bridge_mat_from_wire(&m, mat, maps);
    } else {
        efx_material_default(&m);
    }
    efx_meshdata_set_material(s->md, index, has ? &m : NULL, has);
    return 0;
}

/* ------------------------------------------------- F9 (input) */

/* name lookup + validation (single C source shared with the desktop binding) */
EMSCRIPTEN_KEEPALIVE int efx_bridge_key_id(const char *name) {
    return efx_input_key_id(name);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_button_id(const char *name) {
    return efx_input_button_id(name);
}

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_key_name(int key) {
    return efx_input_key_name(key);
}

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_button_name(int button) {
    return efx_input_button_name(button);
}

/* level / edge queries */
EMSCRIPTEN_KEEPALIVE int efx_bridge_key_down(int key) {
    return efx_input_key_is_down(key);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_key_pressed(int key) {
    return efx_input_key_is_pressed(key);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_key_released(int key) {
    return efx_input_key_is_released(key);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_button_down(int button) {
    return efx_input_button_is_down(button);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_button_pressed(int button) {
    return efx_input_button_is_pressed(button);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_button_released(int button) {
    return efx_input_button_is_released(button);
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_mouse_x(void) {
    float x = 0, y = 0;
    efx_input_pointer(&x, &y);
    return x;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_mouse_y(void) {
    float x = 0, y = 0;
    efx_input_pointer(&x, &y);
    return y;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_mouse_dx(void) {
    float dx = 0, dy = 0;
    efx_input_delta(&dx, &dy);
    return dx;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_mouse_dy(void) {
    float dx = 0, dy = 0;
    efx_input_delta(&dx, &dy);
    return dy;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_wheel_dx(void) {
    float dx = 0, dy = 0;
    efx_input_wheel_delta(&dx, &dy);
    return dx;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_wheel_dy(void) {
    float dx = 0, dy = 0;
    efx_input_wheel_delta(&dx, &dy);
    return dy;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_window_width(void) {
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return w;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_window_height(void) {
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return h;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_window_dpi(void) {
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return dpi;
}

/* frame event queue (entry.js builds the plain event objects) */
EMSCRIPTEN_KEEPALIVE int efx_bridge_input_count(void) {
    return efx_input_event_count();
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_type(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->type : -1;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_key(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->key : -1;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_button(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->button : -1;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_repeat(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->repeat : 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_mods(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? (int)ev->mods : 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_char(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? (int)ev->codepoint : -1;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_input_x(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->x : 0;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_input_y(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->y : 0;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_input_dx(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->dx : 0;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_input_dy(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->dy : 0;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_input_clear(void) {
    efx_input_clear_events();
}

/* ------------------------------------------------- F13 (gamepad) */

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_count(void) {
    return efx_input_gamepad_count();
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_connected(int slot) {
    return efx_input_gamepad_connected(slot);
}

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_gamepad_name(int slot) {
    const char *n = efx_input_gamepad_name(slot);
    return n ? n : "";
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_mapped(int slot) {
    return efx_input_gamepad_mapped(slot);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_button_id(const char *name) {
    return efx_input_gamepad_button_id(name);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_axis_id(const char *name) {
    return efx_input_gamepad_axis_id(name);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_button_down(int slot, int button) {
    return efx_input_gamepad_button_is_down(slot, button);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_button_pressed(int slot,
                                                           int button) {
    return efx_input_gamepad_button_is_pressed(slot, button);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_button_released(int slot,
                                                            int button) {
    return efx_input_gamepad_button_is_released(slot, button);
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_gamepad_axis(int slot, int axis) {
    return efx_input_gamepad_axis(slot, axis);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_raw_button(int slot, int index) {
    return efx_input_gamepad_raw_button(slot, index);
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_gamepad_raw_axis(int slot, int index) {
    return efx_input_gamepad_raw_axis(slot, index);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_connect_count(void) {
    return efx_input_gamepad_connect_count();
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_connect_at(int i) {
    return efx_input_gamepad_connect_at(i);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_disconnect_count(void) {
    return efx_input_gamepad_disconnect_count();
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_disconnect_at(int i) {
    return efx_input_gamepad_disconnect_at(i);
}

static int web_frame(void *ud, double dt) {
    (void)ud;
    int stop = 0;
    if (W.quit_requested || W.in_error) {
        stop = 1;
    } else if (efx_web_call_hook_js(1, dt) != 0) {
        stop = 1;
    } else if (W.quit_requested || W.in_error) {
        stop = 1;
    } else {
        /* F11: advance engine-owned particle systems after the update hooks
           and before the render hooks (auto-update, ADR 0039) */
        efx_render_particles_step((float)(dt > 0.0 ? dt : 0.0));
    }
    if (!stop && !(W.quit_requested || W.in_error) &&
        efx_web_call_hook_js(0, dt) != 0) {
        stop = 1;
    } else if (!stop && (W.quit_requested || W.in_error)) {
        stop = 1;
    }
    if (stop) {
        efx_web_publish_exit(efx_bridge_exit_code());
    }
    return stop;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_frame(void) {
    /* direct (Node harness) path bypasses the sokol frame loop, so derive dt
       here; the first frame reports 0 (desktop parity, design D3) */
    double now = emscripten_get_now();
    double dt = W.frame_have_now ? (now - W.frame_last_now) / 1000.0 : 0.0;
    W.frame_have_now = 1;
    W.frame_last_now = now;
    efx_input_begin_frame();
    int rc = web_frame(NULL, dt);
    efx_input_end_frame();
    return rc;
}

EMSCRIPTEN_KEEPALIVE const char *efx_web_root(void) {
    return W.root;
}

EMSCRIPTEN_KEEPALIVE void efx_web_set_golden_mode(void) {
    W.golden_mode = 1;
}

/* ================================================= F14 audio bridge */

typedef struct {
    efx_audio_data *data;
    int alive;
} web_adata_slot;

typedef struct {
    efx_audio_stream *stream;
    int alive;
} web_astream_slot;

typedef struct {
    int voice;
    long long serial;
    int alive;
} web_audio_slot;

static struct {
    web_adata_slot *slots;
    int count;
    int cap;
} WDATA;

static struct {
    web_astream_slot *slots;
    int count;
    int cap;
} WSTREAM;

static struct {
    web_audio_slot *slots;
    int count;
    int cap;
} WAUDIO;

static web_adata_slot *web_adata_get(int id) {
    if (id <= 0 || id > WDATA.count) return NULL;
    return &WDATA.slots[id - 1];
}

static web_astream_slot *web_astream_get(int id) {
    if (id <= 0 || id > WSTREAM.count) return NULL;
    return &WSTREAM.slots[id - 1];
}

static web_audio_slot *web_audio_get(int id) {
    if (id <= 0 || id > WAUDIO.count) return NULL;
    return &WAUDIO.slots[id - 1];
}

static int web_data_push(efx_audio_data *data) {
    if (WDATA.count == WDATA.cap) {
        int ncap = WDATA.cap ? WDATA.cap * 2 : 8;
        web_adata_slot *ns = realloc(WDATA.slots, (size_t)ncap * sizeof(*ns));
        if (!ns) return 0;
        WDATA.slots = ns;
        WDATA.cap = ncap;
    }
    WDATA.slots[WDATA.count].data = data;
    WDATA.slots[WDATA.count].alive = 1;
    return ++WDATA.count;
}

static int web_stream_push(efx_audio_stream *stream) {
    if (WSTREAM.count == WSTREAM.cap) {
        int ncap = WSTREAM.cap ? WSTREAM.cap * 2 : 8;
        web_astream_slot *ns =
            realloc(WSTREAM.slots, (size_t)ncap * sizeof(*ns));
        if (!ns) return 0;
        WSTREAM.slots = ns;
        WSTREAM.cap = ncap;
    }
    WSTREAM.slots[WSTREAM.count].stream = stream;
    WSTREAM.slots[WSTREAM.count].alive = 1;
    return ++WSTREAM.count;
}

static int web_audio_push(int voice) {
    if (WAUDIO.count == WAUDIO.cap) {
        int ncap = WAUDIO.cap ? WAUDIO.cap * 2 : 8;
        web_audio_slot *ns = realloc(WAUDIO.slots, (size_t)ncap * sizeof(*ns));
        if (!ns) return 0;
        WAUDIO.slots = ns;
        WAUDIO.cap = ncap;
    }
    WAUDIO.slots[WAUDIO.count].voice = voice;
    WAUDIO.slots[WAUDIO.count].serial = efx_audio_voice_serial(voice);
    WAUDIO.slots[WAUDIO.count].alive = 1;
    return ++WAUDIO.count;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_load_data(const char *path) {
    if (!W.resource || !path) return 0;
    size_t n = 0;
    int e = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(W.resource, path, &n, &e);
    if (!bytes) return 0;
    int de = 0;
    efx_audio_data *data = efx_audio_data_load(bytes, n, &de);
    efx_resource_free(bytes);
    if (!data) return 0;
    int id = web_data_push(data);
    if (!id) {
        efx_audio_data_release(data);
    }
    return id;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_load_stream(const char *path) {
    if (!W.resource || !path) return 0;
    size_t n = 0;
    int e = EFX_RESOURCE_OK;
    uint8_t *bytes = efx_resource_read(W.resource, path, &n, &e);
    if (!bytes) return 0;
    int de = 0;
    efx_audio_stream *stream = efx_audio_stream_load(bytes, n, &de);
    efx_resource_free(bytes);
    if (!stream) return 0;
    int id = web_stream_push(stream);
    if (!id) {
        efx_audio_stream_release(stream);
    }
    return id;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_data_destroy(int id) {
    web_adata_slot *s = web_adata_get(id);
    if (!s || !s->alive) return;
    s->alive = 0;
    if (s->data) {
        efx_audio_data_release(s->data);
        s->data = NULL;
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_stream_destroy(int id) {
    web_astream_slot *s = web_astream_get(id);
    if (!s || !s->alive) return;
    s->alive = 0;
    if (s->stream) {
        efx_audio_stream_release(s->stream);
        s->stream = NULL;
    }
}

static int web_audio_play(int voice) {
    if (voice < 0) return 0;
    int id = web_audio_push(voice);
    if (!id) {
        efx_audio_stop_voice(voice);
    }
    return id;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_play_data(int data_id, float volume,
                                                    float pan, float pitch,
                                                    int loop) {
    web_adata_slot *d = web_adata_get(data_id);
    if (!d || !d->alive || !d->data) return 0;
    return web_audio_play(
        efx_audio_play_data(d->data, volume, pan, pitch, loop));
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_play_stream(int stream_id,
                                                      float volume, float pan,
                                                      float pitch, int loop) {
    web_astream_slot *s = web_astream_get(stream_id);
    if (!s || !s->alive || !s->stream) return 0;
    return web_audio_play(
        efx_audio_play_stream(s->stream, volume, pan, pitch, loop));
}

static int web_audio_live(const web_audio_slot *a) {
    return a->voice >= 0 && efx_audio_voice_serial(a->voice) == a->serial;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_handle_playing(int id) {
    web_audio_slot *a = web_audio_get(id);
    if (!a || !a->alive) return 0;
    return web_audio_live(a) && efx_audio_voice_playing(a->voice) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_audio_handle_paused(int id) {
    web_audio_slot *a = web_audio_get(id);
    if (!a || !a->alive) return 0;
    return web_audio_live(a) && efx_audio_voice_paused(a->voice) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_stop(int id) {
    web_audio_slot *a = web_audio_get(id);
    if (!a || !a->alive) return;
    if (web_audio_live(a)) {
        efx_audio_stop_voice(a->voice);
    }
    a->voice = -1;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_pause(int id, int paused) {
    web_audio_slot *a = web_audio_get(id);
    if (a && a->alive && web_audio_live(a)) {
        efx_audio_set_voice_paused(a->voice, paused);
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_destroy(int id) {
    web_audio_slot *a = web_audio_get(id);
    if (!a || !a->alive) return;
    a->alive = 0;
    if (web_audio_live(a)) {
        efx_audio_stop_voice(a->voice);
    }
    a->voice = -1;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_set_volume(int id, float v) {
    web_audio_slot *a = web_audio_get(id);
    if (a && a->alive && web_audio_live(a)) {
        efx_audio_set_voice_volume(a->voice, v);
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_set_pan(int id, float v) {
    web_audio_slot *a = web_audio_get(id);
    if (a && a->alive && web_audio_live(a)) {
        efx_audio_set_voice_pan(a->voice, v);
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_set_pitch(int id, float v) {
    web_audio_slot *a = web_audio_get(id);
    if (a && a->alive && web_audio_live(a)) {
        efx_audio_set_voice_pitch(a->voice, v);
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_handle_set_loop(int id, int loop) {
    web_audio_slot *a = web_audio_get(id);
    if (a && a->alive && web_audio_live(a)) {
        efx_audio_set_voice_loop(a->voice, loop);
    }
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_audio_master_volume(void) {
    return efx_audio_master_volume();
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_set_master_volume(float v) {
    efx_audio_set_master_volume(v);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_audio_resume(void) {
    efx_audio_request_resume();
}


int efx_web_main(int argc, char *const *argv) {
    int golden = W.golden_mode;
    memset(&W, 0, sizeof(W));
    W.golden_mode = golden;
    if (golden) {
        static char outbuf[160];
        char scene[64] = "clear";
        const char *s = emscripten_run_script_string(
            "(function(){ try { return new URLSearchParams(location.search).get('scene') || 'clear'; } catch (e) { return 'clear'; } })()");
        if (s && !strchr(s, '/') && !strstr(s, "..")) {
            snprintf(scene, sizeof(scene), "%s", s);
        }
        const char *r = emscripten_run_script_string(
            "(function(){ try { return new URLSearchParams(location.search).get('root') || ''; } catch (e) { return ''; } })()");
        if (r && r[0] && !strstr(r, "..")) {
            snprintf(W.root, sizeof(W.root), "%s", r);
        } else {
            snprintf(W.root, sizeof(W.root), "/goldens/%s", scene);
        }
        snprintf(outbuf, sizeof(outbuf), "/captures/%s.png", scene);
        W.capture.frame = 2;
        W.capture.output = outbuf;
    } else {
        if (argc >= 2 && strcmp(argv[1], "--repl") == 0) {
            /* no stdin console on the web build (F6d) */
            W.repl_requested = 1;
            W.in_error = 1;
            fprintf(stderr, "player: repl mode is unavailable on the web build\n");
            fflush(stderr);
        } else if (argc >= 2) {
            if (argv[1][0] == '/') {
                snprintf(W.root, sizeof(W.root), "%s", argv[1]);
            } else {
                char cwd[EFX_WEB_ROOT_MAX];
                if (getcwd(cwd, sizeof(cwd))) {
                    snprintf(W.root, sizeof(W.root), "%s/%s", cwd, argv[1]);
                } else {
                    snprintf(W.root, sizeof(W.root), "%s", argv[1]);
                }
            }
        } else {
            snprintf(W.root, sizeof(W.root), "%s", "/examples/browser");
        }
        if (argc > 2) {
            set_args(argv + 2, argc - 2);
        }
    }
    W.dom = efx_web_has_dom_js();
    /* F14: with a DOM (real browser) initialize audio before the script runs,
       so load-time music uses the real device rate; under the Node harness
       (no DOM) the core lazy-inits device-free, matching the desktop --script
       runtime for the cross-runtime comparison */
    if (W.dom) {
        efx_audio_backend_init();
    }
    web_open_root();
    return 0;
}

EMSCRIPTEN_KEEPALIVE void efx_web_start_loop(void) {
    if (!W.dom) {
        return;
    }
    efx_platform_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.capture = W.capture;
    efx_frame_hooks hooks;
    hooks.ud = NULL;
    hooks.on_frame = web_frame;
    efx_platform_run(&desc, hooks);
}

/* ================================================= F12 physics bridge */

static efx_physics_world *web_physics(void) {
    if (!W.physics) {
        W.physics = efx_physics_world_new();
    }
    return W.physics;
}

static void web_shape_from(int type, double r, double hx, double hy, double hz,
                           double height, efx_shape *out) {
    if (type == 0) {
        *out = efx_shape_sphere((float)r);
    } else if (type == 1) {
        *out = efx_shape_box(efx_v3((float)hx, (float)hy, (float)hz));
    } else if (type == 2) {
        *out = efx_shape_capsule((float)r, (float)height);
    } else {
        *out = efx_shape_sphere(0);
        out->type = EFX_PHYS_SHAPE_MESH;
    }
}

static efx_phys_mesh *web_temp_mesh(double mesh_handle) {
    uint64_t h = (uint64_t)mesh_handle;
    int verts = 0, indices = 0;
    if (!efx_render_mesh_geometry_count(h, &verts, &indices) || verts <= 0 ||
        indices < 3 || indices % 3 != 0) {
        return NULL;
    }
    float *p = malloc((size_t)verts * 3 * sizeof(float));
    uint32_t *idx = malloc((size_t)indices * sizeof(uint32_t));
    if (!p || !idx) {
        free(p);
        free(idx);
        return NULL;
    }
    efx_render_mesh_geometry(h, p, idx);
    efx_phys_mesh *m = efx_phys_mesh_create(p, verts, idx, indices / 3);
    free(p);
    free(idx);
    return m;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_set_gravity(double x, double y,
                                                         double z) {
    efx_physics_set_gravity(web_physics(), efx_v3((float)x, (float)y, (float)z));
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_physics_gravity(int i) {
    efx_vec3 g = efx_physics_gravity(web_physics());
    return i == 0 ? g.x : (i == 1 ? g.y : g.z);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_set_iterations(int n) {
    efx_physics_set_iterations(web_physics(), n);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_iterations(void) {
    return efx_physics_iterations(web_physics());
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_step(double dt) {
    efx_physics_step(web_physics(), (float)dt);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_clear(void) {
    if (W.physics) efx_physics_clear(W.physics);
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_physics_create_body(
    int dynamic, int sensor, int shape_type, double radius, double hx,
    double hy, double hz, double height, double px, double py, double pz,
    double mass, double friction, double restitution, double layer,
    double mask) {
    efx_body_desc d;
    memset(&d, 0, sizeof(d));
    d.dynamic = dynamic;
    d.sensor = sensor;
    web_shape_from(shape_type, radius, hx, hy, hz, height, &d.shape);
    d.position = efx_v3((float)px, (float)py, (float)pz);
    d.mass = (float)mass;
    d.friction = (float)friction;
    d.restitution = (float)restitution;
    d.layer = (uint32_t)layer;
    d.mask = (uint32_t)mask;
    return (double)efx_physics_create_body(web_physics(), &d);
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_physics_create_static_mesh(
    double mesh_handle, double px, double py, double pz, int sensor,
    double friction, double restitution, double layer, double mask) {
    uint64_t h = (uint64_t)mesh_handle;
    int verts = 0, indices = 0;
    if (!efx_render_mesh_geometry_count(h, &verts, &indices) || verts <= 0 ||
        indices < 3 || indices % 3 != 0) {
        return 0;
    }
    float *p = malloc((size_t)verts * 3 * sizeof(float));
    uint32_t *idx = malloc((size_t)indices * sizeof(uint32_t));
    if (!p || !idx) {
        free(p);
        free(idx);
        return 0;
    }
    efx_render_mesh_geometry(h, p, idx);
    efx_static_mesh_desc d;
    memset(&d, 0, sizeof(d));
    d.position = efx_v3((float)px, (float)py, (float)pz);
    d.sensor = sensor;
    d.friction = (float)friction;
    d.restitution = (float)restitution;
    d.layer = (uint32_t)layer;
    d.mask = (uint32_t)mask;
    efx_phys_body b =
        efx_physics_create_static_mesh(web_physics(), p, verts, idx,
                                       indices / 3, &d);
    free(p);
    free(idx);
    return (double)b;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_body_alive(double h) {
    return efx_physics_body_alive(web_physics(), (efx_phys_body)h);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_body_position(double h,
                                                           float *out) {
    efx_physics_body_position(web_physics(), (efx_phys_body)h,
                              (efx_vec3 *)out);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_body_get_velocity(double h,
                                                               float *out) {
    efx_physics_body_velocity(web_physics(), (efx_phys_body)h,
                              (efx_vec3 *)out);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_body_set_velocity(
    double h, double x, double y, double z) {
    efx_physics_body_set_velocity(web_physics(), (efx_phys_body)h,
                                  efx_v3((float)x, (float)y, (float)z));
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_body_apply_impulse(
    double h, double x, double y, double z) {
    return efx_physics_body_apply_impulse(web_physics(), (efx_phys_body)h,
                                          efx_v3((float)x, (float)y, (float)z));
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_body_apply_force(
    double h, double x, double y, double z) {
    return efx_physics_body_apply_force(web_physics(), (efx_phys_body)h,
                                        efx_v3((float)x, (float)y, (float)z));
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_body_contact_count(double h) {
    return efx_physics_body_contact_count(web_physics(), (efx_phys_body)h);
}

/* out: [0..2] normal, [3..5] point, [6] depth, [7] impulse, [8] other body,
 * [9] other character, [10] sensor */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_body_contact(double h, int i,
                                                         double *out) {
    efx_contact_info ci;
    if (!efx_physics_body_contact(web_physics(), (efx_phys_body)h, i, &ci)) {
        return 0;
    }
    out[0] = ci.normal.x;
    out[1] = ci.normal.y;
    out[2] = ci.normal.z;
    out[3] = ci.point.x;
    out[4] = ci.point.y;
    out[5] = ci.point.z;
    out[6] = ci.depth;
    out[7] = ci.impulse;
    out[8] = (double)ci.body;
    out[9] = (double)ci.character;
    out[10] = ci.sensor;
    return 1;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_body_destroy(double h) {
    efx_physics_destroy_body(web_physics(), (efx_phys_body)h);
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_physics_create_character(
    double radius, double height, double px, double py, double pz, double ux,
    double uy, double uz, double floor_max_angle, double floor_snap_length,
    double step_height, double safe_margin, int max_slides, double layer,
    double mask) {
    efx_character_desc d;
    memset(&d, 0, sizeof(d));
    d.radius = (float)radius;
    d.height = (float)height;
    d.position = efx_v3((float)px, (float)py, (float)pz);
    d.up = efx_v3((float)ux, (float)uy, (float)uz);
    d.floor_max_angle = (float)floor_max_angle;
    d.floor_snap_length = (float)floor_snap_length;
    d.step_height = (float)step_height;
    d.safe_margin = (float)safe_margin;
    d.max_slides = max_slides;
    d.layer = (uint32_t)layer;
    d.mask = (uint32_t)mask;
    return (double)efx_physics_create_character(web_physics(), &d);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_character_alive(double h) {
    return efx_physics_character_alive(web_physics(), (efx_phys_character)h);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_character_position(double h,
                                                                float *out) {
    efx_physics_character_position(web_physics(), (efx_phys_character)h,
                                   (efx_vec3 *)out);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_character_get_velocity(
    double h, float *out) {
    efx_physics_character_velocity(web_physics(), (efx_phys_character)h,
                                   (efx_vec3 *)out);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_character_set_velocity(
    double h, double x, double y, double z) {
    efx_physics_character_set_velocity(web_physics(), (efx_phys_character)h,
                                       efx_v3((float)x, (float)y, (float)z));
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_character_on_floor(double h) {
    return efx_physics_character_on_floor(web_physics(),
                                          (efx_phys_character)h);
}

/* out: [0..2] position, [3] onFloor, [4] onWall, [5] onCeiling,
 * [6..8] floorNormal, [9] collision count */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_character_move(
    double h, double mx, double my, double mz, double *out) {
    efx_move_result mr;
    if (!efx_physics_character_move_and_slide(
            web_physics(), (efx_phys_character)h,
            efx_v3((float)mx, (float)my, (float)mz), &mr)) {
        return 0;
    }
    out[0] = mr.position.x;
    out[1] = mr.position.y;
    out[2] = mr.position.z;
    out[3] = mr.on_floor;
    out[4] = mr.on_wall;
    out[5] = mr.on_ceiling;
    out[6] = mr.floor_normal.x;
    out[7] = mr.floor_normal.y;
    out[8] = mr.floor_normal.z;
    out[9] = mr.collision_count;
    return 1;
}

/* out: [0] body, [1..3] normal, [4..6] point */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_move_collision(double h, int i,
                                                           double *out) {
    efx_move_collision mc;
    if (!efx_physics_move_collision(web_physics(), (efx_phys_character)h, i,
                                    &mc)) {
        return 0;
    }
    out[0] = (double)mc.body;
    out[1] = mc.normal.x;
    out[2] = mc.normal.y;
    out[3] = mc.normal.z;
    out[4] = mc.point.x;
    out[5] = mc.point.y;
    out[6] = mc.point.z;
    return 1;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_character_destroy(double h) {
    efx_physics_destroy_character(web_physics(), (efx_phys_character)h);
}

/* raycast: out is a flat array of stride 10 per hit:
 * [point3, normal3, distance, body, character, sensor]; returns the count */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_raycast(
    double ox, double oy, double oz, double dx, double dy, double dz,
    double max_distance, double mask, int sensors, int all, double *out) {
    efx_ray_hit hits[256];
    int cap = all ? 256 : 1;
    int n = efx_physics_raycast(web_physics(), efx_v3((float)ox, (float)oy,
                                                      (float)oz),
                                efx_v3((float)dx, (float)dy, (float)dz),
                                (float)max_distance, (uint32_t)mask, sensors,
                                all, hits, cap);
    if (out) {
        for (int i = 0; i < n; i++) {
            double *o = &out[i * 10];
            o[0] = hits[i].point.x;
            o[1] = hits[i].point.y;
            o[2] = hits[i].point.z;
            o[3] = hits[i].normal.x;
            o[4] = hits[i].normal.y;
            o[5] = hits[i].normal.z;
            o[6] = hits[i].distance;
            o[7] = (double)hits[i].body;
            o[8] = (double)hits[i].character;
            o[9] = hits[i].sensor;
        }
    }
    return n;
}

/* overlap: out is a flat array of stride 3 per hit: [body, character, sensor] */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_overlap(
    int shape_type, double radius, double hx, double hy, double hz,
    double height, double mesh_handle, double px, double py, double pz,
    double mask, double *out) {
    efx_shape shape;
    memset(&shape, 0, sizeof(shape));
    web_shape_from(shape_type, radius, hx, hy, hz, height, &shape);
    efx_phys_mesh *temp = NULL;
    if (shape.type == EFX_PHYS_SHAPE_MESH) {
        temp = web_temp_mesh(mesh_handle);
        if (!temp) return 0;
        shape.mesh = temp;
    }
    int count = efx_physics_overlap(web_physics(), &shape,
                                    efx_v3((float)px, (float)py, (float)pz),
                                    (uint32_t)mask, 1, NULL, 0);
    if (out && count > 0) {
        efx_overlap_hit *hits = calloc((size_t)count, sizeof(*hits));
        if (hits) {
            int n = efx_physics_overlap(web_physics(), &shape,
                                        efx_v3((float)px, (float)py, (float)pz),
                                        (uint32_t)mask, 1, hits, count);
            for (int i = 0; i < n; i++) {
                out[i * 3] = (double)hits[i].body;
                out[i * 3 + 1] = (double)hits[i].character;
                out[i * 3 + 2] = hits[i].sensor;
            }
            free(hits);
        } else {
            count = 0;
        }
    }
    efx_phys_mesh_free(temp);
    return count;
}

/* shapeCast: out is [point3, normal3, fraction, body, character, sensor] */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_shape_cast(
    int shape_type, double radius, double hx, double hy, double hz,
    double height, double mesh_handle, double fx, double fy, double fz,
    double mx, double my, double mz, double mask, int sensors, double *out) {
    efx_shape shape;
    memset(&shape, 0, sizeof(shape));
    web_shape_from(shape_type, radius, hx, hy, hz, height, &shape);
    efx_phys_mesh *temp = NULL;
    if (shape.type == EFX_PHYS_SHAPE_MESH) {
        temp = web_temp_mesh(mesh_handle);
        if (!temp) return 0;
        shape.mesh = temp;
    }
    efx_shape_hit hit;
    int rc = efx_physics_shape_cast(
        web_physics(), &shape, efx_v3((float)fx, (float)fy, (float)fz),
        efx_v3((float)mx, (float)my, (float)mz), (uint32_t)mask, sensors,
        &hit);
    efx_phys_mesh_free(temp);
    if (rc && out) {
        out[0] = hit.point.x;
        out[1] = hit.point.y;
        out[2] = hit.point.z;
        out[3] = hit.normal.x;
        out[4] = hit.normal.y;
        out[5] = hit.normal.z;
        out[6] = hit.fraction;
        out[7] = (double)hit.body;
        out[8] = (double)hit.character;
        out[9] = hit.sensor;
    }
    return rc;
}
