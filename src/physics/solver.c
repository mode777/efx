#include "physics/world.h"

#include <math.h>

static void contact_velocity(const efx_physics_world *w, int type, int index,
                             efx_vec3 *v, float *inv) {
    if (type == 0) {
        *v = w->bodies[index].velocity;
        *inv = w->bodies[index].inv_mass;
    } else {
        *v = w->chars[index].velocity;
        *inv = 0;
    }
}

static void contact_add_velocity(efx_physics_world *w, int type, int index,
                                 efx_vec3 dv) {
    if (type == 0) {
        w->bodies[index].velocity =
            efx_v3_add(w->bodies[index].velocity, dv);
    }
    /* characters are immovable during a step: velocity is never applied */
}

void efx_solver_solve(efx_physics_world *w, float dt) {
    if (dt <= 0) return;
    const float inv_dt = 1.0f / dt;

    /* restitution is computed once from the pre-solve approach velocity so the
     * target separating speed is fixed; recomputing it per iteration would let
     * later iterations cancel the bounce */
    for (int i = 0; i < w->pair_count; i++) {
        efx_contact_pair *p = &w->pairs[i];
        p->bounce = 0;
        if (p->sensor) continue;
        efx_vec3 va, vb;
        float inva, invb;
        contact_velocity(w, p->a_type, p->a_index, &va, &inva);
        contact_velocity(w, p->b_type, p->b_index, &vb, &invb);
        if (inva + invb <= 0) continue;
        float vn = efx_v3_dot(efx_v3_sub(va, vb), p->normal);
        if (vn < -EFX_PHYS_RESTITUTION_THRESHOLD) {
            p->bounce = -p->restitution * vn;
        }
    }

    for (int iter = 0; iter < w->iterations; iter++) {
        for (int i = 0; i < w->pair_count; i++) {
            efx_contact_pair *p = &w->pairs[i];
            if (p->sensor) continue;

            efx_vec3 va, vb;
            float inva, invb;
            contact_velocity(w, p->a_type, p->a_index, &va, &inva);
            contact_velocity(w, p->b_type, p->b_index, &vb, &invb);
            float k = inva + invb;
            if (k <= 0) continue;

            efx_vec3 n = p->normal;
            efx_vec3 rel = efx_v3_sub(va, vb);
            float vn = efx_v3_dot(rel, n);

            float bias = EFX_PHYS_BAUMGARTE * inv_dt *
                         fmaxf(p->depth - EFX_PHYS_PEN_SLOP, 0.0f);

            float j = (p->bounce - vn + bias) / k;
            float old = p->normal_impulse;
            p->normal_impulse = fmaxf(old + j, 0.0f);
            j = p->normal_impulse - old;
            contact_add_velocity(w, p->a_type, p->a_index, efx_v3_scale(n, j * inva));
            contact_add_velocity(w, p->b_type, p->b_index,
                                 efx_v3_scale(n, -j * invb));

            /* friction along the tangent, clamped by the accumulated normal
             * impulse times the combined coefficient */
            contact_velocity(w, p->a_type, p->a_index, &va, &inva);
            contact_velocity(w, p->b_type, p->b_index, &vb, &invb);
            rel = efx_v3_sub(va, vb);
            efx_vec3 vt = efx_v3_sub(rel, efx_v3_scale(n, efx_v3_dot(rel, n)));
            float tl = efx_v3_len(vt);
            if (tl > 1e-6f) {
                efx_vec3 t = efx_v3_scale(vt, 1.0f / tl);
                float jt = -efx_v3_dot(rel, t) / k;
                float maxf = p->friction * p->normal_impulse;
                float oldt = p->tangent_impulse;
                p->tangent_impulse = efx_clampf(oldt + jt, -maxf, maxf);
                jt = p->tangent_impulse - oldt;
                contact_add_velocity(w, p->a_type, p->a_index,
                                     efx_v3_scale(t, jt * inva));
                contact_add_velocity(w, p->b_type, p->b_index,
                                     efx_v3_scale(t, -jt * invb));
            }
        }
    }

    /* positional correction with slop: removes residual penetration without
     * adding the energy a pure Baumgarte bias would (ADR 0040) */
    for (int i = 0; i < w->pair_count; i++) {
        efx_contact_pair *p = &w->pairs[i];
        if (p->sensor) continue;
        if (p->depth <= EFX_PHYS_PEN_SLOP) continue;
        float inva = p->a_type == 0 ? w->bodies[p->a_index].inv_mass : 0;
        float invb = p->b_type == 0 ? w->bodies[p->b_index].inv_mass : 0;
        float k = inva + invb;
        if (k <= 0) continue;
        float corr =
            fminf((p->depth - EFX_PHYS_PEN_SLOP) * 0.9f, EFX_PHYS_MAX_CORRECTION);
        efx_vec3 push = efx_v3_scale(p->normal, corr / k);
        if (p->a_type == 0) {
            w->bodies[p->a_index].position =
                efx_v3_add(w->bodies[p->a_index].position,
                           efx_v3_scale(push, inva));
        }
        if (p->b_type == 0) {
            w->bodies[p->b_index].position =
                efx_v3_sub(w->bodies[p->b_index].position,
                           efx_v3_scale(push, invb));
        }
    }
}
