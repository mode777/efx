#include "physics/world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------ registry */

efx_phys_character efx_physics_create_character(efx_physics_world *w,
                                                const efx_character_desc *d) {
    if (!w || !d) return 0;
    if (!(d->radius > 0) || !(d->height >= 2 * d->radius)) return 0;
    if (!(efx_v3_len_sq(d->up) > 0)) return 0;
    if (!(d->max_slides > 0)) return 0;
    int slot = efx_world_alloc_char_slot(w);
    if (slot < 0) return 0;
    efx_pcharacter *c = &w->chars[slot];
    memset(c, 0, sizeof(*c));
    c->alive = 1;
    c->used = 1;
    c->id = w->next_id++;
    c->position = d->position;
    c->velocity = efx_v3(0, 0, 0);
    c->radius = d->radius;
    c->half_height = d->height * 0.5f - d->radius;
    if (c->half_height < 0) c->half_height = 0;
    c->up = efx_v3_normalize(d->up);
    c->floor_cos = cosf(efx_deg_to_rad(d->floor_max_angle));
    c->floor_snap_length = d->floor_snap_length;
    c->step_height = d->step_height;
    c->safe_margin = d->safe_margin;
    c->max_slides = d->max_slides;
    c->layer = d->layer;
    c->mask = d->mask;
    c->on_floor = 0;
    c->floor_normal = c->up;
    return c->id;
}

int efx_physics_destroy_character(efx_physics_world *w,
                                  efx_phys_character id) {
    if (!w) return 0;
    int slot = efx_world_char_slot(w, id);
    if (slot < 0) return 0;
    free(w->chars[slot].collisions);
    w->chars[slot].collisions = NULL;
    w->chars[slot].collision_count = 0;
    w->chars[slot].collision_cap = 0;
    w->chars[slot].alive = 0;
    w->char_free[w->char_free_count++] = slot;
    return 1;
}

int efx_physics_character_alive(const efx_physics_world *w,
                                efx_phys_character id) {
    return efx_world_char_slot(w, id) >= 0;
}

/* ---------------------------------------------------------- accessors */

int efx_physics_character_position(efx_physics_world *w, efx_phys_character id,
                                   efx_vec3 *out) {
    int s = efx_world_char_slot(w, id);
    if (s < 0) return 0;
    if (out) *out = w->chars[s].position;
    return 1;
}

int efx_physics_character_set_position(efx_physics_world *w,
                                       efx_phys_character id, efx_vec3 p) {
    int s = efx_world_char_slot(w, id);
    if (s < 0) return 0;
    w->chars[s].position = p;
    return 1;
}

int efx_physics_character_velocity(efx_physics_world *w, efx_phys_character id,
                                   efx_vec3 *out) {
    int s = efx_world_char_slot(w, id);
    if (s < 0) return 0;
    if (out) *out = w->chars[s].velocity;
    return 1;
}

int efx_physics_character_set_velocity(efx_physics_world *w,
                                       efx_phys_character id, efx_vec3 v) {
    int s = efx_world_char_slot(w, id);
    if (s < 0) return 0;
    w->chars[s].velocity = v;
    return 1;
}

int efx_physics_character_on_floor(efx_physics_world *w,
                                   efx_phys_character id) {
    int s = efx_world_char_slot(w, id);
    return s < 0 ? 0 : w->chars[s].on_floor;
}

int efx_physics_move_collision_count(efx_physics_world *w,
                                     efx_phys_character id) {
    int s = efx_world_char_slot(w, id);
    return s < 0 ? 0 : w->chars[s].collision_count;
}

int efx_physics_move_collision(efx_physics_world *w, efx_phys_character id,
                               int index, efx_move_collision *out) {
    int s = efx_world_char_slot(w, id);
    if (s < 0) return 0;
    efx_pcharacter *c = &w->chars[s];
    if (index < 0 || index >= c->collision_count) return 0;
    if (out) *out = c->collisions[index];
    return 1;
}

/* ------------------------------------------------------- move and slide */

typedef struct char_sweep_hit {
    int hit;
    float t;
    efx_vec3 point;
    efx_vec3 normal;
    efx_phys_body body; /* 0 for a static mesh */
    int sensor;
} char_sweep_hit;

typedef struct char_sweep_ctx {
    efx_pcharacter *ch;
    const efx_shape *capsule;
    efx_vec3 from;
    efx_vec3 motion;
    char_sweep_hit *out;
    efx_physics_world *w;
} char_sweep_ctx;

typedef struct char_mesh_ctx {
    char_sweep_ctx *s;
    efx_vec3 offset;
} char_mesh_ctx;

static int char_mesh_cb(void *ud, const efx_phys_mesh *m, int tri) {
    char_mesh_ctx *mc = ud;
    efx_vec3 v[3];
    efx_phys_mesh_tri(m, tri, v);
    for (int k = 0; k < 3; k++) v[k] = efx_v3_add(v[k], mc->offset);
    float t;
    efx_vec3 point, normal;
    if (efx_narrow_sweep_triangle(mc->s->capsule, mc->s->from,
                                  mc->s->motion, v[0], v[1], v[2], &t, &point,
                                  &normal)) {
        if (!mc->s->out->hit || t < mc->s->out->t) {
            mc->s->out->hit = 1;
            mc->s->out->t = t;
            mc->s->out->point = point;
            mc->s->out->normal = normal;
            mc->s->out->body = 0;
            mc->s->out->sensor = 0;
        }
    }
    return 0;
}

/* earliest hit of the character capsule sweeping from `from` by `motion`
 * against solid static (non-sensor) geometry, respecting layers/masks */
static void char_sweep(efx_physics_world *w, efx_pcharacter *ch,
                       efx_shape *capsule, efx_vec3 from, efx_vec3 motion,
                       char_sweep_hit *out) {
    memset(out, 0, sizeof(*out));
    out->t = 1.0f;
    if (efx_v3_len_sq(motion) < 1e-12f) return;

    efx_aabb swept;
    efx_vec3 end = efx_v3_add(from, motion);
    swept.min = efx_v3_min(from, end);
    swept.max = efx_v3_max(from, end);
    efx_aabb cb;
    efx_shape_bounds(capsule, from, &cb);
    efx_vec3 grow = efx_v3_sub(from, cb.min);
    swept = efx_aabb_expand(swept, fmaxf(grow.x, fmaxf(grow.y, grow.z)));
    swept = efx_aabb_expand(swept, 0.02f);

    char_sweep_ctx sc = {ch, capsule, from, motion, out, w};
    for (int i = 0; i < w->body_count; i++) {
        efx_pbody *b = &w->bodies[i];
        if (!b->alive || b->kind != EFX_PBODY_STATIC || b->sensor) continue;
        if (!efx_world_layers_match(ch->layer, ch->mask, b->layer, b->mask)) {
            continue;
        }
        if (!efx_aabb_overlap(b->aabb, swept)) continue;
        if (b->shape.type == EFX_PHYS_SHAPE_MESH && b->shape.mesh) {
            efx_aabb local;
            local.min = efx_v3_sub(swept.min, b->position);
            local.max = efx_v3_sub(swept.max, b->position);
            char_mesh_ctx mc = {&sc, b->position};
            efx_phys_mesh_query_aabb(b->shape.mesh, local, char_mesh_cb, &mc);
        } else {
            float t;
            efx_vec3 point, normal;
            if (efx_narrow_sweep(capsule, from, motion, &b->shape, b->position,
                                 &t, &point, &normal)) {
                if (!out->hit || t < out->t) {
                    out->hit = 1;
                    out->t = t;
                    out->point = point;
                    out->normal = normal;
                    out->body = b->id;
                    out->sensor = 0;
                }
            }
        }
    }
}

static int is_floor_normal(efx_pcharacter *ch, efx_vec3 n) {
    return efx_v3_dot(n, ch->up) >= ch->floor_cos;
}

static int is_ceiling_normal(efx_pcharacter *ch, efx_vec3 n) {
    return efx_v3_dot(n, ch->up) <= -ch->floor_cos;
}

static void record_collision(efx_pcharacter *ch, efx_phys_body body,
                             efx_vec3 normal, efx_vec3 point) {
    if (ch->collision_count >= ch->collision_cap) {
        int ncap = ch->collision_cap ? ch->collision_cap * 2 : 4;
        efx_move_collision *np =
            realloc(ch->collisions, (size_t)ncap * sizeof(*np));
        if (!np) return;
        ch->collisions = np;
        ch->collision_cap = ncap;
    }
    efx_move_collision *m = &ch->collisions[ch->collision_count++];
    m->body = body;
    m->sensor = 0;
    m->normal = normal;
    m->point = point;
}

/* attempt a step-up over a low ledge; returns 1 and updates *pos when it
 * succeeds */
static int try_step_up(efx_physics_world *w, efx_pcharacter *ch,
                       efx_shape *capsule, efx_vec3 *pos, efx_vec3 horizontal,
                       efx_vec3 *slid_remaining) {
    if (ch->step_height <= 0) return 0;
    float hlen = sqrtf(horizontal.x * horizontal.x + horizontal.z * horizontal.z);
    if (hlen < 1e-5f) return 0;
    char_sweep_hit up_hit;
    efx_vec3 up_motion = efx_v3_scale(ch->up, ch->step_height);
    char_sweep(w, ch, capsule, *pos, up_motion, &up_hit);
    /* the floor the character is standing on is not a ceiling: only a real
     * downward-facing contact blocks the rise */
    if (up_hit.hit && up_hit.t < 0.999f &&
        is_ceiling_normal(ch, up_hit.normal)) {
        return 0;
    }
    efx_vec3 raised = efx_v3_add(*pos, up_motion);

    char_sweep_hit fwd_hit;
    char_sweep(w, ch, capsule, raised, horizontal, &fwd_hit);
    if (fwd_hit.hit && fwd_hit.t < 0.999f &&
        !is_floor_normal(ch, fwd_hit.normal)) {
        return 0; /* obstacle too tall */
    }
    efx_vec3 forward =
        efx_v3_add(raised, efx_v3_scale(horizontal, fwd_hit.hit ? fwd_hit.t : 1));

    char_sweep_hit down_hit;
    efx_vec3 down_motion = efx_v3_scale(ch->up, -(ch->step_height + 0.05f));
    char_sweep(w, ch, capsule, forward, down_motion, &down_hit);
    if (!down_hit.hit || !is_floor_normal(ch, down_hit.normal)) return 0;

    efx_vec3 landed =
        efx_v3_add(forward, efx_v3_scale(down_motion, down_hit.t));
    *pos = landed;
    ch->on_floor = 1;
    ch->floor_normal = down_hit.normal;
    record_collision(ch, down_hit.body, down_hit.normal, down_hit.point);
    if (slid_remaining) *slid_remaining = efx_v3(0, 0, 0);
    return 1;
}

int efx_physics_character_move_and_slide(efx_physics_world *w,
                                         efx_phys_character id, efx_vec3 motion,
                                         efx_move_result *out) {
    int s = efx_world_char_slot(w, id);
    if (s < 0 || !out) return 0;
    efx_pcharacter *ch = &w->chars[s];
    efx_shape capsule =
        efx_shape_capsule(ch->radius, ch->half_height * 2 + ch->radius * 2);

    int was_on_floor = ch->on_floor;
    ch->collision_count = 0;
    int on_floor = 0, on_wall = 0, on_ceiling = 0;
    efx_vec3 floor_normal = ch->up;
    efx_vec3 pos = ch->position;
    efx_vec3 remaining = motion;

    for (int slide = 0; slide < ch->max_slides; slide++) {
        if (efx_v3_len_sq(remaining) < 1e-12f) break;
        char_sweep_hit hit;
        char_sweep(w, ch, &capsule, pos, remaining, &hit);
        if (!hit.hit) {
            pos = efx_v3_add(pos, remaining);
            remaining = efx_v3(0, 0, 0);
            break;
        }
        pos = efx_v3_add(pos, efx_v3_scale(remaining, hit.t));
        efx_vec3 n = hit.normal;
        int is_ceiling = is_ceiling_normal(ch, n);
        int is_floor = is_floor_normal(ch, n);
        if (is_floor) {
            on_floor = 1;
            floor_normal = n;
        } else if (is_ceiling) {
            on_ceiling = 1;
        } else {
            on_wall = 1;
        }
        record_collision(ch, hit.body, n, hit.point);

        /* try to step over a low ledge before sliding */
        efx_vec3 left = efx_v3_scale(remaining, 1.0f - hit.t);
        efx_vec3 horizontal = efx_v3_sub(
            left, efx_v3_scale(ch->up, efx_v3_dot(left, ch->up)));
        if (!is_floor && !is_ceiling && was_on_floor && ch->step_height > 0) {
            efx_vec3 stepped = pos;
            efx_vec3 dummy = efx_v3(0, 0, 0);
            if (try_step_up(w, ch, &capsule, &stepped, horizontal, &dummy)) {
                pos = stepped;
                remaining = dummy;
                on_wall = 0;
                continue;
            }
        }

        /* separate slightly and slide the remaining motion along the plane */
        pos = efx_v3_add(pos, efx_v3_scale(n, ch->safe_margin));
        efx_vec3 slide_motion =
            efx_v3_sub(left, efx_v3_scale(n, efx_v3_dot(left, n)));
        remaining = slide_motion;
    }

    /* floor snap (never when the motion lifts the character) */
    int moving_up = efx_v3_dot(motion, ch->up) > 1e-5f;
    if (ch->floor_snap_length > 0 && !moving_up && (was_on_floor || on_floor)) {
        char_sweep_hit snap;
        efx_vec3 down = efx_v3_scale(ch->up, -ch->floor_snap_length);
        char_sweep(w, ch, &capsule, pos, down, &snap);
        if (snap.hit && snap.t < 0.999f && is_floor_normal(ch, snap.normal)) {
            pos = efx_v3_add(pos, efx_v3_scale(down, snap.t));
            on_floor = 1;
            floor_normal = snap.normal;
        }
    }

    if (efx_v3_dot(motion, ch->up) > 1e-5f) {
        on_floor = 0; /* jumping detaches */
    }

    ch->position = pos;
    ch->on_floor = on_floor;
    if (on_floor) ch->floor_normal = floor_normal;

    out->position = pos;
    out->on_floor = on_floor;
    out->on_wall = on_wall;
    out->on_ceiling = on_ceiling;
    out->floor_normal = on_floor ? floor_normal : ch->up;
    out->collision_count = ch->collision_count;
    return 1;
}
