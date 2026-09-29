#include "physics/narrow.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

#define EPS 1e-6f

/* ---------------------------------------------------------- vector plumbing */

static inline float vget(efx_vec3 v, int i) { return efx_v3_get(v, i); }

static inline void vset(efx_vec3 *v, int i, float x) { efx_v3_set(v, i, x); }

static efx_vec3 tri_normal(efx_vec3 v0, efx_vec3 v1, efx_vec3 v2) {
    efx_vec3 n = efx_v3_cross(efx_v3_sub(v1, v0), efx_v3_sub(v2, v0));
    float l = efx_v3_len(n);
    if (l < EPS) return efx_v3(0, 1, 0);
    return efx_v3_scale(n, 1.0f / l);
}

efx_vec3 efx_narrow_closest_on_aabb(efx_vec3 p, efx_vec3 center, efx_vec3 half) {
    efx_vec3 lo = efx_v3_sub(center, half);
    efx_vec3 hi = efx_v3_add(center, half);
    return efx_v3(efx_clampf(p.x, lo.x, hi.x), efx_clampf(p.y, lo.y, hi.y),
                  efx_clampf(p.z, lo.z, hi.z));
}

efx_vec3 efx_narrow_closest_on_segment(efx_vec3 p, efx_vec3 a, efx_vec3 b) {
    efx_vec3 ab = efx_v3_sub(b, a);
    float denom = efx_v3_dot(ab, ab);
    if (denom < EPS) return a;
    float t = efx_v3_dot(efx_v3_sub(p, a), ab) / denom;
    t = efx_clampf(t, 0.0f, 1.0f);
    return efx_v3_add(a, efx_v3_scale(ab, t));
}

int efx_narrow_closest_on_triangle(efx_vec3 p, efx_vec3 v0, efx_vec3 v1,
                                   efx_vec3 v2, efx_vec3 *out) {
    efx_vec3 ab = efx_v3_sub(v1, v0);
    efx_vec3 ac = efx_v3_sub(v2, v0);
    efx_vec3 ap = efx_v3_sub(p, v0);
    float d1 = efx_v3_dot(ab, ap);
    float d2 = efx_v3_dot(ac, ap);
    if (d1 <= 0 && d2 <= 0) {
        if (out) *out = v0;
        return 1;
    }

    efx_vec3 bp = efx_v3_sub(p, v1);
    float d3 = efx_v3_dot(ab, bp);
    float d4 = efx_v3_dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) {
        if (out) *out = v1;
        return 1;
    }

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) {
        float v = d1 / (d1 - d3);
        if (out) *out = efx_v3_add(v0, efx_v3_scale(ab, v));
        return 1;
    }

    efx_vec3 cp = efx_v3_sub(p, v2);
    float d5 = efx_v3_dot(ab, cp);
    float d6 = efx_v3_dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) {
        if (out) *out = v2;
        return 1;
    }

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) {
        float w = d2 / (d2 - d6);
        if (out) *out = efx_v3_add(v0, efx_v3_scale(ac, w));
        return 1;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        if (out) *out = efx_v3_add(v1, efx_v3_scale(efx_v3_sub(v2, v1), w));
        return 1;
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    if (out) *out = efx_v3_add(v0, efx_v3_add(efx_v3_scale(ab, v),
                                              efx_v3_scale(ac, w)));
    return 1;
}

static int point_in_triangle(efx_vec3 p, efx_vec3 v0, efx_vec3 v1,
                             efx_vec3 v2) {
    efx_vec3 c = efx_v3_cross(efx_v3_sub(v1, v0), efx_v3_sub(p, v0));
    efx_vec3 e = efx_v3_cross(efx_v3_sub(v2, v0), efx_v3_sub(p, v0));
    efx_vec3 f = efx_v3_cross(efx_v3_sub(v1, v0), efx_v3_sub(v2, v0));
    /* all cross products share the face normal's sign (or are ~0) */
    efx_vec3 n = tri_normal(v0, v1, v2);
    return efx_v3_dot(c, n) >= -EPS && efx_v3_dot(e, n) >= -EPS &&
           efx_v3_dot(f, n) >= -EPS;
}

void efx_narrow_closest_segments(efx_vec3 p1, efx_vec3 q1, efx_vec3 p2,
                                 efx_vec3 q2, efx_vec3 *c1, efx_vec3 *c2) {
    efx_vec3 d1 = efx_v3_sub(q1, p1);
    efx_vec3 d2 = efx_v3_sub(q2, p2);
    efx_vec3 r = efx_v3_sub(p1, p2);
    float a = efx_v3_dot(d1, d1);
    float e = efx_v3_dot(d2, d2);
    float f = efx_v3_dot(d2, r);
    float s, t;

    if (a <= EPS && e <= EPS) {
        if (c1) *c1 = p1;
        if (c2) *c2 = p2;
        return;
    }
    if (a <= EPS) {
        s = 0;
        t = efx_clampf(f / e, 0.0f, 1.0f);
    } else {
        float c = efx_v3_dot(d1, r);
        if (e <= EPS) {
            t = 0;
            s = efx_clampf(-c / a, 0.0f, 1.0f);
        } else {
            float b = efx_v3_dot(d1, d2);
            float denom = a * e - b * b;
            if (denom > EPS) {
                s = efx_clampf((b * f - c * e) / denom, 0.0f, 1.0f);
            } else {
                s = 0;
            }
            t = (b * s + f) / e;
            if (t < 0) {
                t = 0;
                s = efx_clampf(-c / a, 0.0f, 1.0f);
            } else if (t > 1) {
                t = 1;
                s = efx_clampf((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    if (c1) *c1 = efx_v3_add(p1, efx_v3_scale(d1, s));
    if (c2) *c2 = efx_v3_add(p2, efx_v3_scale(d2, t));
}

/* -------------------------------------------------------- analytic overlaps */

int efx_narrow_sphere_sphere(efx_vec3 pa, float ra, efx_vec3 pb, float rb,
                             efx_narrow_contact *out) {
    efx_vec3 d = efx_v3_sub(pa, pb);
    float dist = efx_v3_len(d);
    float sum = ra + rb;
    if (dist >= sum) return 0;
    efx_vec3 n;
    if (dist < EPS) {
        n = efx_v3(0, 1, 0);
    } else {
        n = efx_v3_scale(d, 1.0f / dist);
    }
    out->normal = n;
    out->depth = sum - dist;
    out->point = efx_v3_sub(pa, efx_v3_scale(n, ra));
    return 1;
}

int efx_narrow_sphere_box(efx_vec3 pa, float ra, efx_vec3 pb, efx_vec3 hb,
                          efx_narrow_contact *out) {
    efx_vec3 closest = efx_narrow_closest_on_aabb(pa, pb, hb);
    efx_vec3 d = efx_v3_sub(pa, closest);
    float d2 = efx_v3_len_sq(d);
    if (d2 > ra * ra) return 0;
    if (d2 > EPS * EPS) {
        float dist = sqrtf(d2);
        out->normal = efx_v3_scale(d, 1.0f / dist);
        out->depth = ra - dist;
        out->point = closest;
        return 1;
    }
    /* center inside the box: push out along the nearest face */
    int axis = 0;
    float sign = 1;
    float best = FLT_MAX;
    for (int i = 0; i < 3; i++) {
        float h = vget(hb, i);
        float local = vget(pa, i) - vget(pb, i);
        float to_pos = h - local;
        float to_neg = h + local;
        if (to_pos < best) {
            best = to_pos;
            axis = i;
            sign = 1;
        }
        if (to_neg < best) {
            best = to_neg;
            axis = i;
            sign = -1;
        }
    }
    efx_vec3 n = efx_v3(0, 0, 0);
    vset(&n, axis, sign);
    out->normal = n;
    out->depth = ra + best;
    out->point = efx_v3_sub(pa, efx_v3_scale(n, best));
    return 1;
}

int efx_narrow_sphere_capsule(efx_vec3 pa, float ra, efx_vec3 pb, float hhb,
                              float rb, efx_narrow_contact *out) {
    efx_vec3 a = efx_v3(pb.x, pb.y - hhb, pb.z);
    efx_vec3 b = efx_v3(pb.x, pb.y + hhb, pb.z);
    efx_vec3 q = efx_narrow_closest_on_segment(pa, a, b);
    return efx_narrow_sphere_sphere(pa, ra, q, rb, out);
}

int efx_narrow_box_box(efx_vec3 pa, efx_vec3 ha, efx_vec3 pb, efx_vec3 hb,
                       efx_narrow_contact *out) {
    efx_vec3 d = efx_v3_sub(pa, pb);
    efx_vec3 overlap = efx_v3_sub(efx_v3_add(ha, hb), efx_v3_abs(d));
    if (overlap.x <= 0 || overlap.y <= 0 || overlap.z <= 0) return 0;
    int axis = 0;
    if (overlap.y < vget(overlap, axis)) axis = 1;
    if (overlap.z < vget(overlap, axis)) axis = 2;
    float depth = vget(overlap, axis);
    efx_vec3 n = efx_v3(0, 0, 0);
    vset(&n, axis, vget(d, axis) >= 0 ? 1.0f : -1.0f);
    out->normal = n;
    out->depth = depth;
    efx_vec3 lo = efx_v3_max(efx_v3_sub(pa, ha), efx_v3_sub(pb, hb));
    efx_vec3 hi = efx_v3_min(efx_v3_add(pa, ha), efx_v3_add(pb, hb));
    out->point = efx_v3_scale(efx_v3_add(lo, hi), 0.5f);
    return 1;
}

int efx_narrow_capsule_capsule(efx_vec3 pa, float hha, float ra, efx_vec3 pb,
                               float hhb, float rb, efx_narrow_contact *out) {
    efx_vec3 a0 = efx_v3(pa.x, pa.y - hha, pa.z);
    efx_vec3 a1 = efx_v3(pa.x, pa.y + hha, pa.z);
    efx_vec3 b0 = efx_v3(pb.x, pb.y - hhb, pb.z);
    efx_vec3 b1 = efx_v3(pb.x, pb.y + hhb, pb.z);
    efx_vec3 ca, cb;
    efx_narrow_closest_segments(a0, a1, b0, b1, &ca, &cb);
    return efx_narrow_sphere_sphere(ca, ra, cb, rb, out);
}

int efx_narrow_sphere_triangle(efx_vec3 pa, float ra, efx_vec3 v0, efx_vec3 v1,
                               efx_vec3 v2, efx_narrow_contact *out) {
    efx_vec3 q;
    efx_narrow_closest_on_triangle(pa, v0, v1, v2, &q);
    efx_vec3 d = efx_v3_sub(pa, q);
    float dist2 = efx_v3_len_sq(d);
    if (dist2 > ra * ra) return 0;
    efx_vec3 n;
    if (dist2 > EPS * EPS) {
        n = efx_v3_scale(d, 1.0f / sqrtf(dist2));
    } else {
        n = tri_normal(v0, v1, v2);
        if (efx_v3_dot(n, efx_v3_sub(pa, v0)) < 0) n = efx_v3_neg(n);
    }
    out->normal = n;
    out->depth = ra - sqrtf(dist2);
    out->point = q;
    return 1;
}

/* ---- AABB vs triangle using the 13-axis SAT (Akenine-Moller), returning the
 * minimum-translation normal/depth ---- */
int efx_narrow_box_triangle(efx_vec3 pa, efx_vec3 ha, efx_vec3 v0, efx_vec3 v1,
                            efx_vec3 v2, efx_narrow_contact *out) {
    efx_vec3 t0 = efx_v3_sub(v0, pa);
    efx_vec3 t1 = efx_v3_sub(v1, pa);
    efx_vec3 t2 = efx_v3_sub(v2, pa);
    efx_vec3 tverts[3] = {t0, t1, t2};
    efx_vec3 edges[3] = {efx_v3_sub(t1, t0), efx_v3_sub(t2, t1),
                         efx_v3_sub(t0, t2)};
    efx_vec3 tri_center =
        efx_v3_scale(efx_v3_add(t0, efx_v3_add(t1, t2)), 1.0f / 3.0f);

    float best_depth = FLT_MAX;
    efx_vec3 best_axis = efx_v3(0, 1, 0);
    int have = 0;

    /* test an axis (may be non-normalized): box interval is [-r, r] */
    #define TEST_AXIS(AX)                                                       \
        do {                                                                    \
            efx_vec3 a_ = (AX);                                                 \
            float len_ = efx_v3_len(a_);                                        \
            if (len_ > EPS) {                                                   \
                float r_ = ha.x * fabsf(a_.x) + ha.y * fabsf(a_.y) +            \
                           ha.z * fabsf(a_.z);                                  \
                float tmin_ = FLT_MAX, tmax_ = -FLT_MAX;                        \
                for (int k_ = 0; k_ < 3; k_++) {                                \
                    float p_ = efx_v3_dot(tverts[k_], a_);                      \
                    if (p_ < tmin_) tmin_ = p_;                                 \
                    if (p_ > tmax_) tmax_ = p_;                                 \
                }                                                               \
                /* minimum translation to separate the box interval [-r,r]      \
                 * from the triangle interval [tmin,tmax] (handles containment) */ \
                float o_ = fminf(r_ - tmin_, tmax_ + r_);                       \
                if (o_ <= 0) return 0;                                          \
                float depth_ = o_ / len_;                                       \
                if (depth_ < best_depth) {                                      \
                    best_depth = depth_;                                        \
                    best_axis = efx_v3_scale(a_, 1.0f / len_);                   \
                    have = 1;                                                   \
                }                                                               \
            }                                                                   \
        } while (0)

    TEST_AXIS(efx_v3(1, 0, 0));
    TEST_AXIS(efx_v3(0, 1, 0));
    TEST_AXIS(efx_v3(0, 0, 1));
    TEST_AXIS(efx_v3_cross(edges[0], edges[1]));
    for (int i = 0; i < 3; i++) {
        efx_vec3 e = edges[i];
        TEST_AXIS(efx_v3_cross(e, efx_v3(1, 0, 0)));
        TEST_AXIS(efx_v3_cross(e, efx_v3(0, 1, 0)));
        TEST_AXIS(efx_v3_cross(e, efx_v3(0, 0, 1)));
    }
    #undef TEST_AXIS

    if (!have) return 0;
    /* orient the normal from the triangle toward the box (box center = origin,
     * so the box is on the side opposite the triangle centroid) */
    efx_vec3 n = best_axis;
    if (efx_v3_dot(n, tri_center) > 0) n = efx_v3_neg(n);
    out->normal = n;
    out->depth = best_depth;
    efx_vec3 q;
    efx_narrow_closest_on_triangle(pa, v0, v1, v2, &q);
    out->point = q;
    return 1;
}

/* closest distance between a vertical capsule segment and a triangle */
static float segment_triangle_closest(efx_vec3 a, efx_vec3 b, efx_vec3 v0,
                                      efx_vec3 v1, efx_vec3 v2, efx_vec3 *pseg,
                                      efx_vec3 *ptri) {
    float best = FLT_MAX;
    efx_vec3 bs = a, bt = v0;
    efx_vec3 q;

    efx_narrow_closest_on_triangle(a, v0, v1, v2, &q);
    float d = efx_v3_dist_sq(a, q);
    if (d < best) { best = d; bs = a; bt = q; }
    efx_narrow_closest_on_triangle(b, v0, v1, v2, &q);
    d = efx_v3_dist_sq(b, q);
    if (d < best) { best = d; bs = b; bt = q; }

    efx_vec3 tri[4] = {v0, v1, v2, v0};
    for (int i = 0; i < 3; i++) {
        efx_vec3 cs, ct;
        efx_narrow_closest_segments(a, b, tri[i], tri[i + 1], &cs, &ct);
        d = efx_v3_dist_sq(cs, ct);
        if (d < best) { best = d; bs = cs; bt = ct; }
    }

    /* segment crossing the triangle plane inside the triangle -> distance 0 */
    efx_vec3 n = tri_normal(v0, v1, v2);
    float da = efx_v3_dot(efx_v3_sub(a, v0), n);
    float db = efx_v3_dot(efx_v3_sub(b, v0), n);
    if (da * db < 0) {
        float t = da / (da - db);
        efx_vec3 p = efx_v3_lerp(a, b, t);
        if (point_in_triangle(p, v0, v1, v2)) {
            best = 0;
            bs = p;
            bt = p;
        }
    }
    if (pseg) *pseg = bs;
    if (ptri) *ptri = bt;
    return sqrtf(best);
}

int efx_narrow_capsule_triangle(efx_vec3 ca, float hh, float ra, efx_vec3 v0,
                                efx_vec3 v1, efx_vec3 v2,
                                efx_narrow_contact *out) {
    efx_vec3 a = efx_v3(ca.x, ca.y - hh, ca.z);
    efx_vec3 b = efx_v3(ca.x, ca.y + hh, ca.z);
    efx_vec3 pseg, ptri;
    float dist = segment_triangle_closest(a, b, v0, v1, v2, &pseg, &ptri);
    if (dist >= ra) return 0;
    efx_vec3 n;
    if (dist > EPS) {
        n = efx_v3_scale(efx_v3_sub(pseg, ptri), 1.0f / dist);
    } else {
        n = tri_normal(v0, v1, v2);
        if (efx_v3_dot(n, efx_v3_sub(ca, v0)) < 0) n = efx_v3_neg(n);
    }
    out->normal = n;
    out->depth = ra - dist;
    out->point = ptri;
    return 1;
}

/* ------------------------------------------------------- generic dispatch */

/* capsule vs axis-aligned box; the normal pushes the capsule away from the
 * box (alternating projection finds the closest segment/box feature pair). */
static int capsule_box_contact(efx_vec3 cap, float hh, float r, efx_vec3 bc,
                               efx_vec3 bh, efx_narrow_contact *out) {
    efx_vec3 a = efx_v3(cap.x, cap.y - hh, cap.z);
    efx_vec3 b = efx_v3(cap.x, cap.y + hh, cap.z);
    efx_vec3 spt = efx_narrow_closest_on_segment(bc, a, b);
    for (int it = 0; it < 12; it++) {
        efx_vec3 bp = efx_narrow_closest_on_aabb(spt, bc, bh);
        efx_vec3 np = efx_narrow_closest_on_segment(bp, a, b);
        if (efx_v3_dist_sq(np, spt) < 1e-14f) {
            spt = np;
            break;
        }
        spt = np;
    }
    efx_vec3 bpt = efx_narrow_closest_on_aabb(spt, bc, bh);
    efx_vec3 d = efx_v3_sub(spt, bpt);
    float dist = efx_v3_len(d);
    if (dist >= r) return 0;
    if (dist > EPS) {
        out->normal = efx_v3_scale(d, 1.0f / dist);
    } else {
        /* degenerate (the segment intersects the box): pick the least
         * separating axis. A capsule whose center sits at the box's height
         * pushes horizontally (a character shoving a crate), so the vertical
         * axis only participates when the capsule is clearly above/below. */
        int axis = 0;
        float sign = 1, best = FLT_MAX;
        for (int i = 0; i < 3; i++) {
            if (i == 1) {
                if (cap.y > bc.y + bh.y) {
                    float d = cap.y - (bc.y + bh.y);
                    if (d < best) { best = d; axis = 1; sign = 1; }
                } else if (cap.y < bc.y - bh.y) {
                    float d = (bc.y - bh.y) - cap.y;
                    if (d < best) { best = d; axis = 1; sign = -1; }
                }
                continue;
            }
            float local = vget(cap, i) - vget(bc, i);
            float h = vget(bh, i);
            if (h - local < best) { best = h - local; axis = i; sign = 1; }
            if (h + local < best) { best = h + local; axis = i; sign = -1; }
        }
        out->normal = efx_v3(0, 0, 0);
        vset(&out->normal, axis, sign);
    }
    out->depth = r - dist;
    out->point = bpt;
    return 1;
}

int efx_narrow_overlap(const efx_shape *a, efx_vec3 pa, const efx_shape *b,
                       efx_vec3 pb, efx_narrow_contact *out) {
    switch (a->type) {
    case EFX_PHYS_SHAPE_SPHERE:
        switch (b->type) {
        case EFX_PHYS_SHAPE_SPHERE:
            return efx_narrow_sphere_sphere(pa, a->radius, pb, b->radius, out);
        case EFX_PHYS_SHAPE_BOX:
            return efx_narrow_sphere_box(pa, a->radius, pb, b->half, out);
        case EFX_PHYS_SHAPE_CAPSULE:
            return efx_narrow_sphere_capsule(pa, a->radius, pb,
                                             b->half_height, b->radius, out);
        default:
            return 0;
        }
    case EFX_PHYS_SHAPE_BOX:
        switch (b->type) {
        case EFX_PHYS_SHAPE_SPHERE: {
            efx_narrow_contact c;
            if (!efx_narrow_sphere_box(pb, b->radius, pa, a->half, &c)) return 0;
            c.normal = efx_v3_neg(c.normal);
            *out = c;
            return 1;
        }
        case EFX_PHYS_SHAPE_BOX:
            return efx_narrow_box_box(pa, a->half, pb, b->half, out);
        case EFX_PHYS_SHAPE_CAPSULE:
            if (!capsule_box_contact(pb, b->half_height, b->radius, pa, a->half,
                                     out)) {
                return 0;
            }
            out->normal = efx_v3_neg(out->normal);
            return 1;
        default:
            return 0;
        }
    case EFX_PHYS_SHAPE_CAPSULE:
        switch (b->type) {
        case EFX_PHYS_SHAPE_SPHERE:
            return efx_narrow_sphere_capsule(pb, b->radius, pa, a->half_height,
                                             a->radius, out);
        case EFX_PHYS_SHAPE_BOX:
            return capsule_box_contact(pa, a->half_height, a->radius, pb,
                                       b->half, out);
        case EFX_PHYS_SHAPE_CAPSULE:
            return efx_narrow_capsule_capsule(pa, a->half_height, a->radius, pb,
                                              b->half_height, b->radius, out);
        default:
            return 0;
        }
    default:
        return 0;
    }
}

int efx_narrow_shape_triangle(const efx_shape *a, efx_vec3 pa, efx_vec3 v0,
                              efx_vec3 v1, efx_vec3 v2,
                              efx_narrow_contact *out) {
    switch (a->type) {
    case EFX_PHYS_SHAPE_SPHERE:
        return efx_narrow_sphere_triangle(pa, a->radius, v0, v1, v2, out);
    case EFX_PHYS_SHAPE_BOX:
        return efx_narrow_box_triangle(pa, a->half, v0, v1, v2, out);
    case EFX_PHYS_SHAPE_CAPSULE:
        return efx_narrow_capsule_triangle(pa, a->half_height, a->radius, v0,
                                           v1, v2, out);
    default:
        return 0;
    }
}

/* ------------------------------------------------------------- ray casts */

int efx_narrow_ray_sphere(efx_vec3 o, efx_vec3 d, efx_vec3 c, float r,
                          float maxd, float *t, efx_vec3 *n) {
    efx_vec3 oc = efx_v3_sub(o, c);
    float a = efx_v3_dot(d, d);
    float b = 2 * efx_v3_dot(oc, d);
    float cc = efx_v3_dot(oc, oc) - r * r;
    float disc = b * b - 4 * a * cc;
    if (disc < 0 || a < EPS) return 0;
    float sq = sqrtf(disc);
    float t0 = (-b - sq) / (2 * a);
    float t1 = (-b + sq) / (2 * a);
    float hit = t0 >= 0 ? t0 : t1;
    if (hit < 0 || hit > maxd) return 0;
    efx_vec3 p = efx_v3_add(o, efx_v3_scale(d, hit));
    *t = hit;
    *n = efx_v3_scale(efx_v3_sub(p, c), 1.0f / r);
    return 1;
}

int efx_narrow_ray_box(efx_vec3 o, efx_vec3 d, efx_vec3 c, efx_vec3 h,
                       float maxd, float *t, efx_vec3 *n) {
    efx_aabb box = {efx_v3_sub(c, h), efx_v3_add(c, h)};
    float tmin, tmax;
    if (!efx_ray_aabb(o, d, box, &tmin, &tmax)) return 0;
    float hit = tmin >= 0 ? tmin : tmax;
    if (hit < 0 || hit > maxd) return 0;
    efx_vec3 p = efx_v3_add(o, efx_v3_scale(d, hit));
    efx_vec3 local = efx_v3_sub(p, c);
    efx_vec3 normal = efx_v3(0, 0, 0);
    float best = FLT_MAX;
    for (int i = 0; i < 3; i++) {
        float hi = vget(h, i);
        float li = vget(local, i);
        float dp = hi - li;
        float dn = hi + li;
        if (dp < best) {
            best = dp;
            normal = efx_v3(0, 0, 0);
            vset(&normal, i, 1);
        }
        if (dn < best) {
            best = dn;
            normal = efx_v3(0, 0, 0);
            vset(&normal, i, -1);
        }
    }
    *t = hit;
    *n = normal;
    return 1;
}

int efx_narrow_ray_capsule(efx_vec3 o, efx_vec3 d, efx_vec3 c, float hh,
                           float r, float maxd, float *t, efx_vec3 *n) {
    /* vertical cylinder in XZ, then the two caps as spheres */
    float best = maxd + 1;
    efx_vec3 bestn = efx_v3(0, 0, 0);
    int found = 0;

    float ox = o.x - c.x, oz = o.z - c.z;
    float A = d.x * d.x + d.z * d.z;
    if (A > EPS) {
        float B = 2 * (ox * d.x + oz * d.z);
        float C = ox * ox + oz * oz - r * r;
        float disc = B * B - 4 * A * C;
        if (disc >= 0) {
            float sq = sqrtf(disc);
            float ts[2] = {(-B - sq) / (2 * A), (-B + sq) / (2 * A)};
            for (int k = 0; k < 2; k++) {
                float tt = ts[k];
                if (tt < 0 || tt > maxd || tt >= best) continue;
                float y = o.y + d.y * tt;
                if (y >= c.y - hh && y <= c.y + hh) {
                    efx_vec3 p = efx_v3_add(o, efx_v3_scale(d, tt));
                    efx_vec3 nn = efx_v3(p.x - c.x, 0, p.z - c.z);
                    float nl = efx_v3_len(nn);
                    if (nl > EPS) {
                        best = tt;
                        bestn = efx_v3_scale(nn, 1.0f / nl);
                        found = 1;
                    }
                }
            }
        }
    }
    efx_vec3 caps[2] = {efx_v3(c.x, c.y - hh, c.z), efx_v3(c.x, c.y + hh, c.z)};
    for (int k = 0; k < 2; k++) {
        float tt;
        efx_vec3 nn;
        if (efx_narrow_ray_sphere(o, d, caps[k], r, maxd, &tt, &nn) &&
            tt < best) {
            best = tt;
            bestn = nn;
            found = 1;
        }
    }
    if (!found) return 0;
    *t = best;
    *n = bestn;
    return 1;
}

int efx_narrow_ray_triangle(efx_vec3 o, efx_vec3 d, efx_vec3 v0, efx_vec3 v1,
                            efx_vec3 v2, float maxd, float *t, efx_vec3 *n) {
    efx_vec3 e1 = efx_v3_sub(v1, v0);
    efx_vec3 e2 = efx_v3_sub(v2, v0);
    efx_vec3 pv = efx_v3_cross(d, e2);
    float det = efx_v3_dot(e1, pv);
    if (fabsf(det) < EPS) return 0;
    float inv = 1.0f / det;
    efx_vec3 tv = efx_v3_sub(o, v0);
    float u = efx_v3_dot(tv, pv) * inv;
    if (u < -EPS || u > 1 + EPS) return 0;
    efx_vec3 qv = efx_v3_cross(tv, e1);
    float v = efx_v3_dot(d, qv) * inv;
    if (v < -EPS || u + v > 1 + EPS) return 0;
    float tt = efx_v3_dot(e2, qv) * inv;
    if (tt < 0 || tt > maxd) return 0;
    efx_vec3 nn = tri_normal(v0, v1, v2);
    if (efx_v3_dot(nn, d) > 0) nn = efx_v3_neg(nn);
    *t = tt;
    *n = nn;
    return 1;
}

int efx_narrow_ray_shape(efx_vec3 o, efx_vec3 d, const efx_shape *s,
                         efx_vec3 pos, float maxd, float *t, efx_vec3 *n) {
    switch (s->type) {
    case EFX_PHYS_SHAPE_SPHERE:
        return efx_narrow_ray_sphere(o, d, pos, s->radius, maxd, t, n);
    case EFX_PHYS_SHAPE_BOX:
        return efx_narrow_ray_box(o, d, pos, s->half, maxd, t, n);
    case EFX_PHYS_SHAPE_CAPSULE:
        return efx_narrow_ray_capsule(o, d, pos, s->half_height, s->radius,
                                      maxd, t, n);
    default:
        return 0;
    }
}

/* ------------------------------------------------ closest distance helpers */

#define NT_SPHERE 0
#define NT_BOX 1
#define NT_CAPSULE 2
#define NT_TRIANGLE 3

static float box_segment_closest(efx_vec3 bc, efx_vec3 bh, efx_vec3 a,
                                 efx_vec3 b, efx_vec3 *qbox, efx_vec3 *qseg) {
    efx_vec3 spt = efx_narrow_closest_on_segment(bc, a, b);
    for (int it = 0; it < 12; it++) {
        efx_vec3 bpt = efx_narrow_closest_on_aabb(spt, bc, bh);
        efx_vec3 npt = efx_narrow_closest_on_segment(bpt, a, b);
        if (efx_v3_dist_sq(npt, spt) < 1e-14f) {
            spt = npt;
            break;
        }
        spt = npt;
    }
    efx_vec3 bpt = efx_narrow_closest_on_aabb(spt, bc, bh);
    if (qbox) *qbox = bpt;
    if (qseg) *qseg = spt;
    return efx_v3_dist(bpt, spt);
}

static float box_box_closest(efx_vec3 ca, efx_vec3 ha, efx_vec3 cb,
                             efx_vec3 hb, efx_vec3 *qa, efx_vec3 *qb) {
    efx_vec3 d = efx_v3_sub(ca, cb);
    efx_vec3 gap = efx_v3_abs(d);
    gap = efx_v3_sub(gap, efx_v3_add(ha, hb));
    gap = efx_v3_max(gap, efx_v3(0, 0, 0));
    efx_vec3 pa = ca, pb = cb;
    for (int i = 0; i < 3; i++) {
        float sep = vget(d, i);
        float sum = vget(ha, i) + vget(hb, i);
        if (sep > sum) {
            vset(&pa, i, vget(ca, i) - vget(ha, i));
            vset(&pb, i, vget(cb, i) + vget(hb, i));
        } else if (sep < -sum) {
            vset(&pa, i, vget(ca, i) + vget(ha, i));
            vset(&pb, i, vget(cb, i) - vget(hb, i));
        } else {
            float mid = efx_clampf((vget(ca, i) + vget(cb, i)) * 0.5f,
                                   vget(cb, i) - vget(hb, i),
                                   vget(cb, i) + vget(hb, i));
            vset(&pa, i, mid);
            vset(&pb, i, mid);
        }
    }
    if (qa) *qa = pa;
    if (qb) *qb = pb;
    return efx_v3_dist(pa, pb);
}

static float box_triangle_closest(efx_vec3 bc, efx_vec3 bh, efx_vec3 v0,
                                  efx_vec3 v1, efx_vec3 v2, efx_vec3 *qbox,
                                  efx_vec3 *qtri) {
    efx_vec3 tpt;
    efx_narrow_closest_on_triangle(bc, v0, v1, v2, &tpt);
    efx_vec3 bpt = efx_narrow_closest_on_aabb(tpt, bc, bh);
    for (int it = 0; it < 12; it++) {
        efx_vec3 nt;
        efx_narrow_closest_on_triangle(bpt, v0, v1, v2, &nt);
        efx_vec3 nb = efx_narrow_closest_on_aabb(nt, bc, bh);
        if (efx_v3_dist_sq(nb, bpt) < 1e-14f) {
            bpt = nb;
            tpt = nt;
            break;
        }
        bpt = nb;
        tpt = nt;
    }
    if (qbox) *qbox = bpt;
    if (qtri) *qtri = tpt;
    return efx_v3_dist(bpt, tpt);
}

/* closest core-features between A (at pa) and B (other descriptor at pb or a
 * triangle). Returns the raw core distance (no radii subtracted). */
static float feature_closest(const efx_shape *s, efx_vec3 pos, int otype,
                             efx_vec3 oc, efx_vec3 oh, float ohh, efx_vec3 v0,
                             efx_vec3 v1, efx_vec3 v2, efx_vec3 *out_s,
                             efx_vec3 *out_o) {
    if (s->type == EFX_PHYS_SHAPE_SPHERE) {
        efx_vec3 p = pos;
        efx_vec3 q;
        if (otype == NT_SPHERE) {
            q = oc;
        } else if (otype == NT_BOX) {
            q = efx_narrow_closest_on_aabb(p, oc, oh);
        } else if (otype == NT_CAPSULE) {
            q = efx_narrow_closest_on_segment(
                p, efx_v3(oc.x, oc.y - ohh, oc.z),
                efx_v3(oc.x, oc.y + ohh, oc.z));
        } else {
            efx_narrow_closest_on_triangle(p, v0, v1, v2, &q);
        }
        if (out_s) *out_s = p;
        if (out_o) *out_o = q;
        return efx_v3_dist(p, q);
    }
    if (s->type == EFX_PHYS_SHAPE_BOX) {
        if (otype == NT_SPHERE) {
            efx_vec3 q = efx_narrow_closest_on_aabb(oc, pos, s->half);
            if (out_s) *out_s = q;
            if (out_o) *out_o = oc;
            return efx_v3_dist(q, oc);
        }
        if (otype == NT_BOX) {
            return box_box_closest(pos, s->half, oc, oh, out_s, out_o);
        }
        if (otype == NT_CAPSULE) {
            return box_segment_closest(pos, s->half, efx_v3(oc.x, oc.y - ohh, oc.z),
                                       efx_v3(oc.x, oc.y + ohh, oc.z), out_s,
                                       out_o);
        }
        return box_triangle_closest(pos, s->half, v0, v1, v2, out_s, out_o);
    }
    /* capsule */
    efx_vec3 a = efx_v3(pos.x, pos.y - s->half_height, pos.z);
    efx_vec3 b = efx_v3(pos.x, pos.y + s->half_height, pos.z);
    if (otype == NT_SPHERE) {
        efx_vec3 q = efx_narrow_closest_on_segment(oc, a, b);
        if (out_s) *out_s = q;
        if (out_o) *out_o = oc;
        return efx_v3_dist(q, oc);
    }
    if (otype == NT_BOX) {
        return box_segment_closest(oc, oh, a, b, out_o, out_s);
    }
    if (otype == NT_CAPSULE) {
        efx_vec3 cs, co;
        efx_narrow_closest_segments(a, b, efx_v3(oc.x, oc.y - ohh, oc.z),
                                    efx_v3(oc.x, oc.y + ohh, oc.z), &cs, &co);
        if (out_s) *out_s = cs;
        if (out_o) *out_o = co;
        return efx_v3_dist(cs, co);
    }
    if (out_s || out_o) {
        efx_vec3 ps, pt;
        float d = segment_triangle_closest(a, b, v0, v1, v2, &ps, &pt);
        if (out_s) *out_s = ps;
        if (out_o) *out_o = pt;
        return d;
    }
    return segment_triangle_closest(a, b, v0, v1, v2, NULL, NULL);
}

static float shape_radius(const efx_shape *s) {
    return (s->type == EFX_PHYS_SHAPE_SPHERE || s->type == EFX_PHYS_SHAPE_CAPSULE)
               ? s->radius
               : 0.0f;
}

float efx_narrow_distance(const efx_shape *a, efx_vec3 pa, const efx_shape *b,
                          efx_vec3 pb, efx_vec3 *pa_out, efx_vec3 *pb_out) {
    efx_vec3 qa, qb;
    float d = feature_closest(a, pa, b->type, pb, b->half, b->half_height,
                              efx_v3(0, 0, 0), efx_v3(0, 0, 0),
                              efx_v3(0, 0, 0), &qa, &qb);
    if (pa_out) *pa_out = qa;
    if (pb_out) *pb_out = qb;
    return d - shape_radius(a) - shape_radius(b);
}

float efx_narrow_distance_triangle(const efx_shape *a, efx_vec3 pa,
                                   efx_vec3 v0, efx_vec3 v1, efx_vec3 v2,
                                   efx_vec3 *pa_out, efx_vec3 *pb_out) {
    efx_vec3 qa, qb;
    float d = feature_closest(a, pa, NT_TRIANGLE, efx_v3(0, 0, 0),
                              efx_v3(0, 0, 0), 0, v0, v1, v2, &qa, &qb);
    if (pa_out) *pa_out = qa;
    if (pb_out) *pb_out = qb;
    return d - shape_radius(a);
}

/* ------------------------------------------------------------------ sweeps */

int efx_narrow_sweep(const efx_shape *a, efx_vec3 from, efx_vec3 motion,
                     const efx_shape *b, efx_vec3 pb, float *t, efx_vec3 *point,
                     efx_vec3 *normal) {
    /* subtract B's radius by pre-expanding via distance: handled inside
     * feature_closest; use a wrapper that accounts for both radii */
    float len = efx_v3_len(motion);
    if (len < EPS) return 0;
    float travel = 0;
    const float margin = 1e-4f;
    for (int it = 0; it < 128; it++) {
        efx_vec3 pos = efx_v3_add(from, efx_v3_scale(motion, travel));
        efx_vec3 qa, qb;
        float d = efx_narrow_distance(a, pos, b, pb, &qa, &qb);
        if (d <= margin) {
            if (point) *point = qa;
            if (normal) {
                efx_vec3 n = efx_v3_sub(qa, qb);
                float nl = efx_v3_len(n);
                if (nl > EPS) {
                    n = efx_v3_scale(n, 1.0f / nl);
                } else {
                    n = efx_v3_neg(efx_v3_scale(motion, 1.0f / len));
                }
                *normal = n;
            }
            if (t) *t = efx_clampf(travel, 0, 1);
            return 1;
        }
        travel += d / len;
        if (travel > 1.0f) return 0;
    }
    return 0;
}

int efx_narrow_sweep_triangle(const efx_shape *a, efx_vec3 from, efx_vec3 motion,
                              efx_vec3 v0, efx_vec3 v1, efx_vec3 v2, float *t,
                              efx_vec3 *point, efx_vec3 *normal) {
    float len = efx_v3_len(motion);
    if (len < EPS) return 0;
    float travel = 0;
    const float margin = 1e-4f;
    for (int it = 0; it < 128; it++) {
        efx_vec3 pos = efx_v3_add(from, efx_v3_scale(motion, travel));
        efx_vec3 qa, qb;
        float d = efx_narrow_distance_triangle(a, pos, v0, v1, v2, &qa, &qb);
        if (d <= margin) {
            if (point) *point = qa;
            if (normal) {
                efx_vec3 n = efx_v3_sub(qa, qb);
                float nl = efx_v3_len(n);
                if (nl > EPS) {
                    n = efx_v3_scale(n, 1.0f / nl);
                } else {
                    n = efx_v3_neg(efx_v3_scale(motion, 1.0f / len));
                }
                *normal = n;
            }
            if (t) *t = efx_clampf(travel, 0, 1);
            return 1;
        }
        travel += d / len;
        if (travel > 1.0f) return 0;
    }
    return 0;
}
