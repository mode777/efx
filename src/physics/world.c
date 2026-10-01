#include "physics/world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define EFX_INIT_CAP 16
#define EFX_ALL_MASK 0xFFFFFFFFu

/* ------------------------------------------------------------ small helpers */

static int grow(void **ptr, int *cap, int need, size_t elem) {
    if (need <= *cap) return 1;
    int ncap = *cap ? *cap : EFX_INIT_CAP;
    while (ncap < need) ncap *= 2;
    void *np = realloc(*ptr, (size_t)ncap * elem);
    if (!np) return 0;
    *ptr = np;
    *cap = ncap;
    return 1;
}

static void update_body_aabb(efx_physics_world *w, efx_pbody *b) {
    (void)w;
    if (b->shape.type == EFX_PHYS_SHAPE_MESH && b->shape.mesh) {
        efx_aabb local;
        efx_phys_mesh_bounds(b->shape.mesh, &local);
        local.min = efx_v3_add(local.min, b->position);
        local.max = efx_v3_add(local.max, b->position);
        b->aabb = local;
    } else {
        efx_shape_bounds(&b->shape, b->position, &b->aabb);
    }
}

static int efx_world_body_slot(const efx_physics_world *w, efx_phys_body id) {
    if (id == 0) return -1;
    for (int i = 0; i < w->body_count; i++) {
        if (w->bodies[i].alive && w->bodies[i].id == id) return i;
    }
    return -1;
}

int efx_world_char_slot(const efx_physics_world *w, efx_phys_character id) {
    if (id == 0) return -1;
    for (int i = 0; i < w->char_count; i++) {
        if (w->chars[i].alive && w->chars[i].id == id) return i;
    }
    return -1;
}

/* --------------------------------------------------------------- lifecycle */

efx_physics_world *efx_physics_world_new(void) {
    efx_physics_world *w = calloc(1, sizeof(*w));
    if (!w) return NULL;
    w->gravity = efx_v3(0, -9.81f, 0);
    w->iterations = 8;
    w->next_id = 1;
    return w;
}

static void free_body_contents(efx_pbody *b) {
    if (b->owns_mesh && b->shape.mesh) {
        efx_phys_mesh_free((efx_phys_mesh *)b->shape.mesh);
        b->shape.mesh = NULL;
    }
    free(b->contacts);
    b->contacts = NULL;
    b->contact_count = b->contact_cap = 0;
}

void efx_physics_clear(efx_physics_world *w) {
    if (!w) return;
    for (int i = 0; i < w->body_count; i++) {
        if (w->bodies[i].used) free_body_contents(&w->bodies[i]);
        w->bodies[i].alive = 0;
    }
    w->body_free_count = 0;
    for (int i = 0; i < w->body_count; i++) {
        w->body_free[w->body_free_count++] = i;
    }
    for (int i = 0; i < w->char_count; i++) {
        free(w->chars[i].collisions);
        w->chars[i].collisions = NULL;
        w->chars[i].collision_count = w->chars[i].collision_cap = 0;
        w->chars[i].alive = 0;
    }
    w->char_free_count = 0;
    for (int i = 0; i < w->char_count; i++) {
        w->char_free[w->char_free_count++] = i;
    }
    w->pair_count = 0;
}

void efx_physics_world_free(efx_physics_world *w) {
    if (!w) return;
    for (int i = 0; i < w->body_count; i++) {
        if (w->bodies[i].used) free_body_contents(&w->bodies[i]);
    }
    for (int i = 0; i < w->char_count; i++) {
        free(w->chars[i].collisions);
    }
    free(w->bodies);
    free(w->body_free);
    free(w->chars);
    free(w->char_free);
    free(w->meshes);
    free(w->pairs);
    free(w);
}

void efx_physics_set_gravity(efx_physics_world *w, efx_vec3 g) {
    if (w) w->gravity = g;
}

efx_vec3 efx_physics_gravity(const efx_physics_world *w) {
    return w ? w->gravity : efx_v3(0, -9.81f, 0);
}

void efx_physics_set_iterations(efx_physics_world *w, int iterations) {
    if (w && iterations > 0) w->iterations = iterations;
}

int efx_physics_iterations(const efx_physics_world *w) {
    return w ? w->iterations : 8;
}

/* ------------------------------------------------------------ body factory */

static int efx_world_alloc_body_slot(efx_physics_world *w) {
    if (w->body_free_count > 0) {
        return w->body_free[--w->body_free_count];
    }
    if (!grow((void **)&w->bodies, &w->body_cap, w->body_count + 1,
              sizeof(efx_pbody))) {
        return -1;
    }
    if (!grow((void **)&w->body_free, &w->body_free_cap, w->body_cap,
              sizeof(int))) {
        return -1;
    }
    int idx = w->body_count++;
    memset(&w->bodies[idx], 0, sizeof(w->bodies[idx]));
    return idx;
}

int efx_world_alloc_char_slot(efx_physics_world *w) {
    if (w->char_free_count > 0) {
        return w->char_free[--w->char_free_count];
    }
    if (!grow((void **)&w->chars, &w->char_cap, w->char_count + 1,
              sizeof(efx_pcharacter))) {
        return -1;
    }
    if (!grow((void **)&w->char_free, &w->char_free_cap, w->char_cap,
              sizeof(int))) {
        return -1;
    }
    int idx = w->char_count++;
    memset(&w->chars[idx], 0, sizeof(w->chars[idx]));
    return idx;
}

efx_phys_body efx_physics_create_body(efx_physics_world *w,
                                      const efx_body_desc *desc) {
    if (!w || !desc) return 0;
    int slot = efx_world_alloc_body_slot(w);
    if (slot < 0) return 0;
    efx_pbody *b = &w->bodies[slot];
    b->alive = 1;
    b->used = 1;
    b->id = w->next_id++;
    b->kind = desc->dynamic ? EFX_PBODY_DYNAMIC : EFX_PBODY_STATIC;
    b->sensor = desc->sensor ? 1 : 0;
    b->shape = desc->shape;
    b->position = desc->position;
    b->velocity = efx_v3(0, 0, 0);
    b->force = efx_v3(0, 0, 0);
    b->friction = desc->friction;
    b->restitution = desc->restitution;
    b->layer = desc->layer;
    b->mask = desc->mask;
    if (b->kind == EFX_PBODY_DYNAMIC && desc->mass > 0) {
        b->mass = desc->mass;
        b->inv_mass = 1.0f / desc->mass;
    } else {
        b->mass = 0;
        b->inv_mass = 0;
    }
    update_body_aabb(w, b);
    return b->id;
}

efx_phys_body efx_physics_create_static_mesh(efx_physics_world *w,
                                             const float *positions,
                                             int vert_count,
                                             const uint32_t *indices,
                                             int tri_count,
                                             const efx_static_mesh_desc *desc) {
    if (!w || !positions || vert_count <= 0) return 0;
    efx_phys_mesh *mesh =
        efx_phys_mesh_create(positions, vert_count, indices, tri_count);
    if (!mesh) return 0;

    int slot = efx_world_alloc_body_slot(w);
    if (slot < 0) {
        efx_phys_mesh_free(mesh);
        return 0;
    }
    efx_pbody *b = &w->bodies[slot];
    memset(b, 0, sizeof(*b));
    b->alive = 1;
    b->used = 1;
    b->id = w->next_id++;
    b->kind = EFX_PBODY_STATIC;
    b->sensor = desc ? (desc->sensor ? 1 : 0) : 0;
    b->owns_mesh = 1;
    b->shape = efx_shape_sphere(0);
    b->shape.type = EFX_PHYS_SHAPE_MESH;
    b->shape.mesh = mesh;
    b->position = desc ? desc->position : efx_v3(0, 0, 0);
    b->friction = desc ? desc->friction : 0.5f;
    b->restitution = desc ? desc->restitution : 0.0f;
    b->layer = desc ? desc->layer : EFX_ALL_MASK;
    b->mask = desc ? desc->mask : EFX_ALL_MASK;
    update_body_aabb(w, b);
    return b->id;
}

int efx_physics_destroy_body(efx_physics_world *w, efx_phys_body id) {
    if (!w) return 0;
    int slot = efx_world_body_slot(w, id);
    if (slot < 0) return 0;
    free_body_contents(&w->bodies[slot]);
    w->bodies[slot].alive = 0;
    w->body_free[w->body_free_count++] = slot;
    return 1;
}

int efx_physics_body_alive(const efx_physics_world *w, efx_phys_body b) {
    return efx_world_body_slot(w, b) >= 0;
}

/* --------------------------------------------------------- body accessors */

int efx_physics_body_position(efx_physics_world *w, efx_phys_body id,
                              efx_vec3 *out) {
    int s = efx_world_body_slot(w, id);
    if (s < 0) return 0;
    if (out) *out = w->bodies[s].position;
    return 1;
}

int efx_physics_body_set_position(efx_physics_world *w, efx_phys_body id,
                                  efx_vec3 p) {
    int s = efx_world_body_slot(w, id);
    if (s < 0) return 0;
    w->bodies[s].position = p;
    update_body_aabb(w, &w->bodies[s]);
    return 1;
}

int efx_physics_body_velocity(efx_physics_world *w, efx_phys_body id,
                              efx_vec3 *out) {
    int s = efx_world_body_slot(w, id);
    if (s < 0) return 0;
    if (out) *out = w->bodies[s].velocity;
    return 1;
}

int efx_physics_body_set_velocity(efx_physics_world *w, efx_phys_body id,
                                  efx_vec3 v) {
    int s = efx_world_body_slot(w, id);
    if (s < 0) return 0;
    w->bodies[s].velocity = v;
    return 1;
}

int efx_physics_body_apply_impulse(efx_physics_world *w, efx_phys_body id,
                                   efx_vec3 impulse) {
    int s = efx_world_body_slot(w, id);
    if (s < 0) return 0;
    efx_pbody *b = &w->bodies[s];
    if (b->kind != EFX_PBODY_DYNAMIC) return 0;
    b->velocity = efx_v3_add(b->velocity, efx_v3_scale(impulse, b->inv_mass));
    return 1;
}

int efx_physics_body_apply_force(efx_physics_world *w, efx_phys_body id,
                                 efx_vec3 force) {
    int s = efx_world_body_slot(w, id);
    if (s < 0) return 0;
    efx_pbody *b = &w->bodies[s];
    if (b->kind != EFX_PBODY_DYNAMIC) return 0;
    b->force = efx_v3_add(b->force, force);
    return 1;
}

int efx_physics_body_contact_count(efx_physics_world *w, efx_phys_body id) {
    int s = efx_world_body_slot(w, id);
    return s < 0 ? 0 : w->bodies[s].contact_count;
}

int efx_physics_body_contact(efx_physics_world *w, efx_phys_body id, int index,
                             efx_contact_info *out) {
    int s = efx_world_body_slot(w, id);
    if (s < 0) return 0;
    efx_pbody *b = &w->bodies[s];
    if (index < 0 || index >= b->contact_count) return 0;
    efx_contact_record *r = &b->contacts[index];
    if (out) {
        out->body = r->other;
        out->character = r->character;
        out->sensor = r->sensor;
        out->normal = r->normal;
        out->point = r->point;
        out->depth = r->depth;
        out->impulse = r->impulse;
    }
    return 1;
}

/* --------------------------------------------------------- contact generation */

static int add_contact_record(efx_pbody *b, const efx_contact_record *rec) {
    if (b->contact_count >= b->contact_cap) {
        int ncap = b->contact_cap ? b->contact_cap * 2 : 8;
        efx_contact_record *np =
            realloc(b->contacts, (size_t)ncap * sizeof(*np));
        if (!np) return 0;
        b->contacts = np;
        b->contact_cap = ncap;
    }
    b->contacts[b->contact_count++] = *rec;
    return 1;
}

static void push_pair(efx_physics_world *w, int a_type, int a_index, int b_type,
                      int b_index, const efx_narrow_contact *c, int sensor,
                      float friction, float restitution) {
    if (!grow((void **)&w->pairs, &w->pair_cap, w->pair_count + 1,
              sizeof(efx_contact_pair))) {
        return;
    }
    efx_contact_pair *p = &w->pairs[w->pair_count++];
    p->a_type = a_type;
    p->a_index = a_index;
    p->b_type = b_type;
    p->b_index = b_index;
    p->sensor = sensor;
    p->normal = c->normal;
    p->point = c->point;
    p->depth = c->depth;
    p->friction = friction;
    p->restitution = restitution;
    p->bounce = 0;
    p->normal_impulse = 0;
    p->tangent_impulse = 0;
}

typedef struct mesh_query {
    efx_physics_world *w;
    const efx_shape *sa;
    efx_vec3 pa;
    efx_vec3 offset;
    int a_index;
    int b_index;
    float friction, restitution;
    int sensor;
} mesh_query;

static int mesh_tri_cb(void *ud, const efx_phys_mesh *m, int tri) {
    mesh_query *q = ud;
    efx_vec3 v[3];
    efx_phys_mesh_tri(m, tri, v);
    for (int k = 0; k < 3; k++) v[k] = efx_v3_add(v[k], q->offset);
    efx_narrow_contact c;
    if (efx_narrow_shape_triangle(q->sa, q->pa, v[0], v[1], v[2], &c)) {
        push_pair(q->w, 0, q->a_index, 0, q->b_index, &c, q->sensor,
                  q->friction, q->restitution);
    }
    return 0;
}

static void gen_dynamic_vs_mesh(efx_physics_world *w, int dyn_index,
                                int mesh_index) {
    efx_pbody *d = &w->bodies[dyn_index];
    efx_pbody *m = &w->bodies[mesh_index];
    efx_aabb q = efx_aabb_expand(d->aabb, EFX_PHYS_BROAD_MARGIN);
    efx_aabb local;
    local.min = efx_v3_sub(q.min, m->position);
    local.max = efx_v3_sub(q.max, m->position);
    mesh_query ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.w = w;
    ctx.sa = &d->shape;
    ctx.pa = d->position;
    ctx.offset = m->position;
    ctx.a_index = dyn_index;
    ctx.b_index = mesh_index;
    ctx.sensor = d->sensor || m->sensor;
    ctx.friction = sqrtf(fmaxf(d->friction, 0) * fmaxf(m->friction, 0));
    ctx.restitution = fmaxf(d->restitution, m->restitution);
    efx_phys_mesh_query_aabb(m->shape.mesh, local, mesh_tri_cb, &ctx);
}

/* fills w->pairs for the current positions and resets each dynamic body's
 * contact report */
static void efx_world_generate_contacts(efx_physics_world *w) {
    w->pair_count = 0;
    for (int i = 0; i < w->body_count; i++) {
        if (w->bodies[i].alive) w->bodies[i].contact_count = 0;
    }

    for (int i = 0; i < w->body_count; i++) {
        efx_pbody *a = &w->bodies[i];
        if (!a->alive || a->kind != EFX_PBODY_DYNAMIC) continue;
        for (int j = 0; j < w->body_count; j++) {
            efx_pbody *b = &w->bodies[j];
            if (!b->alive) continue;
            if (b->kind == EFX_PBODY_DYNAMIC && j <= i) continue;
            if (!efx_world_layers_match(a->layer, a->mask, b->layer, b->mask)) {
                continue;
            }
            int sensor = a->sensor || b->sensor;
            float friction =
                sqrtf(fmaxf(a->friction, 0) * fmaxf(b->friction, 0));
            float restitution = fmaxf(a->restitution, b->restitution);
            efx_narrow_contact c;

            if (b->shape.type == EFX_PHYS_SHAPE_MESH) {
                gen_dynamic_vs_mesh(w, i, j);
                continue;
            }
            if (efx_narrow_overlap(&a->shape, a->position, &b->shape,
                                   b->position, &c)) {
                push_pair(w, 0, i, 0, j, &c, sensor, friction, restitution);
            }
        }
        /* dynamic vs every character (characters are immovable) */
        for (int ci = 0; ci < w->char_count; ci++) {
            efx_pcharacter *ch = &w->chars[ci];
            if (!ch->alive) continue;
            if (!efx_world_layers_match(a->layer, a->mask, ch->layer,
                                        ch->mask)) {
                continue;
            }
            efx_shape cap_shape = efx_shape_capsule(ch->radius,
                                              ch->half_height * 2 +
                                                  ch->radius * 2);
            efx_narrow_contact c;
            if (efx_narrow_overlap(&a->shape, a->position, &cap_shape, ch->position,
                                   &c)) {
                push_pair(w, 0, i, 1, ci, &c, a->sensor, 0.4f,
                          a->restitution);
            }
        }
    }
}

static void efx_world_report_contacts(efx_physics_world *w) {
    for (int k = 0; k < w->pair_count; k++) {
        efx_contact_pair *p = &w->pairs[k];
        if (p->a_type == 0) {
            efx_pbody *a = &w->bodies[p->a_index];
            if (a->kind != EFX_PBODY_DYNAMIC) continue;
            efx_contact_record rec;
            memset(&rec, 0, sizeof(rec));
            if (p->b_type == 0) {
                efx_pbody *b = &w->bodies[p->b_index];
                rec.other = (b->shape.type == EFX_PHYS_SHAPE_MESH) ? 0 : b->id;
                rec.character = 0;
            } else {
                rec.other = 0;
                rec.character = w->chars[p->b_index].id;
            }
            rec.sensor = p->sensor;
            rec.normal = p->normal;
            rec.point = p->point;
            rec.depth = p->depth;
            rec.impulse = p->normal_impulse;
            add_contact_record(a, &rec);
        }
        if (p->b_type == 0) {
            efx_pbody *b = &w->bodies[p->b_index];
            if (b->kind != EFX_PBODY_DYNAMIC) continue;
            efx_contact_record rec;
            memset(&rec, 0, sizeof(rec));
            if (p->a_type == 0) {
                efx_pbody *a = &w->bodies[p->a_index];
                rec.other = (a->shape.type == EFX_PHYS_SHAPE_MESH) ? 0 : a->id;
            } else {
                rec.character = w->chars[p->a_index].id;
            }
            rec.sensor = p->sensor;
            rec.normal = efx_v3_neg(p->normal);
            rec.point = p->point;
            rec.depth = p->depth;
            rec.impulse = p->normal_impulse;
            add_contact_record(b, &rec);
        }
    }
}

/* -------------------------------------------------------------------- step */

void efx_physics_step(efx_physics_world *w, float dt) {
    if (!w || !isfinite(dt) || dt <= 0) return;
    if (dt > EFX_PHYS_MAX_DT) dt = EFX_PHYS_MAX_DT;

    /* Frame-rate robustness (design physics-tunneling D1/D3): never advance a
     * body by more than a bounded translation per collision sample, so a large
     * frame dt cannot skip a thin static collider. Equal substeps derived only
     * from dt keep a call sequence deterministic; a dt at or below the bound is
     * a single substep and reproduces the un-subdivided arithmetic exactly. */
    int substeps = (int)ceilf(dt / EFX_PHYS_MAX_SUBSTEP);
    if (substeps < 1) substeps = 1;
    const float h = dt / (float)substeps;

    for (int s = 0; s < substeps; s++) {
        for (int i = 0; i < w->body_count; i++) {
            efx_pbody *b = &w->bodies[i];
            if (!b->alive || b->kind != EFX_PBODY_DYNAMIC) continue;
            b->velocity = efx_v3_add(b->velocity, efx_v3_scale(w->gravity, h));
            b->velocity = efx_v3_add(
                b->velocity, efx_v3_scale(b->force, b->inv_mass * h));
            b->position = efx_v3_add(b->position, efx_v3_scale(b->velocity, h));
            update_body_aabb(w, b);
        }

        efx_world_generate_contacts(w);
        efx_solver_solve(w, h);
    }

    /* the accumulated force acted over the whole step; consume it exactly once
     * (clearing per substep would silently scale it by 1/substeps) */
    for (int i = 0; i < w->body_count; i++) {
        efx_pbody *b = &w->bodies[i];
        if (!b->alive || b->kind != EFX_PBODY_DYNAMIC) continue;
        b->force = efx_v3(0, 0, 0);
    }

    efx_world_report_contacts(w);
}

/* ----------------------------------------------------------------- queries */

static int ray_mask_ok(uint32_t layer, uint32_t mask) {
    return mask == 0 || (layer & mask) != 0;
}

typedef struct ray_acc {
    efx_ray_hit *hits;
    int count;
    int cap;
    int all;
    float best;
} ray_acc;

static void ray_consider(ray_acc *acc, const efx_ray_hit *h) {
    if (h->distance < 0 || h->distance > acc->best) return;
    if (!acc->all) {
        acc->hits[0] = *h;
        acc->count = 1;
        acc->best = h->distance;
        return;
    }
    if (acc->count < acc->cap) {
        acc->hits[acc->count++] = *h;
    } else {
        /* replace the current farthest if this is nearer */
        int far = 0;
        for (int i = 1; i < acc->count; i++) {
            if (acc->hits[i].distance > acc->hits[far].distance) far = i;
        }
        if (h->distance < acc->hits[far].distance) acc->hits[far] = *h;
    }
}

typedef struct ray_mesh_ctx {
    ray_acc *acc;
    efx_vec3 origin, dir, offset;
    float maxd;
    efx_phys_body body;
    int sensor;
} ray_mesh_ctx;

static int ray_mesh_tri_cb(void *ud, const efx_phys_mesh *m, int tri) {
    ray_mesh_ctx *ctx = ud;
    efx_vec3 v[3];
    efx_phys_mesh_tri(m, tri, v);
    for (int k = 0; k < 3; k++) v[k] = efx_v3_add(v[k], ctx->offset);
    float t;
    efx_vec3 n;
    if (efx_narrow_ray_triangle(ctx->origin, ctx->dir, v[0], v[1], v[2],
                                ctx->maxd, &t, &n)) {
        efx_ray_hit h;
        h.point = efx_v3_add(ctx->origin, efx_v3_scale(ctx->dir, t));
        h.normal = n;
        h.distance = t;
        h.body = ctx->body;
        h.character = 0;
        h.sensor = ctx->sensor;
        ray_consider(ctx->acc, &h);
    }
    return 0;
}

static int ray_hit_cmp(const void *a, const void *b) {
    const efx_ray_hit *ha = a;
    const efx_ray_hit *hb = b;
    if (ha->distance < hb->distance) return -1;
    if (ha->distance > hb->distance) return 1;
    return 0;
}

int efx_physics_raycast(efx_physics_world *w, efx_vec3 origin, efx_vec3 dir,
                        float max_distance, uint32_t mask, int sensors, int all,
                        efx_ray_hit *out, int cap) {
    if (!w || !out || cap <= 0 || max_distance <= 0) return 0;
    float len = efx_v3_len(dir);
    if (len < 1e-9f) return 0;
    efx_vec3 d = efx_v3_scale(dir, 1.0f / len);
    ray_acc acc = {out, 0, cap, all ? 1 : 0, max_distance};

    for (int i = 0; i < w->body_count; i++) {
        efx_pbody *b = &w->bodies[i];
        if (!b->alive) continue;
        if (b->sensor && !sensors) continue;
        if (!ray_mask_ok(b->layer, mask)) continue;
        if (b->shape.type == EFX_PHYS_SHAPE_MESH) {
            efx_aabb seg;
            efx_vec3 end = efx_v3_add(origin, efx_v3_scale(d, max_distance));
            seg.min = efx_v3_min(origin, end);
            seg.max = efx_v3_max(origin, end);
            seg = efx_aabb_expand(seg, EFX_PHYS_BROAD_MARGIN);
            efx_aabb local;
            local.min = efx_v3_sub(seg.min, b->position);
            local.max = efx_v3_sub(seg.max, b->position);
            ray_mesh_ctx ctx = {&acc, origin, d, b->position, max_distance,
                                b->id, b->sensor};
            efx_phys_mesh_query_aabb(b->shape.mesh, local, ray_mesh_tri_cb,
                                     &ctx);
        } else {
            float t;
            efx_vec3 n;
            if (efx_narrow_ray_shape(origin, d, &b->shape, b->position,
                                     max_distance, &t, &n)) {
                efx_ray_hit h;
                h.point = efx_v3_add(origin, efx_v3_scale(d, t));
                h.normal = n;
                h.distance = t;
                h.body = b->id;
                h.character = 0;
                h.sensor = b->sensor;
                ray_consider(&acc, &h);
            }
        }
    }
    for (int ci = 0; ci < w->char_count; ci++) {
        efx_pcharacter *ch = &w->chars[ci];
        if (!ch->alive) continue;
        if (!ray_mask_ok(ch->layer, mask)) continue;
        efx_shape cap_shape =
            efx_shape_capsule(ch->radius, ch->half_height * 2 + ch->radius * 2);
        float t;
        efx_vec3 n;
        if (efx_narrow_ray_shape(origin, d, &cap_shape, ch->position,
                                 max_distance, &t, &n)) {
            efx_ray_hit h;
            h.point = efx_v3_add(origin, efx_v3_scale(d, t));
            h.normal = n;
            h.distance = t;
            h.body = 0;
            h.character = ch->id;
            h.sensor = 0;
            ray_consider(&acc, &h);
        }
    }

    if (!all && acc.count == 1) return 1;
    if (all && acc.count > 1) {
        qsort(out, (size_t)acc.count, sizeof(efx_ray_hit), ray_hit_cmp);
    }
    return acc.count;
}

/* ---- overlap ---- */

typedef struct overlap_ctx {
    efx_physics_world *w;
    const efx_shape *shape;
    efx_vec3 pos;
    uint32_t mask;
    int sensors;
    efx_overlap_hit *out;
    int cap;
    int count;
} overlap_ctx;

static void overlap_add(overlap_ctx *ctx, efx_phys_body body,
                        efx_phys_character ch, int sensor) {
    if (ctx->out && ctx->count < ctx->cap) {
        ctx->out[ctx->count].body = body;
        ctx->out[ctx->count].character = ch;
        ctx->out[ctx->count].sensor = sensor;
    }
    ctx->count++;
}

typedef struct mesh_overlap_ctx {
    const efx_shape *shape;
    efx_vec3 pos;
    efx_vec3 offset;
    int hit;
} mesh_overlap_ctx;

static int mesh_overlap_cb(void *ud, const efx_phys_mesh *m, int tri) {
    mesh_overlap_ctx *c = ud;
    efx_vec3 v[3];
    efx_phys_mesh_tri(m, tri, v);
    for (int k = 0; k < 3; k++) v[k] = efx_v3_add(v[k], c->offset);
    efx_narrow_contact out;
    if (efx_narrow_shape_triangle(c->shape, c->pos, v[0], v[1], v[2], &out)) {
        c->hit = 1;
        return 1;
    }
    return 0;
}

/* tests an analytic shape against a triangle mesh collider (BVH accelerated) */
static int world_mesh_overlap(const efx_shape *shape, efx_vec3 pos,
                              const efx_phys_mesh *mesh, efx_vec3 offset,
                              efx_aabb qbounds) {
    if (!mesh) return 0;
    efx_aabb local;
    local.min = efx_v3_sub(qbounds.min, offset);
    local.max = efx_v3_sub(qbounds.max, offset);
    mesh_overlap_ctx ctx = {shape, pos, offset, 0};
    efx_phys_mesh_query_aabb(mesh, local, mesh_overlap_cb, &ctx);
    return ctx.hit;
}

int efx_physics_overlap(efx_physics_world *w, const efx_shape *shape,
                        efx_vec3 position, uint32_t mask, int sensors,
                        efx_overlap_hit *out, int cap) {
    if (!w || !shape) return 0;
    overlap_ctx ctx = {w, shape, position, mask, sensors, out, cap, 0};
    efx_aabb qbounds;
    efx_shape_bounds(shape, position, &qbounds);

    if (shape->type == EFX_PHYS_SHAPE_MESH) {
        /* the query is a mesh: test every collider's shape against its
         * triangles (mesh-vs-mesh falls back to an AABB overlap) */
        for (int i = 0; i < w->body_count; i++) {
            efx_pbody *b = &w->bodies[i];
            if (!b->alive) continue;
            if (b->sensor && !sensors) continue;
            if (!ray_mask_ok(b->layer, mask)) continue;
            if (!efx_aabb_overlap(b->aabb, qbounds)) continue;
            if (b->shape.type == EFX_PHYS_SHAPE_MESH) {
                overlap_add(&ctx, b->id, 0, b->sensor);
            } else if (world_mesh_overlap(&b->shape, b->position, shape->mesh,
                                          position, b->aabb)) {
                overlap_add(&ctx, b->id, 0, b->sensor);
            }
        }
        for (int ci = 0; ci < w->char_count; ci++) {
            efx_pcharacter *ch = &w->chars[ci];
            if (!ch->alive) continue;
            if (!ray_mask_ok(ch->layer, mask)) continue;
            efx_shape cap_shape = efx_shape_capsule(
                ch->radius, ch->half_height * 2 + ch->radius * 2);
            efx_aabb cb;
            efx_shape_bounds(&cap_shape, ch->position, &cb);
            if (efx_aabb_overlap(cb, qbounds) &&
                world_mesh_overlap(&cap_shape, ch->position, shape->mesh, position,
                                   cb)) {
                overlap_add(&ctx, 0, ch->id, 0);
            }
        }
        return ctx.count;
    }

    for (int i = 0; i < w->body_count; i++) {
        efx_pbody *b = &w->bodies[i];
        if (!b->alive) continue;
        if (b->sensor && !sensors) continue;
        if (!ray_mask_ok(b->layer, mask)) continue;
        if (b->shape.type == EFX_PHYS_SHAPE_MESH && b->shape.mesh) {
            if (world_mesh_overlap(shape, position, b->shape.mesh, b->position,
                                   qbounds)) {
                overlap_add(&ctx, b->id, 0, b->sensor);
            }
            continue;
        }
        efx_narrow_contact c;
        if (efx_narrow_overlap(shape, position, &b->shape, b->position, &c)) {
            overlap_add(&ctx, b->id, 0, b->sensor);
        }
    }
    for (int ci = 0; ci < w->char_count; ci++) {
        efx_pcharacter *ch = &w->chars[ci];
        if (!ch->alive) continue;
        if (!ray_mask_ok(ch->layer, mask)) continue;
        efx_shape cap_shape = efx_shape_capsule(
            ch->radius, ch->half_height * 2 + ch->radius * 2);
        efx_narrow_contact c;
        if (efx_narrow_overlap(shape, position, &cap_shape, ch->position, &c)) {
            overlap_add(&ctx, 0, ch->id, 0);
        }
    }
    return ctx.count;
}

/* ---- shape cast ---- */

typedef struct sweep_ctx {
    const efx_shape *shape;
    efx_vec3 from;
    efx_vec3 motion;
    float best_t;
    int found;
    efx_shape_hit *out;
    uint32_t mask;
    int sensors;
} sweep_ctx;

static void sweep_consider(sweep_ctx *ctx, float t, efx_vec3 point,
                           efx_vec3 normal, efx_phys_body body,
                           efx_phys_character ch, int sensor) {
    if (!ctx->found || t < ctx->best_t) {
        ctx->found = 1;
        ctx->best_t = t;
        ctx->out->point = point;
        ctx->out->normal = normal;
        ctx->out->fraction = t;
        ctx->out->body = body;
        ctx->out->character = ch;
        ctx->out->sensor = sensor;
    }
}

typedef struct sweep_mesh_ctx {
    sweep_ctx *sweep;
    efx_vec3 offset;
    efx_phys_body body;
    int sensor;
} sweep_mesh_ctx;

static int sweep_mesh_tri_cb(void *ud, const efx_phys_mesh *m, int tri) {
    sweep_mesh_ctx *mc = ud;
    efx_vec3 v[3];
    efx_phys_mesh_tri(m, tri, v);
    for (int k = 0; k < 3; k++) v[k] = efx_v3_add(v[k], mc->offset);
    float t;
    efx_vec3 point, normal;
    if (efx_narrow_sweep_triangle(mc->sweep->shape, mc->sweep->from,
                                  mc->sweep->motion, v[0], v[1], v[2], &t,
                                  &point, &normal)) {
        sweep_consider(mc->sweep, t, point, normal, mc->body, 0, mc->sensor);
    }
    return 0;
}

int efx_physics_shape_cast(efx_physics_world *w, const efx_shape *shape,
                           efx_vec3 from, efx_vec3 motion, uint32_t mask,
                           int sensors, efx_shape_hit *out) {
    if (!w || !shape || !out) return 0;
    efx_shape fallback;
    if (shape->type == EFX_PHYS_SHAPE_MESH && shape->mesh) {
        efx_aabb b;
        efx_phys_mesh_bounds(shape->mesh, &b);
        efx_vec3 e = efx_aabb_extent(b);
        float r = fmaxf(e.x, fmaxf(e.y, e.z));
        fallback = efx_shape_sphere(r > 0 ? r : 0.1f);
        shape = &fallback;
    }
    sweep_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.shape = shape;
    ctx.from = from;
    ctx.motion = motion;
    ctx.out = out;
    ctx.mask = mask;
    ctx.sensors = sensors;

    efx_aabb swept;
    efx_vec3 end = efx_v3_add(from, motion);
    swept.min = efx_v3_min(from, end);
    swept.max = efx_v3_max(from, end);
    efx_aabb shape_b;
    efx_shape_bounds(shape, from, &shape_b);
    efx_vec3 shrink = efx_v3_sub(from, shape_b.min);
    swept = efx_aabb_expand(swept, fmaxf(shrink.x, fmaxf(shrink.y, shrink.z)));
    swept = efx_aabb_expand(swept, EFX_PHYS_BROAD_MARGIN);

    for (int i = 0; i < w->body_count; i++) {
        efx_pbody *b = &w->bodies[i];
        if (!b->alive) continue;
        if (b->sensor && !sensors) continue;
        if (!ray_mask_ok(b->layer, mask)) continue;
        if (!efx_aabb_overlap(b->aabb, swept)) continue;
        if (b->shape.type == EFX_PHYS_SHAPE_MESH && b->shape.mesh) {
            efx_aabb local;
            local.min = efx_v3_sub(swept.min, b->position);
            local.max = efx_v3_sub(swept.max, b->position);
            sweep_mesh_ctx mc = {&ctx, b->position, b->id, b->sensor};
            efx_phys_mesh_query_aabb(b->shape.mesh, local, sweep_mesh_tri_cb,
                                     &mc);
        } else {
            float t;
            efx_vec3 point, normal;
            if (efx_narrow_sweep(shape, from, motion, &b->shape, b->position,
                                 &t, &point, &normal)) {
                sweep_consider(&ctx, t, point, normal, b->id, 0, b->sensor);
            }
        }
    }
    for (int ci = 0; ci < w->char_count; ci++) {
        efx_pcharacter *ch = &w->chars[ci];
        if (!ch->alive) continue;
        if (!ray_mask_ok(ch->layer, mask)) continue;
        efx_shape cap_shape = efx_shape_capsule(
            ch->radius, ch->half_height * 2 + ch->radius * 2);
        float t;
        efx_vec3 point, normal;
        if (efx_narrow_sweep(shape, from, motion, &cap_shape, ch->position, &t,
                             &point, &normal)) {
            sweep_consider(&ctx, t, point, normal, 0, ch->id, 0);
        }
    }
    return ctx.found;
}
