#include "bridge_internal.h"

/* ------------------------------------------------- F5a (render targets) */

EMSCRIPTEN_KEEPALIVE double efx_bridge_target_create(int w, int h) {
    return (double)efx_render_target_create(w, h);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_target_destroy(double handle) {
    efx_render_target_destroy((uint64_t)handle);
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

