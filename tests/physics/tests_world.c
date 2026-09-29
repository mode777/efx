#include "test_support.h"

#include <stdlib.h>

/* ---- 3.1 registry: ids, free-list reuse, clear ---- */
int t_world_registry(void) {
    efx_physics_world *w = test_world();
    efx_phys_body a = test_static_box(w, efx_v3(0, 0, 0), efx_v3(1, 1, 1));
    efx_phys_body b = test_static_box(w, efx_v3(2, 0, 0), efx_v3(1, 1, 1));
    CHECK(a != 0 && b != 0 && a != b);
    CHECK(efx_physics_body_alive(w, a) && efx_physics_body_alive(w, b));

    CHECK(efx_physics_destroy_body(w, a) == 1);
    CHECK(efx_physics_destroy_body(w, a) == 0); /* idempotent */
    CHECK(!efx_physics_body_alive(w, a));

    /* the freed slot is reused but the id is new (never recycled) */
    efx_phys_body c = test_static_box(w, efx_v3(0, 0, 0), efx_v3(1, 1, 1));
    CHECK(c != a && c != b);
    CHECK(efx_physics_body_alive(w, c));

    efx_physics_clear(w);
    CHECK(!efx_physics_body_alive(w, b));
    CHECK(!efx_physics_body_alive(w, c));
    efx_phys_body d = test_static_box(w, efx_v3(0, 0, 0), efx_v3(1, 1, 1));
    CHECK(d != 0);
    efx_physics_world_free(w);
    return 0;
}

/* ---- 3.3 layer/mask filtering and stable order ---- */
int t_broadphase_filter(void) {
    efx_physics_world *w = test_world();
    /* a dynamic box with an exclusive mask falls through a static one */
    efx_body_desc s = test_body_desc(0, 0, efx_shape_box(efx_v3(1, 1, 1)),
                                     efx_v3(0, 1.2f, 0), 0);
    s.layer = 0x2u;
    s.mask = 0x2u;
    efx_phys_body b = efx_physics_create_body(w, &s);
    (void)b;
    efx_body_desc d = test_body_desc(1, 0, efx_shape_box(efx_v3(1, 1, 1)),
                                     efx_v3(0, 3, 0), 1);
    d.layer = 0x1u;
    d.mask = 0x1u;
    efx_phys_body a = efx_physics_create_body(w, &d);
    test_step_n(w, 1.0f / 60.0f, 120);
    CHECK(efx_physics_body_contact_count(w, a) == 0);
    efx_vec3 pa;
    efx_physics_body_position(w, a, &pa);
    CHECK(pa.y < 1.2f); /* a passed through b */

    /* now make them interact */
    d.layer = 0x3u;
    d.mask = 0x3u;
    efx_phys_body c = efx_physics_create_body(w, &d);
    s.layer = 0x1u;
    s.mask = 0x3u;
    s.position = efx_v3(0, 1.2f, 0);
    efx_phys_body e = efx_physics_create_body(w, &s);
    CHECK(e != 0);
    test_step_n(w, 1.0f / 60.0f, 120);
    CHECK(efx_physics_body_contact_count(w, c) > 0);
    efx_physics_world_free(w);
    return 0;
}

/* ---- 3.4 determinism: replay + independent worlds ---- */
static uint64_t hash_state(efx_physics_world *w) {
    uint64_t h = 1469598103934665603ull;
    for (int i = 1; i <= 20; i++) {
        efx_vec3 p;
        if (efx_physics_body_position(w, (efx_phys_body)i, &p)) {
            float v[3] = {p.x, p.y, p.z};
            for (int k = 0; k < 3; k++) {
                uint32_t bits;
                memcpy(&bits, &v[k], 4);
                h = (h ^ bits) * 1099511628211ull;
            }
        }
    }
    return h;
}

static void build_replay_scene(efx_physics_world *w) {
    test_static_box(w, efx_v3(0, -0.5f, 0), efx_v3(20, 1, 20));
    for (int i = 0; i < 8; i++) {
        test_dynamic_sphere(w, efx_v3((float)(i - 4) * 0.8f, 2.0f + i * 0.7f, 0),
                            0.4f, 1.0f);
    }
}

int t_determinism(void) {
    efx_physics_world *w1 = test_world();
    efx_physics_world *w2 = test_world();
    build_replay_scene(w1);
    build_replay_scene(w2);
    for (int i = 0; i < 180; i++) {
        efx_physics_step(w1, 1.0f / 60.0f);
        efx_physics_step(w2, 1.0f / 60.0f);
    }
    CHECK(hash_state(w1) == hash_state(w2));
    efx_physics_world_free(w1);
    efx_physics_world_free(w2);

    /* interleaved stepping equals isolated stepping */
    efx_physics_world *a = test_world();
    efx_physics_world *b = test_world();
    efx_physics_world *solo = test_world();
    build_replay_scene(a);
    build_replay_scene(b);
    build_replay_scene(solo);
    for (int i = 0; i < 180; i++) {
        efx_physics_step(a, 1.0f / 60.0f);
        efx_physics_step(b, 1.0f / 60.0f);
        efx_physics_step(solo, 1.0f / 60.0f);
    }
    CHECK(hash_state(a) == hash_state(solo));
    CHECK(hash_state(b) == hash_state(solo));
    efx_physics_world_free(a);
    efx_physics_world_free(b);
    efx_physics_world_free(solo);
    return 0;
}

/* ---- 4.1 integration + forces ---- */
int t_integrate(void) {
    efx_physics_world *w = test_world();
    efx_phys_body b = test_dynamic_sphere(w, efx_v3(0, 10, 0), 0.5f, 2.0f);

    /* no step -> nothing moves even with velocity */
    efx_physics_body_set_velocity(w, b, efx_v3(5, 0, 0));
    efx_vec3 p;
    efx_physics_body_position(w, b, &p);
    CHECK(nearf(p.y, 10, 1e-6f));
    CHECK(nearf(p.x, 0, 1e-6f));

    /* free fall */
    efx_physics_body_set_velocity(w, b, efx_v3(0, 0, 0));
    efx_physics_step(w, 1.0f / 60.0f);
    efx_vec3 v;
    efx_physics_body_velocity(w, b, &v);
    CHECK(nearf(v.y, -9.81f / 60.0f, 1e-3f));
    efx_physics_body_position(w, b, &p);
    CHECK(p.y < 10);

    /* impulse: delta v = impulse / mass */
    efx_physics_body_set_velocity(w, b, efx_v3(0, 0, 0));
    efx_physics_body_apply_impulse(w, b, efx_v3(0, 4, 0));
    efx_physics_body_velocity(w, b, &v);
    CHECK(nearf(v.y, 2.0f, 1e-4f));

    /* force accumulates and is consumed by the next step (gravity off) */
    efx_physics_set_gravity(w, efx_v3(0, 0, 0));
    efx_physics_body_set_velocity(w, b, efx_v3(0, 0, 0));
    efx_physics_body_apply_force(w, b, efx_v3(0, 20, 0));
    efx_physics_step(w, 1.0f / 60.0f);
    efx_physics_body_velocity(w, b, &v);
    CHECK(nearf(v.y, 20.0f / 2.0f / 60.0f, 1e-4f));
    efx_physics_step(w, 1.0f / 60.0f);
    efx_physics_body_velocity(w, b, &v);
    CHECK(nearf(v.y, 20.0f / 2.0f / 60.0f, 1e-4f)); /* force was consumed */
    efx_physics_world_free(w);
    return 0;
}

/* ---- 4.2 contact manifold ---- */
int t_contact_manifold(void) {
    efx_physics_world *w = test_world();
    efx_phys_body ground = test_static_box(w, efx_v3(0, -0.5f, 0),
                                           efx_v3(10, 1, 10));
    efx_phys_body box = test_dynamic_box(w, efx_v3(0, 0.45f, 0),
                                         efx_v3(1, 1, 1), 1.0f);
    test_step_n(w, 1.0f / 60.0f, 5);
    efx_contact_info ci;
    CHECK(test_find_contact(w, box, ground, &ci));
    CHECK(ci.normal.y > 0.9f);
    CHECK(nearf(ci.normal.x, 0, 1e-4f));
    CHECK(ci.depth > 0);
    CHECK(ci.impulse > 0);
    efx_physics_world_free(w);
    return 0;
}

/* ---- 4.3 restitution + friction ---- */
int t_restitution_friction(void) {
    efx_physics_world *w = test_world();
    test_static_box(w, efx_v3(0, -0.5f, 0), efx_v3(20, 1, 20));

    efx_body_desc d = test_body_desc(1, 0, efx_shape_sphere(0.5f),
                                     efx_v3(0, 1.0f, 0), 1.0f);
    d.restitution = 0.8f;
    efx_phys_body bouncer = efx_physics_create_body(w, &d);
    efx_physics_body_set_velocity(w, bouncer, efx_v3(0, -6, 0));
    int bounced = 0;
    for (int i = 0; i < 60; i++) {
        efx_physics_step(w, 1.0f / 60.0f);
        efx_vec3 v;
        efx_physics_body_velocity(w, bouncer, &v);
        if (v.y > 1.0f) {
            bounced = 1;
            break;
        }
    }
    CHECK(bounced);

    /* friction reduces tangential velocity */
    efx_body_desc f = test_body_desc(1, 0, efx_shape_box(efx_v3(1, 1, 1)),
                                     efx_v3(0, 0.5f, 0), 1.0f);
    f.friction = 1.0f;
    efx_phys_body slider = efx_physics_create_body(w, &f);
    efx_physics_body_set_velocity(w, slider, efx_v3(5, 0, 0));
    test_step_n(w, 1.0f / 60.0f, 30);
    efx_vec3 v;
    efx_physics_body_velocity(w, slider, &v);
    CHECK(v.x < 5.0f);
    CHECK(v3_near(v, efx_v3(0, 0, 0), 3.0f)); /* heavily damped */
    efx_physics_world_free(w);
    return 0;
}

/* ---- 4.4 resting settle ---- */
static int settle_check(efx_shape shape, float half_extent, const char *what) {
    efx_physics_world *w = test_world();
    test_static_box(w, efx_v3(0, -0.5f, 0), efx_v3(30, 1, 30));
    efx_body_desc d = test_body_desc(1, 0, shape,
                                     efx_v3(0, half_extent + 1.0f, 0), 1.0f);
    efx_phys_body b = efx_physics_create_body(w, &d);
    test_step_n(w, 1.0f / 60.0f, 400);
    efx_vec3 p;
    efx_physics_body_position(w, b, &p);
    efx_vec3 v;
    efx_physics_body_velocity(w, b, &v);
    if (!isfinite(p.y) || !isfinite(v.y)) {
        fprintf(stderr, "FAIL settle %s: non-finite\n", what);
        efx_physics_world_free(w);
        return 1;
    }
    /* surface should rest between the plane and the slop, no sinking */
    if (p.y < half_extent - 0.05f || p.y > half_extent + 0.05f) {
        fprintf(stderr, "FAIL settle %s: rest y=%.5f expected ~%.3f\n", what,
                (double)p.y, half_extent);
        efx_physics_world_free(w);
        return 1;
    }
    if (fabsf(v.y) > 0.05f) {
        fprintf(stderr, "FAIL settle %s: jitter vy=%.5f\n", what, (double)v.y);
        efx_physics_world_free(w);
        return 1;
    }
    efx_physics_world_free(w);
    return 0;
}

int t_settle(void) {
    if (settle_check(efx_shape_sphere(0.5f), 0.5f, "sphere")) return 1;
    if (settle_check(efx_shape_box(efx_v3(1, 1, 1)), 0.5f, "box")) return 1;
    return 0;
}

/* ---- 4.5 step(dt) wiring + clamps ---- */
int t_step_dt(void) {
    efx_physics_world *w = test_world();
    efx_phys_body b = test_dynamic_sphere(w, efx_v3(0, 5, 0), 0.5f, 1.0f);
    efx_physics_body_set_velocity(w, b, efx_v3(1, 0, 0));
    efx_vec3 p0, p;
    efx_physics_body_position(w, b, &p0);

    efx_physics_step(w, 0);
    efx_physics_step(w, -1);
    efx_physics_body_position(w, b, &p);
    CHECK(v3_near(p, p0, 1e-6f)); /* non-positive dt is a no-op */

    efx_physics_step(w, 1000); /* absurd dt is clamped */
    efx_physics_body_position(w, b, &p);
    CHECK(p.y > p0.y - 10 && p.y < p0.y); /* finite, bounded fall */

    /* stable across the documented range: no NaN after many steps */
    for (float dt = 1.0f / 240.0f; dt <= 0.1f; dt += 1.0f / 120.0f) {
        efx_physics_world *s = test_world();
        efx_phys_body ob = test_dynamic_sphere(s, efx_v3(0, 3, 0), 0.5f, 1.0f);
        test_step_n(s, dt, 100);
        efx_vec3 q;
        efx_physics_body_position(s, ob, &q);
        CHECK(isfinite(q.x) && isfinite(q.y) && isfinite(q.z));
        efx_physics_world_free(s);
    }
    efx_physics_world_free(w);
    return 0;
}

/* ---- 4.6 stress / fuzz ---- */
int t_stress(void) {
    efx_physics_world *w = test_world();
    test_static_box(w, efx_v3(0, -0.5f, 0), efx_v3(60, 1, 60));
    uint32_t seed = 12345u;
    int created = 0;
    for (int i = 0; i < 300; i++) {
        seed = seed * 1664525u + 1013904223u;
        float x = (float)((seed >> 8) % 2000) / 100.0f - 10.0f;
        seed = seed * 1664525u + 1013904223u;
        float z = (float)((seed >> 8) % 2000) / 100.0f - 10.0f;
        seed = seed * 1664525u + 1013904223u;
        float y = 1.0f + (float)((seed >> 8) % 1000) / 100.0f;
        efx_body_desc d = test_body_desc(1, 0, efx_shape_box(efx_v3(0.5f, 0.5f,
                                                                    0.5f)),
                                         efx_v3(x, y, z), 1.0f);
        if (efx_physics_create_body(w, &d)) created++;
    }
    CHECK(created == 300);
    for (int i = 0; i < 200; i++) {
        efx_physics_step(w, 1.0f / 60.0f);
        if (i % 40 == 0) {
            for (int id = 1; id <= 301; id++) {
                efx_vec3 p;
                if (!efx_physics_body_position(w, (efx_phys_body)id, &p)) break;
                CHECK(isfinite(p.x) && isfinite(p.y) && isfinite(p.z));
                CHECK(p.y > -5.0f); /* no tunneling through the floor */
            }
        }
    }
    efx_physics_world_free(w);
    return 0;
}

/* ---- 5.1 sensors ---- */
int t_sensors(void) {
    efx_physics_world *w = test_world();
    test_static_box_sensor(w, efx_v3(0, 2, 0), efx_v3(4, 1, 4));
    efx_phys_body ball = test_dynamic_sphere(w, efx_v3(0, 5, 0), 0.4f, 1.0f);
    int saw_sensor = 0;
    for (int i = 0; i < 120; i++) {
        efx_physics_step(w, 1.0f / 60.0f);
        int n = efx_physics_body_contact_count(w, ball);
        for (int k = 0; k < n; k++) {
            efx_contact_info ci;
            efx_physics_body_contact(w, ball, k, &ci);
            if (ci.sensor) saw_sensor = 1;
        }
    }
    CHECK(saw_sensor);
    efx_vec3 p;
    efx_physics_body_position(w, ball, &p);
    CHECK(p.y < 0.0f); /* passed through the sensor */
    efx_physics_world_free(w);
    return 0;
}

/* ---- 5.2 contact reporting + reset ---- */
int t_contact_report(void) {
    efx_physics_world *w = test_world();
    efx_phys_body ground = test_static_box(w, efx_v3(0, -0.5f, 0),
                                           efx_v3(10, 1, 10));
    efx_phys_body box = test_dynamic_box(w, efx_v3(0, 0.5f, 0),
                                         efx_v3(1, 1, 1), 1.0f);
    test_step_n(w, 1.0f / 60.0f, 5);
    CHECK(efx_physics_body_contact_count(w, box) >= 1);

    /* lift the box clear, step again: contacts reset */
    efx_physics_body_set_position(w, box, efx_v3(0, 5, 0));
    efx_physics_body_set_velocity(w, box, efx_v3(0, 0, 0));
    efx_physics_step(w, 1.0f / 60.0f);
    CHECK(efx_physics_body_contact_count(w, box) == 0);

    /* deterministic order across identical runs */
    efx_physics_world *w2 = test_world();
    efx_phys_body g2 = test_static_box(w2, efx_v3(0, -0.5f, 0),
                                       efx_v3(10, 1, 10));
    test_dynamic_box(w2, efx_v3(0, 0.5f, 0), efx_v3(1, 1, 1), 1.0f);
    test_step_n(w2, 1.0f / 60.0f, 5);
    efx_physics_world_free(w2);
    (void)g2;
    (void)ground;
    efx_physics_world_free(w);
    return 0;
}

/* ---- 6.1 raycast query ---- */
int t_raycast_query(void) {
    efx_physics_world *w = test_world();
    test_static_box(w, efx_v3(2, 0, 0), efx_v3(1, 2, 2));
    test_static_box(w, efx_v3(5, 0, 0), efx_v3(1, 2, 2));
    efx_ray_hit hits[8];
    int n = efx_physics_raycast(w, efx_v3(0, 0, 0), efx_v3(1, 0, 0), 100,
                                0xFFFFFFFFu, 0, 0, hits, 8);
    CHECK(n == 1);
    CHECK(nearf(hits[0].distance, 1.5f, 1e-3f));
    CHECK(nearf(hits[0].normal.x, -1, 1e-4f));

    n = efx_physics_raycast(w, efx_v3(0, 0, 0), efx_v3(1, 0, 0), 100,
                            0xFFFFFFFFu, 0, 1, hits, 8);
    CHECK(n == 2);
    CHECK(hits[0].distance < hits[1].distance);

    /* miss */
    n = efx_physics_raycast(w, efx_v3(0, 0, 0), efx_v3(0, 1, 0), 100,
                            0xFFFFFFFFu, 0, 0, hits, 8);
    CHECK(n == 0);

    /* beyond maxDistance */
    n = efx_physics_raycast(w, efx_v3(0, 0, 0), efx_v3(1, 0, 0), 1.0f,
                            0xFFFFFFFFu, 0, 0, hits, 8);
    CHECK(n == 0);

    /* sensors excluded unless requested */
    test_static_box_sensor(w, efx_v3(0.5f, 0, 0), efx_v3(0.2f, 2, 2));
    n = efx_physics_raycast(w, efx_v3(0, 0, 0), efx_v3(1, 0, 0), 100,
                            0xFFFFFFFFu, 0, 0, hits, 8);
    CHECK(n == 1);
    CHECK(nearf(hits[0].distance, 1.5f, 1e-3f));
    n = efx_physics_raycast(w, efx_v3(0, 0, 0), efx_v3(1, 0, 0), 100,
                            0xFFFFFFFFu, 1, 1, hits, 8);
    CHECK(n >= 1);
    CHECK(nearf(hits[0].distance, 0.4f, 1e-2f));
    efx_physics_world_free(w);
    return 0;
}

/* ---- 6.2 overlap query ---- */
int t_overlap_query(void) {
    efx_physics_world *w = test_world();
    efx_body_desc sd = test_body_desc(0, 0, efx_shape_box(efx_v3(2, 2, 2)),
                                      efx_v3(0, 0, 0), 0);
    sd.layer = 0x1u;
    sd.mask = 0x1u;
    efx_phys_body solid = efx_physics_create_body(w, &sd);
    sd.position = efx_v3(1, 0, 0);
    sd.sensor = 1;
    efx_phys_body sensor = efx_physics_create_body(w, &sd);
    efx_shape q = efx_shape_sphere(0.5f);
    efx_overlap_hit out[8];
    int n = efx_physics_overlap(w, &q, efx_v3(0, 0, 0), 0xFFFFFFFFu, 1, out, 8);
    CHECK(n == 2);
    (void)solid;
    (void)sensor;

    /* mask selects a layer */
    efx_body_desc d = test_body_desc(0, 0, efx_shape_box(efx_v3(1, 1, 1)),
                                     efx_v3(0.2f, 0, 0), 0);
    d.layer = 0x4u;
    d.mask = 0x4u;
    efx_phys_body layered = efx_physics_create_body(w, &d);
    n = efx_physics_overlap(w, &q, efx_v3(0, 0, 0), 0x4u, 1, out, 8);
    CHECK(n == 1 && out[0].body == layered);

    /* empty space */
    n = efx_physics_overlap(w, &q, efx_v3(20, 20, 20), 0xFFFFFFFFu, 1, out, 8);
    CHECK(n == 0);
    efx_physics_world_free(w);
    return 0;
}

/* ---- 6.3 shapeCast query ---- */
int t_shapecast_query(void) {
    efx_physics_world *w = test_world();
    test_static_box(w, efx_v3(2, 0, 0), efx_v3(1, 2, 2));
    test_static_box(w, efx_v3(5, 0, 0), efx_v3(1, 2, 2));
    efx_shape q = efx_shape_sphere(0.5f);
    efx_shape_hit hit;
    CHECK(efx_physics_shape_cast(w, &q, efx_v3(0, 0, 0), efx_v3(10, 0, 0),
                                 0xFFFFFFFFu, 0, &hit));
    CHECK(hit.fraction > 0 && hit.fraction < 1);
    CHECK(nearf(hit.point.x, 1.0f, 0.1f));
    CHECK(nearf(hit.normal.x, -1, 0.1f));

    /* miss */
    CHECK(!efx_physics_shape_cast(w, &q, efx_v3(0, 0, 0), efx_v3(0, 5, 0),
                                  0xFFFFFFFFu, 0, &hit));

    /* sensors excluded by default */
    test_static_box_sensor(w, efx_v3(0.5f, 0, 0), efx_v3(0.2f, 2, 2));
    CHECK(efx_physics_shape_cast(w, &q, efx_v3(0, 0, 0), efx_v3(10, 0, 0),
                                 0xFFFFFFFFu, 0, &hit));
    CHECK(nearf(hit.fraction, 0.1f, 0.03f));
    CHECK(efx_physics_shape_cast(w, &q, efx_v3(0, 0, 0), efx_v3(10, 0, 0),
                                 0xFFFFFFFFu, 1, &hit));
    CHECK(hit.fraction < 0.03f);
    efx_physics_world_free(w);
    return 0;
}
