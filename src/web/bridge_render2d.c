#include "bridge_internal.h"


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
EMSCRIPTEN_KEEPALIVE int efx_bridge_imagedata_width(int id) {
    img_slot *s = img_get(id);
    return s ? s->w : 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_imagedata_height(int id) {
    img_slot *s = img_get(id);
    return s ? s->h : 0;
}
