#ifndef EFX_PHYSICS_TEST_SUPPORT_H
#define EFX_PHYSICS_TEST_SUPPORT_H

/*
 * Shared helpers for the F12 headless physics tests. Everything here is
 * header-only and depends solely on the physics module, so the standalone
 * `efx_physics_tests` binary builds without the engine.
 */

#include "../test_support.h"
#include "physics/physics.h"

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            return 1;                                                        \
        }                                                                    \
    } while (0)

#define CHECK_MSG(cond, msg)                                                 \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__,     \
                    #cond, msg);                                             \
            return 1;                                                        \
        }                                                                    \
    } while (0)

static inline int nearf(float a, float b, float eps) {
    return fabsf(a - b) <= eps;
}

static inline int v3_near(efx_vec3 a, efx_vec3 b, float eps) {
    return nearf(a.x, b.x, eps) && nearf(a.y, b.y, eps) && nearf(a.z, b.z, eps);
}

static inline efx_physics_world *test_world(void) {
    return efx_physics_world_new();
}

static inline efx_body_desc test_body_desc(int dynamic, int sensor,
                                           efx_shape shape, efx_vec3 pos,
                                           float mass) {
    efx_body_desc d;
    memset(&d, 0, sizeof(d));
    d.dynamic = dynamic;
    d.sensor = sensor;
    d.shape = shape;
    d.position = pos;
    d.mass = mass;
    d.friction = 0.6f;
    d.restitution = 0.0f;
    d.layer = 0xFFFFFFFFu;
    d.mask = 0xFFFFFFFFu;
    return d;
}

static inline efx_phys_body test_static_box(efx_physics_world *w, efx_vec3 c,
                                            efx_vec3 size) {
    efx_body_desc d = test_body_desc(0, 0, efx_shape_box(size), c, 0);
    return efx_physics_create_body(w, &d);
}

static inline efx_phys_body test_static_box_sensor(efx_physics_world *w,
                                                   efx_vec3 c, efx_vec3 size) {
    efx_body_desc d = test_body_desc(0, 1, efx_shape_box(size), c, 0);
    return efx_physics_create_body(w, &d);
}

static inline efx_phys_body test_dynamic_sphere(efx_physics_world *w,
                                                efx_vec3 c, float r,
                                                float mass) {
    efx_body_desc d = test_body_desc(1, 0, efx_shape_sphere(r), c, mass);
    return efx_physics_create_body(w, &d);
}

static inline efx_phys_body test_dynamic_box(efx_physics_world *w, efx_vec3 c,
                                             efx_vec3 size, float mass) {
    efx_body_desc d = test_body_desc(1, 0, efx_shape_box(size), c, mass);
    return efx_physics_create_body(w, &d);
}

static inline void test_step_n(efx_physics_world *w, float dt, int n) {
    for (int i = 0; i < n; i++) efx_physics_step(w, dt);
}

static inline int test_find_contact(efx_physics_world *w, efx_phys_body b,
                                    efx_phys_body other,
                                    efx_contact_info *out) {
    int n = efx_physics_body_contact_count(w, b);
    for (int i = 0; i < n; i++) {
        efx_contact_info ci;
        if (!efx_physics_body_contact(w, b, i, &ci)) continue;
        if (ci.body == other && ci.sensor == 0) {
            if (out) *out = ci;
            return 1;
        }
    }
    return 0;
}

/* a triangle mesh slab at y = height covering [-size, size]^2 */
static inline efx_phys_body test_mesh_quad(efx_physics_world *w, float height,
                                           float half) {
    float verts[12] = {
        -half, height, -half, half, height, -half,
        -half, height, half,  half, height, half,
    };
    uint32_t idx[6] = {0, 1, 2, 1, 3, 2};
    efx_static_mesh_desc d;
    memset(&d, 0, sizeof(d));
    d.position = efx_v3(0, 0, 0);
    d.friction = 0.6f;
    d.layer = 0xFFFFFFFFu;
    d.mask = 0xFFFFFFFFu;
    return efx_physics_create_static_mesh(w, verts, 4, idx, 2, &d);
}

#endif /* EFX_PHYSICS_TEST_SUPPORT_H */
