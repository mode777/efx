#include "render_internal.h"

static int ps_config_valid(const efx_particle_config *c) {
    if (!c || !c->texture) return 0;
    if (c->max < 1 || c->max > EFX_PARTICLES_MAX) return 0;
    if (c->space != EFX_SPACE_WORLD && c->space != EFX_SPACE_SCREEN) return 0;
    if (c->facing < EFX_FACING_VIEW || c->facing > EFX_FACING_PLANE) return 0;
    if (c->space == EFX_SPACE_SCREEN && c->facing != EFX_FACING_VIEW) return 0;
    if (c->blend < EFX_BLEND_INHERIT || c->blend > EFX_BLEND_SUBTRACTIVE) return 0;
    if (!(c->life_min > 0.0f) || !(c->life_max >= c->life_min) ||
        !isfinite(c->life_min) || !isfinite(c->life_max)) {
        return 0;
    }
    if (!(c->emission_rate >= 0.0f) || !isfinite(c->emission_rate)) return 0;
    if (!(c->emitter_lifetime == -1.0f || c->emitter_lifetime > 0.0f)) return 0;
    if (!(c->spread >= 0.0f) || !isfinite(c->spread)) return 0;
    if (!(c->speed_min >= 0.0f) || !(c->speed_max >= c->speed_min)) return 0;
    for (int i = 0; i < 3; i++) {
        if (!isfinite(c->position[i]) || !isfinite(c->direction[i]) ||
            !isfinite(c->gravity[i]) || !isfinite(c->lin_acc_min[i]) ||
            !isfinite(c->lin_acc_max[i]) || !isfinite(c->normal[i]) ||
            !isfinite(c->shape_size[i])) {
            return 0;
        }
    }
    if (!(c->damping_min >= 0.0f) || !(c->damping_max >= c->damping_min)) {
        return 0;
    }
    if (c->size_count < 1 || c->size_count > 8) return 0;
    for (int i = 0; i < c->size_count; i++) {
        if (!(c->sizes[i] > 0.0f) || !isfinite(c->sizes[i])) return 0;
    }
    if (!(c->size_variation >= 0.0f && c->size_variation <= 1.0f)) return 0;
    if (c->color_count < 1 || c->color_count > 8) return 0;
    for (int i = 0; i < c->color_count; i++) {
        for (int k = 0; k < 4; k++) {
            if (!(c->colors[i][k] >= 0.0f && c->colors[i][k] <= 1.0f)) return 0;
        }
    }
    if (!isfinite(c->rotation_min) || !isfinite(c->rotation_max)) return 0;
    if (!isfinite(c->spin_start) || !isfinite(c->spin_end)) return 0;
    if (!(c->spin_variation >= 0.0f && c->spin_variation <= 1.0f)) return 0;
    if (c->shape < EFX_SHAPE_POINT || c->shape > EFX_SHAPE_DISC) return 0;
    if (c->quad_count < 0 || c->quad_count > 64) return 0;
    if (c->insert_mode < EFX_INSERT_TOP || c->insert_mode > EFX_INSERT_RANDOM)
        return 0;
    if (!(c->speed_scale > 0.0f) || !isfinite(c->speed_scale)) return 0;
    return 1;
}

static void ps_vnorm3(float out[3], const float v[3]) {
    float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 1e-8f) {
        out[0] = v[0] / len;
        out[1] = v[1] / len;
        out[2] = v[2] / len;
    } else {
        out[0] = 0.0f;
        out[1] = 1.0f;
        out[2] = 0.0f;
    }
}

static void ps_cross3(float out[3], const float a[3], const float b[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

/* deterministic xorshift32; every system starts from the same seed so
 * identically configured systems produce identical sequences */
static float ps_randf(ps_slot *p) {
    uint32_t x = p->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    p->rng = x;
    return (float)(x >> 8) * (1.0f / 16777216.0f);
}

static float ps_range(ps_slot *p, float lo, float hi) {
    if (hi <= lo) return lo;
    return lo + (hi - lo) * ps_randf(p);
}

/* Love-style variation for a start/end pair */
static float ps_var(ps_slot *p, float inner, float outer, float var) {
    float low = inner - (outer * 0.5f) * var;
    float high = inner + (outer * 0.5f) * var;
    float r = ps_randf(p);
    return low * (1.0f - r) + high * r;
}

static void ps_spawn(ps_slot *p);

static void ps_insert(ps_slot *p, const efx_particle *q) {
    if (p->count >= p->cfg.max) {
        return; /* drop when full (free capacity) */
    }
    int at = p->count;
    if (p->cfg.insert_mode == EFX_INSERT_BOTTOM) {
        at = 0;
    } else if (p->cfg.insert_mode == EFX_INSERT_RANDOM) {
        at = (int)(ps_randf(p) * (float)(p->count + 1));
        if (at > p->count) at = p->count;
    }
    if (at < p->count) {
        memmove(&p->parts[at + 1], &p->parts[at],
                (size_t)(p->count - at) * sizeof(efx_particle));
    }
    p->parts[at] = *q;
    p->count++;
}

static void ps_spawn(ps_slot *p) {
    if (p->count >= p->cfg.max) {
        return;
    }
    const efx_particle_config *c = &p->cfg;
    efx_particle q;
    memset(&q, 0, sizeof(q));

    /* emission position by shape */
    q.pos[0] = c->position[0];
    q.pos[1] = c->position[1];
    q.pos[2] = c->position[2];
    switch (c->shape) {
    case EFX_SHAPE_BOX:
        for (int i = 0; i < 3; i++) {
            q.pos[i] += ps_range(p, -c->shape_size[i], c->shape_size[i]);
        }
        break;
    case EFX_SHAPE_SPHERE:
    case EFX_SHAPE_SPHERE_SURFACE: {
        float d[3] = {ps_randf(p) * 2 - 1, ps_randf(p) * 2 - 1,
                      ps_randf(p) * 2 - 1};
        ps_vnorm3(d, d);
        float r = c->shape_size[0];
        if (c->shape == EFX_SHAPE_SPHERE) {
            r *= cbrtf(ps_randf(p));
        }
        for (int i = 0; i < 3; i++) {
            q.pos[i] += d[i] * r;
        }
        break;
    }
    case EFX_SHAPE_DISC: {
        /* circle in the plane perpendicular to the emission direction */
        float n[3];
        ps_vnorm3(n, c->direction);
        float t[3] = {0, 1, 0};
        if (fabsf(n[1]) > 0.9f) {
            t[0] = 1; t[1] = 0; t[2] = 0;
        }
        float u[3], v[3];
        ps_cross3(u, n, t);
        ps_vnorm3(u, u);
        ps_cross3(v, n, u);
        float r = c->shape_size[0] * sqrtf(ps_randf(p));
        float a = 6.2831853f * ps_randf(p);
        for (int i = 0; i < 3; i++) {
            q.pos[i] += u[i] * (cosf(a) * r) + v[i] * (sinf(a) * r);
        }
        break;
    }
    default:
        break; /* point */
    }

    /* direction cone */
    float n[3];
    ps_vnorm3(n, c->direction);
    float t[3] = {0, 1, 0};
    if (fabsf(n[1]) > 0.9f) {
        t[0] = 1; t[1] = 0; t[2] = 0;
    }
    float ta[3], tb[3];
    ps_cross3(ta, n, t);
    ps_vnorm3(ta, ta);
    ps_cross3(tb, n, ta);
    float cone = ps_range(p, -c->spread, c->spread) * DEG2RAD;
    float az = 6.2831853f * ps_randf(p);
    float dir[3];
    for (int i = 0; i < 3; i++) {
        dir[i] = cosf(cone) * n[i] +
                 sinf(cone) * (cosf(az) * ta[i] + sinf(az) * tb[i]);
    }
    float speed = ps_range(p, c->speed_min, c->speed_max);
    for (int i = 0; i < 3; i++) {
        q.vel[i] = dir[i] * speed;
        q.origin[i] = q.pos[i];
        q.lin_acc[i] = ps_range(p, c->lin_acc_min[i], c->lin_acc_max[i]);
    }
    q.radial_acc = ps_range(p, c->radial_acc_min, c->radial_acc_max);
    q.tangential_acc =
        ps_range(p, c->tangential_acc_min, c->tangential_acc_max);
    q.damping = ps_range(p, c->damping_min, c->damping_max);
    q.lifetime = ps_range(p, c->life_min, c->life_max);
    q.life = q.lifetime;
    q.rotation = ps_range(p, c->rotation_min, c->rotation_max);
    q.spin_start = ps_var(p, c->spin_start, c->spin_end, c->spin_variation);
    q.spin_end = ps_var(p, c->spin_end, c->spin_start, c->spin_variation);
    q.size_scale = 1.0f - ps_randf(p) * c->size_variation;
    q.quad = 0;

    ps_insert(p, &q);
}

static float ps_lerp_sizes(const efx_particle_config *c, float t) {
    if (c->size_count == 1) return c->sizes[0];
    float s = t * (float)(c->size_count - 1);
    int i = (int)s;
    if (i >= c->size_count - 1) return c->sizes[c->size_count - 1];
    float f = s - (float)i;
    return c->sizes[i] * (1.0f - f) + c->sizes[i + 1] * f;
}

static void ps_lerp_color(const efx_particle_config *c, float t, float out[4]) {
    if (c->color_count == 1) {
        for (int i = 0; i < 4; i++) out[i] = c->colors[0][i];
        return;
    }
    float s = t * (float)(c->color_count - 1);
    int i = (int)s;
    if (i >= c->color_count - 1) i = c->color_count - 2, s = (float)(c->color_count - 1);
    float f = s - (float)i;
    for (int k = 0; k < 4; k++) {
        out[k] = c->colors[i][k] * (1.0f - f) + c->colors[i + 1][k] * f;
    }
}

static void ps_build_views(ps_slot *p) {
    const efx_particle_config *c = &p->cfg;
    for (int i = 0; i < p->count; i++) {
        const efx_particle *q = &p->parts[i];
        efx_particle_view *v = &p->views[i];
        float t = q->lifetime > 0.0f ? 1.0f - q->life / q->lifetime : 1.0f;
        v->pos[0] = q->pos[0];
        v->pos[1] = q->pos[1];
        v->pos[2] = q->pos[2];
        v->size = ps_lerp_sizes(c, t) * q->size_scale;
        ps_lerp_color(c, t, v->color);
        v->angle = q->rotation;
        if (c->relative_rotation) {
            v->angle += atan2f(q->vel[1], q->vel[0]) * (180.0f / 3.14159265f);
        }
        if (c->quad_count > 0) {
            int qi = (int)(t * (float)c->quad_count);
            if (qi >= c->quad_count) qi = c->quad_count - 1;
            if (qi < 0) qi = 0;
            float x = c->quads[qi][0], y = c->quads[qi][1];
            float w = c->quads[qi][2], h = c->quads[qi][3];
            v->uv[0] = p->tw > 0 ? x / p->tw : 0.0f;
            v->uv[1] = p->th > 0 ? y / p->th : 0.0f;
            v->uv[2] = p->tw > 0 ? (x + w) / p->tw : 1.0f;
            v->uv[3] = p->th > 0 ? (y + h) / p->th : 1.0f;
        } else {
            v->uv[0] = 0.0f;
            v->uv[1] = 0.0f;
            v->uv[2] = 1.0f;
            v->uv[3] = 1.0f;
        }
    }
}

static int ps_realloc_pool(ps_slot *p, int max) {
    efx_particle *parts = realloc(p->parts, (size_t)max * sizeof(efx_particle));
    if (!parts) return 0;
    p->parts = parts;
    efx_particle_view *views =
        realloc(p->views, (size_t)max * sizeof(efx_particle_view));
    if (!views) return 0;
    p->views = views;
    if (p->count > max) p->count = max;
    p->cfg.max = max;
    return 1;
}

uint64_t efx_render_particles_create(const efx_particle_config *cfg, int *err) {
    if (err) *err = EFX_RENDER_OK;
    ensure_state();
    if (!ps_config_valid(cfg)) {
        if (err) *err = EFX_RENDER_ERR_SIZE;
        return 0;
    }
    ps_slot *p = NULL;
    for (int i = 0; i < R.ps_count; i++) {
        if (!R.ps[i].used) {
            p = &R.ps[i];
            break;
        }
    }
    uint32_t gen = 0;
    if (!p) {
        if (R.ps_count >= R.ps_cap) {
            if (!pool_grow((void **)&R.ps, &R.ps_cap, R.ps_count + 1,
                           sizeof(ps_slot), 8)) {
                if (err) *err = EFX_RENDER_ERR_NOMEM;
                return 0;
            }
        }
        p = &R.ps[R.ps_count++];
    } else {
        gen = p->gen; /* preserve generation across reuse */
    }
    memset(p, 0, sizeof(*p));
    p->gen = gen + 1;
    p->cfg = *cfg;
    p->parts = calloc((size_t)cfg->max, sizeof(efx_particle));
    p->views = calloc((size_t)cfg->max, sizeof(efx_particle_view));
    if (!p->parts || !p->views) {
        free(p->parts);
        free(p->views);
        p->parts = NULL;
        p->views = NULL;
        if (err) *err = EFX_RENDER_ERR_NOMEM;
        return 0;
    }
    p->used = 1;
    p->alive = 1;
    p->active = 1;
    p->paused = 0;
    p->emit_counter = 0.0f;
    p->emitter_life = cfg->emitter_lifetime;
    p->count = 0;
    p->rng = 0x1234567u; /* fixed seed: identical config => identical motion */
    {
        int tw = 0, th = 0;
        efx_render_sample_size(cfg->texture, &tw, &th);
        p->tw = (float)tw;
        p->th = (float)th;
    }
    texture_bind_retain(cfg->texture); /* keep the texture alive (ADR 0027) */
    uint32_t idx = (uint32_t)(p - R.ps) + 1;
    return ((uint64_t)p->gen << 32) | (uint64_t)idx;
}

void ps_free_pool(ps_slot *p) {
    texture_bind_release(p->cfg.texture);
    free(p->parts);
    free(p->views);
    p->parts = NULL;
    p->views = NULL;
    p->count = 0;
}

int efx_render_particles_destroy(uint64_t h) {
    ps_slot *p = ps_get(h);
    if (!p) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (!p->alive) {
        return EFX_RENDER_OK; /* idempotent */
    }
    p->alive = 0;
    if (R.deferred_ps_count >= R.deferred_ps_cap) {
        if (!pool_grow((void **)&R.deferred_ps, &R.deferred_ps_cap,
                       R.deferred_ps_count + 1, sizeof(int), 8)) {
            return EFX_RENDER_ERR_NOMEM;
        }
    }
    R.deferred_ps[R.deferred_ps_count++] = (int)(p - R.ps);
    return EFX_RENDER_OK;
}

int efx_render_particles_count(uint64_t h) {
    ps_slot *p = ps_get(h);
    return (p && p->alive) ? p->count : -1;
}

int efx_render_particles_emit(uint64_t h, int n) {
    ps_slot *p = ps_get(h);
    if (!p || !p->alive) return EFX_RENDER_ERR_HANDLE;
    if (n < 0) return EFX_RENDER_ERR_SIZE;
    for (int i = 0; i < n; i++) {
        if (p->count >= p->cfg.max) break;
        ps_spawn(p);
    }
    return EFX_RENDER_OK;
}

void efx_render_particles_start(uint64_t h) {
    ps_slot *p = ps_get(h);
    if (p && p->alive) {
        p->active = 1;
        p->paused = 0;
    }
}

void efx_render_particles_stop(uint64_t h) {
    ps_slot *p = ps_get(h);
    if (p && p->alive) {
        p->active = 0;
        p->paused = 0;
        p->emitter_life = p->cfg.emitter_lifetime;
        p->emit_counter = 0.0f;
    }
}

void efx_render_particles_pause(uint64_t h) {
    ps_slot *p = ps_get(h);
    if (p && p->alive) {
        p->paused = 1;
    }
}

void efx_render_particles_reset(uint64_t h) {
    ps_slot *p = ps_get(h);
    if (p && p->alive) {
        p->count = 0;
        p->emit_counter = 0.0f;
        p->emitter_life = p->cfg.emitter_lifetime;
    }
}

int efx_render_particles_set(uint64_t h, const efx_particle_config *cfg) {
    ps_slot *p = ps_get(h);
    if (!p || !p->alive) return EFX_RENDER_ERR_HANDLE;
    if (!ps_config_valid(cfg)) return EFX_RENDER_ERR_SIZE;
    if (cfg->max != p->cfg.max) {
        if (!ps_realloc_pool(p, cfg->max)) {
            return EFX_RENDER_ERR_NOMEM;
        }
    }
    if (cfg->texture != p->cfg.texture) {
        texture_bind_release(p->cfg.texture);
        texture_bind_retain(cfg->texture);
        int tw = 0, th = 0;
        efx_render_sample_size(cfg->texture, &tw, &th);
        p->tw = (float)tw;
        p->th = (float)th;
    }
    p->cfg = *cfg;
    p->rng = 0x1234567u;
    return EFX_RENDER_OK;
}

void efx_render_particles_config(uint64_t h, efx_particle_config *out) {
    ps_slot *p = ps_get(h);
    if (p && p->alive && out) {
        *out = p->cfg;
    }
}

float efx_render_particles_speed_scale(uint64_t h) {
    ps_slot *p = ps_get(h);
    return (p && p->alive) ? p->cfg.speed_scale : -1.0f;
}

void efx_render_particles_set_speed_scale(uint64_t h, float s) {
    ps_slot *p = ps_get(h);
    if (p && p->alive && isfinite(s) && s > 0.0f) {
        p->cfg.speed_scale = s;
    }
}

void efx_render_particles_step(float dt) {
    if (dt <= 0.0f) return;
    for (int i = 0; i < R.ps_count; i++) {
        ps_slot *p = &R.ps[i];
        if (!p->used || !p->alive || p->paused) continue;
        float sdt = dt * p->cfg.speed_scale;
        if (sdt <= 0.0f) continue;

        int i2 = 0;
        while (i2 < p->count) {
            efx_particle *q = &p->parts[i2];
            q->life -= sdt;
            if (q->life <= 0.0f) {
                p->parts[i2] = p->parts[p->count - 1];
                p->count--;
                continue;
            }
            float radial[3] = {q->pos[0] - q->origin[0], q->pos[1] - q->origin[1],
                               q->pos[2] - q->origin[2]};
            ps_vnorm3(radial, radial);
            float tangent[3];
            ps_cross3(tangent, radial, (float[3]){0.0f, 1.0f, 0.0f});
            ps_vnorm3(tangent, tangent);
            for (int k = 0; k < 3; k++) {
                q->vel[k] += (radial[k] * q->radial_acc +
                              tangent[k] * q->tangential_acc + q->lin_acc[k] +
                              p->cfg.gravity[k]) *
                             sdt;
            }
            float damp = 1.0f / (1.0f + q->damping * sdt);
            for (int k = 0; k < 3; k++) {
                q->vel[k] *= damp;
                q->pos[k] += q->vel[k] * sdt;
            }
            float t = 1.0f - q->life / q->lifetime;
            q->rotation +=
                (q->spin_start * (1.0f - t) + q->spin_end * t) * sdt;
            i2++;
        }

        if (p->active) {
            if (p->cfg.emitter_lifetime != -1.0f) {
                p->emitter_life -= sdt;
                if (p->emitter_life < 0.0f) {
                    p->active = 0;
                }
            }
            if (p->active && p->cfg.emission_rate > 0.0f) {
                p->emit_counter += sdt * p->cfg.emission_rate;
                while (p->emit_counter >= 1.0f && p->count < p->cfg.max) {
                    p->emit_counter -= 1.0f;
                    ps_spawn(p);
                }
                if (p->emit_counter > 1.0f) p->emit_counter = 1.0f;
            }
        }
    }
}

const efx_particle_view *efx_render_particles_views(uint64_t h, int *count) {
    ps_slot *p = ps_get(h);
    if (!p || !p->alive) {
        if (count) *count = 0;
        return NULL;
    }
    ps_build_views(p);
    if (count) *count = p->count;
    return p->views;
}

int efx_render_particles_space(uint64_t h) {
    ps_slot *p = ps_get(h);
    return (p && p->alive) ? p->cfg.space : -1;
}

int efx_render_particles_facing(uint64_t h) {
    ps_slot *p = ps_get(h);
    return (p && p->alive) ? p->cfg.facing : -1;
}

uint64_t efx_render_particles_texture(uint64_t h) {
    ps_slot *p = ps_get(h);
    return (p && p->alive) ? p->cfg.texture : 0;
}

void efx_render_particles_normal(uint64_t h, float out[3]) {
    ps_slot *p = ps_get(h);
    if (p && p->alive) {
        out[0] = p->cfg.normal[0];
        out[1] = p->cfg.normal[1];
        out[2] = p->cfg.normal[2];
    } else {
        out[0] = out[1] = out[2] = 0.0f;
    }
}

void efx_render_billboard_basis(const efx_camera3d *cam, int facing,
                                const float normal[3], float right[3],
                                float up[3]) {
    float fwd[3] = {cam->target[0] - cam->pos[0], cam->target[1] - cam->pos[1],
                    cam->target[2] - cam->pos[2]};
    ps_vnorm3(fwd, fwd);
    if (facing == EFX_FACING_PLANE) {
        float n[3];
        ps_vnorm3(n, normal);
        float t[3] = {0.0f, 1.0f, 0.0f};
        if (fabsf(n[1]) > 0.9f) {
            t[0] = 1.0f; t[1] = 0.0f; t[2] = 0.0f;
        }
        float r[3];
        ps_cross3(r, t, n);
        ps_vnorm3(right, r);
        float u[3];
        ps_cross3(u, n, right);
        ps_vnorm3(up, u);
    } else if (facing == EFX_FACING_Y) {
        up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
        float r[3];
        ps_cross3(r, fwd, up);
        ps_vnorm3(right, r);
        if (right[0] == 0.0f && right[1] == 0.0f && right[2] == 0.0f) {
            right[0] = 1.0f;
        }
    } else { /* view */
        float world_up[3] = {0.0f, 1.0f, 0.0f};
        float r[3];
        ps_cross3(r, fwd, world_up);
        ps_vnorm3(right, r);
        ps_cross3(up, right, fwd);
        ps_vnorm3(up, up);
    }
}

int efx_render_billboard(uint64_t texture, const float pos[3], float w, float h,
                         const float color[4], float rotation, int facing,
                         const float normal[3], int depth_test,
                         const float src_rect[4], int has_src,
                         int blend_override) {
    ensure_state();
    if (!texture || !efx_render_sample_alive(texture)) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (!(w > 0.0f) || !(h > 0.0f) || !isfinite(w) || !isfinite(h) ||
        !isfinite(rotation)) {
        return EFX_RENDER_ERR_SIZE;
    }
    if (facing < EFX_FACING_VIEW || facing > EFX_FACING_PLANE) {
        return EFX_RENDER_ERR_SIZE;
    }
    efx_record rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = EFX_RECORD_BILLBOARD;
    efx_billboard_record *b = &rec.u.billboard;
    b->pos[0] = pos[0];
    b->pos[1] = pos[1];
    b->pos[2] = pos[2];
    b->w = w;
    b->h = h;
    color_or_white(b->color, color);
    b->rotation = rotation;
    b->facing = (uint8_t)facing;
    b->depth_test = depth_test ? 1 : 0;
    b->blend = (uint8_t)(blend_override < 0 ? R.blend : blend_override);
    b->normal[0] = normal ? normal[0] : 0.0f;
    b->normal[1] = normal ? normal[1] : 1.0f;
    b->normal[2] = normal ? normal[2] : 0.0f;
    b->texture = texture;
    int tw = 0, th = 0;
    efx_render_sample_size(texture, &tw, &th);
    b->tw = (float)tw;
    b->th = (float)th;
    if (has_src && src_rect) {
        b->sx = src_rect[0];
        b->sy = src_rect[1];
        b->sw = src_rect[2];
        b->sh = src_rect[3];
    } else {
        b->sx = 0.0f;
        b->sy = 0.0f;
        b->sw = (float)tw;
        b->sh = (float)th;
    }
    b->camera = R.camera3d;
    return record_push(&rec);
}

int efx_render_particles_draw(uint64_t h) {
    ensure_state();
    ps_slot *p = ps_get(h);
    if (!p || !p->alive) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (p->cfg.texture && p->cfg.texture == R.active_target) {
        return EFX_RENDER_ERR_FEEDBACK;
    }
    efx_record rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = EFX_RECORD_PARTICLES;
    efx_particle_record *pr = &rec.u.particles;
    pr->system = h;
    pr->blend = (uint8_t)(p->cfg.blend < 0 ? R.blend : p->cfg.blend);
    pr->camera = R.camera3d;
    pr->camera2d = R.camera;
    {
        int sw = 0, sh = 0;
        efx_render_surface_size(&sw, &sh);
        pr->frame_w = R.camera.frame_w > 0.0f ? R.camera.frame_w : (float)sw;
        pr->frame_h = R.camera.frame_h > 0.0f ? R.camera.frame_h : (float)sh;
    }
    return record_push(&rec);
}

