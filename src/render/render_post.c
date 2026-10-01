#include "render_internal.h"

render_post_state POST = {.scale = 1.0f, .filter = EFX_FILTER_LINEAR};

void post_reset(void) {
    POST.count = 0;
    POST.scale = 1.0f;
    POST.filter = EFX_FILTER_LINEAR;
}

/* full teardown: the scene/temp target slots are freed with R.targets */
void post_clear(void) {
    memset(&POST, 0, sizeof(POST));
    POST.scale = 1.0f;
    POST.filter = EFX_FILTER_LINEAR;
}
int post_scene_size(int surface) {
    if (surface <= 0) {
        return surface;
    }
    int scaled = (int)ceilf((float)surface * POST.scale);
    return scaled < 1 ? 1 : scaled;
}

int efx_render_post_active(void) {
    return POST.count > 0 || POST.scale != 1.0f;
}

/* registered-effect bounds check (the native half of the validation matrix;
 * the binding owns JS field/type checks). Returns EFX_POST_OK / an error. */
static int post_entry_valid(const efx_post_entry *e) {
    if (!e) {
        return EFX_POST_ERR_UNKNOWN;
    }
    if (!(e->mix >= 0.0f && e->mix <= 1.0f) || !isfinite(e->mix)) {
        return EFX_POST_ERR_RANGE;
    }
    switch (e->effect) {
    case EFX_POST_COLOR_FILTER: {
        const float *f = &e->u.color_filter.brightness;
        for (int i = 0; i < 3; i++) {
            if (!isfinite(f[i]) || f[i] < 0.0f) {
                return EFX_POST_ERR_RANGE;
            }
        }
        for (int i = 0; i < 4; i++) {
            float t = e->u.color_filter.tint[i];
            if (!isfinite(t) || t < 0.0f || t > 1.0f) {
                return EFX_POST_ERR_RANGE;
            }
        }
        return EFX_POST_OK;
    }
    case EFX_POST_BLUR:
        if (!isfinite(e->u.blur.radius) || e->u.blur.radius <= 0.0f ||
            e->u.blur.radius > 64.0f) {
            return EFX_POST_ERR_RANGE;
        }
        return EFX_POST_OK;
    case EFX_POST_BLOOM:
        if (!isfinite(e->u.bloom.threshold) || e->u.bloom.threshold < 0.0f ||
            e->u.bloom.threshold > 1.0f ||
            !isfinite(e->u.bloom.strength) || e->u.bloom.strength < 0.0f ||
            e->u.bloom.strength > 1.0f) {
            return EFX_POST_ERR_RANGE;
        }
        return EFX_POST_OK;
    default:
        return EFX_POST_ERR_UNKNOWN;
    }
}

int efx_render_set_post_effects(const efx_post_entry *entries, int count) {
    ensure_state();
    if (count < 0 || count > EFX_POST_MAX_ENTRIES) {
        return EFX_POST_ERR_COUNT;
    }
    if (count > 0 && !entries) {
        return EFX_POST_ERR_COUNT;
    }
    for (int i = 0; i < count; i++) {
        int rc = post_entry_valid(&entries[i]);
        if (rc != EFX_POST_OK) {
            return rc; /* previous chain remains (atomic) */
        }
    }
    for (int i = 0; i < count; i++) {
        POST.entries[i] = entries[i]; /* snapshot at call time */
    }
    POST.count = count;
    return EFX_POST_OK;
}

void efx_render_post_effects(efx_post_entry *out, int *count) {
    if (out) {
        for (int i = 0; i < POST.count; i++) {
            out[i] = POST.entries[i];
        }
    }
    if (count) {
        *count = POST.count;
    }
}

int efx_render_set_render_scale(float scale, int filter) {
    ensure_state();
    if (!isfinite(scale) || scale <= 0.0f || scale > 2.0f) {
        return EFX_POST_ERR_RANGE;
    }
    if (filter != EFX_FILTER_NEAREST && filter != EFX_FILTER_LINEAR) {
        return EFX_POST_ERR_FILTER;
    }
    POST.scale = scale;
    POST.filter = filter;
    return EFX_POST_OK;
}

void efx_render_render_scale(float *out_scale, int *out_filter) {
    if (out_scale) {
        *out_scale = POST.scale;
    }
    if (out_filter) {
        *out_filter = POST.filter;
    }
}

/* get-or-recreate an engine-owned target when its size changed */
static uint64_t ensure_post_target(uint64_t *handle, int w, int h) {
    if (*handle) {
        int cw = 0, ch = 0;
        efx_render_target_size(*handle, &cw, &ch);
        if (cw == w && ch == h) {
            return *handle;
        }
        efx_render_target_destroy(*handle); /* deferred to frame end */
        *handle = 0;
    }
    *handle = efx_render_target_create(w, h);
    return *handle;
}

uint64_t efx_render_post_scene_target(int w, int h) {
    if (w < 1 || h < 1) {
        return 0;
    }
    uint64_t t = ensure_post_target(&POST.scene, w, h);
    POST.scene_w = w;
    POST.scene_h = h;
    return t;
}

uint64_t efx_render_post_temp_target(int slot, int w, int h) {
    if (slot < 0 || slot > 3 || w < 1 || h < 1) {
        return 0;
    }
    uint64_t t = ensure_post_target(&POST.temps[slot], w, h);
    POST.temp_w[slot] = w;
    POST.temp_h[slot] = h;
    return t;
}

uint64_t efx_render_post_half_target(int slot, int w, int h) {
    if (slot < 0 || slot > 1 || w < 1 || h < 1) {
        return 0;
    }
    uint64_t t = ensure_post_target(&POST.half[slot], w, h);
    POST.half_w[slot] = w;
    POST.half_h[slot] = h;
    return t;
}

uint64_t efx_render_post_scene_handle(void) {
    return POST.scene;
}
