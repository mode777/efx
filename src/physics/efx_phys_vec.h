#ifndef EFX_PHYS_VEC_H
#define EFX_PHYS_VEC_H

/*
 * F12 physics core: its own tiny vector math (design D1).
 *
 * The collision/dynamics core MUST stay dependency-free — no renderer, no
 * platform layer, no script runtime, and crucially no src/math (GLM/C++), so a
 * plain C test binary can build it in isolation. This header is the whole math
 * surface; everything is `static inline` so including it costs nothing.
 *
 * Conventions: right-handed. Capsules and boxes are axis-aligned / vertical;
 * the core is linear-only and never needs a rotation.
 */

#include <math.h>
#include <stddef.h>

#define EFX_PI 3.14159265358979323846f

typedef struct efx_vec3 {
    float x, y, z;
} efx_vec3;

static inline efx_vec3 efx_v3(float x, float y, float z) {
    efx_vec3 v = {x, y, z};
    return v;
}

/* component access by index (defined without out-of-struct pointer arithmetic,
 * which optimizers may treat as undefined) */
static inline float efx_v3_get(efx_vec3 v, int i) {
    if (i == 0) return v.x;
    if (i == 1) return v.y;
    return v.z;
}

static inline void efx_v3_set(efx_vec3 *v, int i, float x) {
    if (i == 0) {
        v->x = x;
    } else if (i == 1) {
        v->y = x;
    } else {
        v->z = x;
    }
}

static inline efx_vec3 efx_v3_add(efx_vec3 a, efx_vec3 b) {
    return efx_v3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline efx_vec3 efx_v3_sub(efx_vec3 a, efx_vec3 b) {
    return efx_v3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline efx_vec3 efx_v3_scale(efx_vec3 a, float s) {
    return efx_v3(a.x * s, a.y * s, a.z * s);
}

static inline efx_vec3 efx_v3_neg(efx_vec3 a) {
    return efx_v3(-a.x, -a.y, -a.z);
}

static inline float efx_v3_dot(efx_vec3 a, efx_vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline efx_vec3 efx_v3_cross(efx_vec3 a, efx_vec3 b) {
    return efx_v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                  a.x * b.y - a.y * b.x);
}

static inline float efx_v3_len_sq(efx_vec3 a) { return efx_v3_dot(a, a); }

static inline float efx_v3_len(efx_vec3 a) { return sqrtf(efx_v3_len_sq(a)); }

static inline float efx_v3_dist(efx_vec3 a, efx_vec3 b) {
    return efx_v3_len(efx_v3_sub(a, b));
}

static inline float efx_v3_dist_sq(efx_vec3 a, efx_vec3 b) {
    return efx_v3_len_sq(efx_v3_sub(a, b));
}

static inline efx_vec3 efx_v3_normalize(efx_vec3 a) {
    float len = efx_v3_len(a);
    if (len <= 1e-12f) {
        return efx_v3(0, 0, 0);
    }
    return efx_v3_scale(a, 1.0f / len);
}

static inline efx_vec3 efx_v3_min(efx_vec3 a, efx_vec3 b) {
    return efx_v3(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y,
                  a.z < b.z ? a.z : b.z);
}

static inline efx_vec3 efx_v3_max(efx_vec3 a, efx_vec3 b) {
    return efx_v3(a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y,
                  a.z > b.z ? a.z : b.z);
}

static inline efx_vec3 efx_v3_lerp(efx_vec3 a, efx_vec3 b, float t) {
    return efx_v3_add(a, efx_v3_scale(efx_v3_sub(b, a), t));
}

static inline efx_vec3 efx_v3_abs(efx_vec3 a) {
    return efx_v3(fabsf(a.x), fabsf(a.y), fabsf(a.z));
}

static inline float efx_clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float efx_deg_to_rad(float deg) { return deg * (EFX_PI / 180.0f); }

/* axis-aligned box used by broadphase and shape bounds */
typedef struct efx_aabb {
    efx_vec3 min, max;
} efx_aabb;

static inline efx_aabb efx_aabb_empty(void) {
    efx_aabb b;
    b.min = efx_v3(INFINITY, INFINITY, INFINITY);
    b.max = efx_v3(-INFINITY, -INFINITY, -INFINITY);
    return b;
}

static inline efx_aabb efx_aabb_add_point(efx_aabb b, efx_vec3 p) {
    b.min = efx_v3_min(b.min, p);
    b.max = efx_v3_max(b.max, p);
    return b;
}

static inline efx_aabb efx_aabb_union(efx_aabb a, efx_aabb b) {
    efx_aabb r;
    r.min = efx_v3_min(a.min, b.min);
    r.max = efx_v3_max(a.max, b.max);
    return r;
}

static inline efx_aabb efx_aabb_expand(efx_aabb a, float e) {
    a.min = efx_v3_sub(a.min, efx_v3(e, e, e));
    a.max = efx_v3_add(a.max, efx_v3(e, e, e));
    return a;
}

static inline int efx_aabb_overlap(efx_aabb a, efx_aabb b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y &&
           a.max.y >= b.min.y && a.min.z <= b.max.z && a.max.z >= b.min.z;
}

static inline efx_vec3 efx_aabb_center(efx_aabb a) {
    return efx_v3_scale(efx_v3_add(a.min, a.max), 0.5f);
}

static inline efx_vec3 efx_aabb_extent(efx_aabb a) {
    return efx_v3_scale(efx_v3_sub(a.max, a.min), 0.5f);
}

/* ray/segment vs AABB (slab method). Returns 1 on hit; tmin/tmax out
 * parameters are the entry/exit parameters along dir (dir not normalized). */
static inline int efx_ray_aabb(efx_vec3 origin, efx_vec3 dir, efx_aabb box,
                               float *tmin, float *tmax) {
    float t0 = -INFINITY, t1 = INFINITY;
    for (int i = 0; i < 3; i++) {
        float o = efx_v3_get(origin, i);
        float d = efx_v3_get(dir, i);
        float lo = efx_v3_get(box.min, i);
        float hi = efx_v3_get(box.max, i);
        if (fabsf(d) < 1e-20f) {
            if (o < lo || o > hi) return 0;
        } else {
            float inv = 1.0f / d;
            float near = (lo - o) * inv;
            float far = (hi - o) * inv;
            if (near > far) {
                float tmp = near;
                near = far;
                far = tmp;
            }
            if (near > t0) t0 = near;
            if (far < t1) t1 = far;
            if (t0 > t1) return 0;
        }
    }
    if (tmin) *tmin = t0;
    if (tmax) *tmax = t1;
    return 1;
}

#endif /* EFX_PHYS_VEC_H */
