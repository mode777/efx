#include "render_internal.h"


/* ------------------------------------------------------ render targets */

uint64_t efx_render_target_create(int w, int h) {
    if (w <= 0 || h <= 0 || w > EFX_RENDER_MAX_TARGET_SIZE ||
        h > EFX_RENDER_MAX_TARGET_SIZE) {
        return 0;
    }
    rt_slot *t = NULL;
    for (int i = 0; i < R.target_count; i++) {
        if (!R.targets[i].used) {
            t = &R.targets[i];
            break;
        }
    }
    if (!t) {
        if (R.target_count >= R.target_cap) {
            if (!pool_grow((void **)&R.targets, &R.target_cap, R.target_count + 1,
                           sizeof(rt_slot), 16)) {
                return 0;
            }
        }
        t = &R.targets[R.target_count++];
        t->gen = 0;
    }
    void *native = NULL;
    if (R.sink && R.sink->create_render_target) {
        native = R.sink->create_render_target(R.sink->ud, w, h);
        if (!native) {
            return 0;
        }
    }
    t->used = 1;
    t->alive = 1;
    t->bind_refs = 0;
    t->release_pending = 0;
    t->gen++;
    t->w = w;
    t->h = h;
    t->native = native;
    uint32_t idx = (uint32_t)(t - R.targets) + 1;
    return ((uint64_t)t->gen << 32) | 0x10000000ull | (uint64_t)idx;
}

static void schedule_target_native_release(rt_slot *t) {
    if (!R.sink || !R.sink->destroy_render_target || !t->native) {
        return;
    }
    if (R.deferred_rt_count >= R.deferred_rt_cap) {
        if (!pool_grow((void **)&R.deferred_rt, &R.deferred_rt_cap,
                       R.deferred_rt_count + 1, sizeof(int), 16)) {
            return; /* best effort; slot stays until shutdown */
        }
    }
    R.deferred_rt[R.deferred_rt_count++] = (int)(t - R.targets);
}

void finalize_target_release(rt_slot *t) {
    schedule_target_native_release(t);
}

int efx_render_target_destroy(uint64_t h) {
    rt_slot *t = rt_get(h);
    if (!t) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (!t->alive) {
        return EFX_RENDER_OK; /* destroy() is idempotent */
    }
    t->alive = 0;
    if (t->bind_refs > 0) {
        /* a bound map keeps the target alive until the binding is released */
        t->release_pending = 1;
        return EFX_RENDER_OK;
    }
    finalize_target_release(t);
    return EFX_RENDER_OK;
}

int efx_render_target_alive(uint64_t h) {
    rt_slot *t = rt_get(h);
    return t && t->alive;
}

int efx_render_target_ref_count(uint64_t h) {
    rt_slot *t = rt_get(h);
    return t ? t->bind_refs : -1;
}

void efx_render_target_size(uint64_t h, int *out_w, int *out_h) {
    rt_slot *t = rt_get(h);
    if (t) {
        if (out_w) *out_w = t->w;
        if (out_h) *out_h = t->h;
    }
}

void *efx_render_target_native(uint64_t h) {
    rt_slot *t = rt_get(h);
    return t ? t->native : NULL;
}

/* ------------------------------------------------- render redirection */

uint64_t efx_render_active_target(void) {
    return R.active_target;
}

int efx_render_begin_target(uint64_t h) {
    ensure_state();
    rt_slot *t = rt_get(h);
    if (!t || !t->alive) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (R.active_target) {
        return EFX_RENDER_ERR_NESTED;
    }
    efx_record rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = EFX_RECORD_BEGIN_TARGET;
    rec.target = h;
    rec.u.begin_target.target = h;
    efx_render_clear_color(rec.u.begin_target.clear); /* snapshot (design D3) */
    rec.sort_key = (uint32_t)R.record_count;
    int rc = record_push(rec, sizeof(efx_record));
    if (rc != EFX_RENDER_OK) {
        return rc;
    }
    R.active_target = h;
    return EFX_RENDER_OK;
}

int efx_render_end_target(void) {
    ensure_state();
    if (!R.active_target) {
        return EFX_RENDER_ERR_STATE;
    }
    efx_record rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = EFX_RECORD_END_TARGET;
    rec.target = R.active_target;
    rec.u.begin_target.target = R.active_target;
    rec.sort_key = (uint32_t)R.record_count;
    int rc = record_push(rec, sizeof(efx_record));
    if (rc != EFX_RENDER_OK) {
        return rc;
    }
    R.active_target = 0;
    return EFX_RENDER_OK;
}

/* --------------------------------------------------- sample coercion */

int efx_render_sample_alive(uint64_t h) {
    if (!h) {
        return 0;
    }
    tex_slot *ts = slot_get(h);
    if (ts) {
        return ts->alive;
    }
    rt_slot *t = rt_get(h);
    return t && t->alive;
}

void efx_render_sample_size(uint64_t h, int *out_w, int *out_h) {
    tex_slot *ts = slot_get(h);
    if (ts) {
        if (out_w) *out_w = ts->w;
        if (out_h) *out_h = ts->h;
        return;
    }
    efx_render_target_size(h, out_w, out_h);
}

void *efx_render_sample_native(uint64_t h) {
    tex_slot *ts = slot_get(h);
    if (ts) {
        return ts->native;
    }
    return efx_render_target_native(h);
}

void efx_render_surface_size(int *out_w, int *out_h) {
    ensure_state();
    if (R.active_target) {
        efx_render_target_size(R.active_target, out_w, out_h);
        return;
    }
    if (efx_render_post_active()) {
        /* F5b: the implicit scene target is the default surface; the camera
           frame and 3D aspect map onto it, and it maps onto the output */
        if (out_w) *out_w = post_scene_size(R.viewport_w);
        if (out_h) *out_h = post_scene_size(R.viewport_h);
        return;
    }
    if (out_w) *out_w = R.viewport_w;
    if (out_h) *out_h = R.viewport_h;
}
