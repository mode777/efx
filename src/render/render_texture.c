#include "render_internal.h"


/* initialise the fields shared by the sink-less (CPU-only) and live
 * texture paths. The generation bump and the append-vs-reuse choice stay with
 * each caller (the queued path always appends; the live path scans for a free
 * slot), so observable handle sequencing is unchanged. */
static void tex_slot_init(tex_slot *s, int w, int h, int wrap, int filter,
                          int mipmaps, void *native) {
    s->used = 1;
    s->alive = 1;
    s->permanent = 0;
    s->bind_refs = 0;
    s->release_pending = 0;
    s->w = w;
    s->h = h;
    s->wrap = wrap;
    s->filter = filter;
    s->mipmaps = mipmaps;
    s->native = native;
}

uint64_t efx_render_texture_create(int w, int h, const uint8_t *rgba, int wrap,
                                   int filter, int mipmaps) {
    if (wrap < EFX_TEX_WRAP_REPEAT || wrap > EFX_TEX_WRAP_MIRROR) {
        wrap = EFX_TEX_WRAP_REPEAT;
    }
    if (filter != EFX_FILTER_NEAREST && filter != EFX_FILTER_LINEAR) {
        filter = EFX_FILTER_LINEAR;
    }
    mipmaps = mipmaps ? 1 : 0;
    /* with a sink, upload now; without one, create a CPU-only slot: size
       queries and recorded draws work, nothing is uploaded or rendered
       (ADR 0052) */
    void *native = NULL;
    if (R.sink && R.sink->create_texture) {
        native = R.sink->create_texture(R.sink->ud, w, h, rgba, wrap, filter,
                                        mipmaps);
        if (!native) {
            return 0;
        }
    }
    /* find a free slot or grow */
    tex_slot *s = NULL;
    for (int i = 0; i < R.slot_count; i++) {
        if (!R.slots[i].used) {
            s = &R.slots[i];
            break;
        }
    }
    if (!s) {
        if (R.slot_count >= R.slot_cap) {
            if (!pool_grow((void **)&R.slots, &R.slot_cap, R.slot_count + 1,
                           sizeof(tex_slot), 64)) {
                if (native) {
                    R.sink->destroy_texture(R.sink->ud, native);
                }
                return 0;
            }
        }
        s = &R.slots[R.slot_count++];
        s->gen = 0;
    }
    s->gen++;
    tex_slot_init(s, w, h, wrap, filter, mipmaps, native);
    uint32_t idx = (uint32_t)(s - R.slots) + 1;
    return ((uint64_t)s->gen << 32) | (uint64_t)idx;
}

/* schedule the native release of a texture slot at frame end (records may
 * reference the texture until playback finishes — js-api lifecycle rules) */
static void schedule_texture_native_release(tex_slot *s) {
    if (!R.sink || !R.sink->destroy_texture || !s->native) {
        return;
    }
    if (R.deferred_tex_count >= R.deferred_tex_cap) {
        if (!pool_grow((void **)&R.deferred_tex, &R.deferred_tex_cap,
                       R.deferred_tex_count + 1, sizeof(int), 16)) {
            return; /* best effort; slot stays until shutdown */
        }
    }
    R.deferred_tex[R.deferred_tex_count++] = (int)(s - R.slots);
}

/* finish a release once no material references the slot (ADR 0027) */
static void finalize_texture_release(tex_slot *s) {
    schedule_texture_native_release(s);
}

/* F4b map retention (ADR 0027): material bindings keep their maps alive.
 * F5a: maps may reference a Texture or a RenderTarget — the retain/ref
 * helpers dispatch on whichever registry holds the handle. */
void texture_bind_retain(uint64_t h) {
    tex_slot *ts = slot_get(h);
    if (ts) {
        ts->bind_refs++;
        return;
    }
    rt_slot *t = rt_get(h);
    if (t) {
        t->bind_refs++;
    }
}

void texture_bind_release(uint64_t h) {
    tex_slot *ts = slot_get(h);
    if (ts) {
        if (ts->bind_refs > 0) {
            ts->bind_refs--;
        }
        if (ts->bind_refs == 0 && ts->release_pending) {
            ts->release_pending = 0;
            finalize_texture_release(ts);
        }
        return;
    }
    rt_slot *t = rt_get(h);
    if (t && t->bind_refs > 0) {
        t->bind_refs--;
        if (t->bind_refs == 0 && t->release_pending) {
            t->release_pending = 0;
            finalize_target_release(t);
        }
    }
}

static int texture_release(uint64_t h, tex_slot **out_slot) {
    tex_slot *s = slot_get(h);
    if (!s) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (s->permanent) {
        return EFX_RENDER_ERR_PERMANENT;
    }
    if (!s->alive) {
        return EFX_RENDER_OK; /* destroy() is idempotent */
    }
    s->alive = 0;
    if (s->bind_refs > 0) {
        /* a bound map keeps the texture alive until the binding is released */
        s->release_pending = 1;
        if (out_slot) {
            *out_slot = s;
        }
        return EFX_RENDER_OK;
    }
    finalize_texture_release(s);
    if (out_slot) {
        *out_slot = s;
    }
    return EFX_RENDER_OK;
}

int efx_render_texture_destroy(uint64_t h) {
    return texture_release(h, NULL);
}

int efx_render_texture_alive(uint64_t h) {
    tex_slot *s = slot_get(h);
    return s && s->alive;
}

int efx_render_texture_ref_count(uint64_t h) {
    tex_slot *s = slot_get(h);
    return s ? s->bind_refs : -1;
}

/* F4b: the five per-channel map handles retained by a material snapshot,
 * in the order the desktop/web bindings lay them out */
static const size_t MAP_OFFSETS[] = {
    offsetof(efx_material, ambient_map),
    offsetof(efx_material, diffuse_map),
    offsetof(efx_material, specular_map),
    offsetof(efx_material, emissive_map),
    offsetof(efx_material, alpha_mask),
};
#define MAP_OFFSETS_COUNT (sizeof(MAP_OFFSETS) / sizeof(MAP_OFFSETS[0]))

/* F4b: retain/release every map referenced by a material snapshot (0 = none) */
void material_retain_maps(const efx_material *m) {
    if (!m) {
        return;
    }
    for (size_t i = 0; i < MAP_OFFSETS_COUNT; i++) {
        uint64_t h;
        memcpy(&h, (const uint8_t *)m + MAP_OFFSETS[i], sizeof(h));
        texture_bind_retain(h);
    }
}

void material_release_maps(const efx_material *m) {
    if (!m) {
        return;
    }
    for (size_t i = 0; i < MAP_OFFSETS_COUNT; i++) {
        uint64_t h;
        memcpy(&h, (const uint8_t *)m + MAP_OFFSETS[i], sizeof(h));
        texture_bind_release(h);
    }
}

void efx_render_texture_size(uint64_t handle, int *out_w, int *out_h) {
    tex_slot *s = slot_get(handle);
    if (s) {
        if (out_w) *out_w = s->w;
        if (out_h) *out_h = s->h;
    }
}

void efx_render_texture_sampler(uint64_t handle, int *out_wrap,
                                int *out_filter, int *out_mipmaps) {
    tex_slot *s = slot_get(handle);
    if (out_wrap) *out_wrap = s ? s->wrap : -1;
    if (out_filter) *out_filter = s ? s->filter : -1;
    if (out_mipmaps) *out_mipmaps = s ? s->mipmaps : -1;
}

void *efx_render_texture_native(uint64_t h) {
    tex_slot *s = slot_get(h);
    return s ? s->native : NULL;
}

uint64_t efx_render_white_texture(void) {
    if (R.white_handle) {
        return R.white_handle;
    }
    static const uint8_t white[4] = {255, 255, 255, 255};
    uint64_t h = efx_render_texture_create(1, 1, white, EFX_TEX_WRAP_REPEAT,
                                           EFX_FILTER_LINEAR, 0);
    if (h) {
        tex_slot *s = slot_get(h);
        s->permanent = 1;
        R.white_handle = h;
    }
    return h;
}
