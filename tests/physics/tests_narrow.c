#include "physics/broadphase.h"
#include "physics/narrow.h"
#include "physics/shape.h"
#include "test_support.h"

#include <stdlib.h>

/* ---- 1.3 trivial self-test / helpers ---- */
int t_support_basic(void) {
    CHECK(v3_near(efx_v3(1, 2, 3), efx_v3(1, 2, 3), 1e-6f));
    CHECK(!v3_near(efx_v3(1, 2, 3), efx_v3(1, 2, 4), 1e-6f));
    efx_physics_world *w = test_world();
    CHECK(w != NULL);
    efx_phys_body g = test_static_box(w, efx_v3(0, -0.5f, 0), efx_v3(10, 1, 10));
    CHECK(g != 0);
    test_step_n(w, 1.0f / 60.0f, 10);
    efx_physics_world_free(w);
    return 0;
}

/* ---- 2.1 shape descriptors ---- */
int t_shapes(void) {
    efx_shape s = efx_shape_sphere(0.5f);
    CHECK(s.type == EFX_PHYS_SHAPE_SPHERE && nearf(s.radius, 0.5f, 1e-6f));
    CHECK(efx_shape_valid(&s));
    efx_aabb b;
    efx_shape_bounds(&s, efx_v3(1, 2, 3), &b);
    CHECK(v3_near(b.min, efx_v3(0.5f, 1.5f, 2.5f), 1e-5f));
    CHECK(v3_near(b.max, efx_v3(1.5f, 2.5f, 3.5f), 1e-5f));

    efx_shape bx = efx_shape_box(efx_v3(2, 4, 6));
    CHECK(nearf(bx.half.x, 1, 1e-6f) && nearf(bx.half.y, 2, 1e-6f));
    CHECK(efx_shape_valid(&bx));
    efx_shape_bounds(&bx, efx_v3(0, 0, 0), &b);
    CHECK(v3_near(b.max, efx_v3(1, 2, 3), 1e-5f));

    efx_shape cap = efx_shape_capsule(0.4f, 1.8f);
    CHECK(nearf(cap.half_height, 0.5f, 1e-6f));
    CHECK(efx_shape_valid(&cap));
    efx_vec3 a, c;
    efx_shape_capsule_segment(&cap, efx_v3(0, 1, 0), &a, &c);
    CHECK(v3_near(a, efx_v3(0, 0.5f, 0), 1e-5f));
    CHECK(v3_near(c, efx_v3(0, 1.5f, 0), 1e-5f));
    efx_shape_bounds(&cap, efx_v3(0, 1, 0), &b);
    CHECK(nearf(b.min.y, 1 - 0.9f, 1e-5f));
    CHECK(nearf(b.max.y, 1 + 0.9f, 1e-5f));

    efx_shape bad = efx_shape_sphere(0);
    CHECK(!efx_shape_valid(&bad));
    bad = efx_shape_capsule(0.4f, 0.5f);
    CHECK(!efx_shape_valid(&bad));
    return 0;
}

/* ---- 2.2 shape vs triangle: face, edge, vertex ---- */
int t_narrow_triangle(void) {
    efx_vec3 v0 = efx_v3(-1, 0, -1), v1 = efx_v3(1, 0, -1),
             v2 = efx_v3(0, 0, 1);
    efx_narrow_contact c;

    /* face: sphere above the interior */
    CHECK(efx_narrow_sphere_triangle(efx_v3(0, 0.4f, -0.3f), 0.5f, v0, v1, v2,
                                     &c));
    CHECK(c.normal.y > 0.9f);
    CHECK(nearf(c.depth, 0.1f, 0.02f));

    /* vertex: sphere next to v0 */
    CHECK(efx_narrow_sphere_triangle(efx_v3(-1.3f, 0.3f, -1), 0.5f, v0, v1, v2,
                                     &c));
    CHECK(c.normal.x < -0.5f);
    CHECK(c.depth > 0);

    /* edge (v0-v1): sphere in front of the edge */
    CHECK(efx_narrow_sphere_triangle(efx_v3(0, 0.3f, -1.3f), 0.5f, v0, v1, v2,
                                     &c));
    CHECK(c.normal.z < -0.5f);

    /* separated sphere misses */
    CHECK(!efx_narrow_sphere_triangle(efx_v3(0, 2, 0), 0.5f, v0, v1, v2, &c));

    /* capsule vs triangle */
    CHECK(efx_narrow_capsule_triangle(efx_v3(0, 0.6f, -0.3f), 0.4f, 0.4f, v0,
                                      v1, v2, &c));
    CHECK(c.normal.y > 0.9f);
    CHECK(!efx_narrow_capsule_triangle(efx_v3(0, 5, 0), 0.4f, 0.4f, v0, v1, v2,
                                       &c));

    /* box vs triangle */
    CHECK(efx_narrow_box_triangle(efx_v3(0, 0.4f, -0.3f), efx_v3(0.5f, 0.5f,
                                                                 0.5f),
                                  v0, v1, v2, &c));
    CHECK(c.normal.y > 0.9f);
    CHECK(!efx_narrow_box_triangle(efx_v3(0, 5, 0),
                                   efx_v3(0.5f, 0.5f, 0.5f), v0, v1, v2, &c));
    return 0;
}

/* ---- 2.3 pairwise overlaps + symmetric normals ---- */
int t_narrow_pairs(void) {
    efx_narrow_contact c;

    /* sphere/sphere */
    CHECK(efx_narrow_sphere_sphere(efx_v3(0, 0, 0), 1.0f, efx_v3(1.5f, 0, 0),
                                   1.0f, &c));
    CHECK(nearf(c.normal.x, -1, 1e-5f));
    CHECK(nearf(c.depth, 0.5f, 1e-5f));
    CHECK(!efx_narrow_sphere_sphere(efx_v3(0, 0, 0), 1.0f, efx_v3(3, 0, 0),
                                    1.0f, &c));

    /* sphere/box face and inside */
    efx_shape box = efx_shape_box(efx_v3(2, 2, 2));
    CHECK(efx_narrow_sphere_box(efx_v3(1.4f, 0, 0), 0.5f, efx_v3(0, 0, 0),
                                box.half, &c));
    CHECK(nearf(c.normal.x, 1, 1e-4f));
    CHECK(efx_narrow_sphere_box(efx_v3(0.9f, 0, 0), 0.2f, efx_v3(0, 0, 0),
                                box.half, &c));
    CHECK(c.depth > 0.25f); /* center inside pushes out */

    /* sphere/capsule */
    efx_shape cap = efx_shape_capsule(0.5f, 2.0f);
    CHECK(efx_narrow_sphere_capsule(efx_v3(0.7f, 0, 0), 0.5f, efx_v3(0, 0, 0),
                                    cap.half_height, cap.radius, &c));
    CHECK(nearf(c.normal.x, 1, 1e-4f));

    /* box/box */
    CHECK(efx_narrow_box_box(efx_v3(0, 0, 0), efx_v3(1, 1, 1),
                             efx_v3(1.5f, 0, 0), efx_v3(1, 1, 1), &c));
    CHECK(nearf(c.depth, 0.5f, 1e-5f));
    CHECK(nearf(c.normal.x, -1, 1e-5f));

    /* capsule/capsule */
    CHECK(efx_narrow_capsule_capsule(efx_v3(0, 0, 0), 0.5f, 0.5f,
                                     efx_v3(0.8f, 0, 0), 0.5f, 0.5f, &c));
    CHECK(nearf(c.normal.x, -1, 1e-4f));

    /* symmetric normal invariant for a centered pair */
    efx_narrow_contact c2;
    CHECK(efx_narrow_sphere_sphere(efx_v3(1.5f, 0, 0), 1.0f, efx_v3(0, 0, 0),
                                   1.0f, &c2));
    CHECK(nearf(c2.normal.x, 1, 1e-5f));
    return 0;
}

/* ---- 2.4 rays ---- */
int t_ray_shapes(void) {
    float t;
    efx_vec3 n;
    CHECK(efx_narrow_ray_sphere(efx_v3(0, 0, 5), efx_v3(0, 0, -1),
                                efx_v3(0, 0, 0), 1.0f, 100, &t, &n));
    CHECK(nearf(t, 4, 1e-4f));
    CHECK(nearf(n.z, 1, 1e-4f));
    CHECK(!efx_narrow_ray_sphere(efx_v3(5, 5, 5), efx_v3(0, 0, -1),
                                 efx_v3(0, 0, 0), 1.0f, 100, &t, &n));

    CHECK(efx_narrow_ray_box(efx_v3(0, 0, 5), efx_v3(0, 0, -1),
                             efx_v3(0, 0, 0), efx_v3(1, 1, 1), 100, &t, &n));
    CHECK(nearf(t, 4, 1e-4f));
    CHECK(nearf(n.z, 1, 1e-4f));

    CHECK(efx_narrow_ray_capsule(efx_v3(2, 0, 0), efx_v3(-1, 0, 0),
                                 efx_v3(0, 0, 0), 1.0f, 0.5f, 100, &t, &n));
    CHECK(nearf(t, 1.5f, 1e-3f));
    CHECK(nearf(n.x, 1, 1e-3f));

    efx_vec3 v0 = efx_v3(-1, 0, -1), v1 = efx_v3(1, 0, -1),
             v2 = efx_v3(0, 0, 1);
    CHECK(efx_narrow_ray_triangle(efx_v3(0, 5, 0), efx_v3(0, -1, 0), v0, v1, v2,
                                  100, &t, &n));
    CHECK(nearf(t, 5, 1e-4f));
    CHECK(efx_narrow_ray_triangle(efx_v3(0, 5, 0), efx_v3(0, -1, 0), v0, v1, v2,
                                  3, &t, &n) == 0);
    return 0;
}

/* ---- 2.5 sweeps / no tunneling ---- */
int t_sweep_shapes(void) {
    efx_vec3 v0 = efx_v3(-10, 0, -10), v1 = efx_v3(10, 0, -10),
             v2 = efx_v3(0, 0, 10);
    efx_shape sphere = efx_shape_sphere(0.5f);
    float t;
    efx_vec3 point, normal;

    /* moderate drop */
    CHECK(efx_narrow_sweep_triangle(&sphere, efx_v3(0, 5, 0),
                                    efx_v3(0, -10, 0), v0, v1, v2, &t, &point,
                                    &normal));
    CHECK(nearf(t, 0.45f, 0.02f));
    CHECK(normal.y > 0.9f);

    /* fast motion must not tunnel */
    CHECK(efx_narrow_sweep_triangle(&sphere, efx_v3(0, 5, 0),
                                    efx_v3(0, -1000, 0), v0, v1, v2, &t, &point,
                                    &normal));
    CHECK(nearf(t, 0.0045f, 0.002f));

    /* miss when the motion stays above */
    CHECK(!efx_narrow_sweep_triangle(&sphere, efx_v3(0, 5, 0),
                                     efx_v3(0, -1, 0), v0, v1, v2, &t, &point,
                                     &normal));

    /* corner/vertex impact */
    CHECK(efx_narrow_sweep_triangle(&sphere, efx_v3(-10, 5, -10),
                                    efx_v3(10, -5, 10), v0, v1, v2, &t, &point,
                                    &normal));
    CHECK(t >= 0 && t <= 1);

    /* swept capsule */
    efx_shape cap = efx_shape_capsule(0.4f, 1.8f);
    CHECK(efx_narrow_sweep_triangle(&cap, efx_v3(0, 5, 0), efx_v3(0, -10, 0),
                                    v0, v1, v2, &t, &point, &normal));
    CHECK(nearf(t, (5 - 0.9f) / 10.0f, 0.03f));

    /* vs analytic box */
    efx_shape box = efx_shape_box(efx_v3(4, 1, 4));
    CHECK(efx_narrow_sweep(&sphere, efx_v3(0, 5, 0), efx_v3(0, -10, 0), &box,
                           efx_v3(0, 0, 0), &t, &point, &normal));
    CHECK(nearf(t, 0.4f, 0.03f));
    return 0;
}

/* ---- 3.2 BVH query vs brute force ---- */
static int bvh_collect_cb(void *ud, const efx_phys_mesh *m, int tri) {
    (void)m;
    (void)tri;
    int *count = ud;
    (*count)++;
    return 0;
}

int t_bvh_query(void) {
    /* a small grid mesh */
    enum { N = 8 };
    float verts[N * N * 3];
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            int i = (y * N + x) * 3;
            verts[i] = (float)x - N * 0.5f;
            verts[i + 1] = sinf((float)(x + y)) * 0.2f;
            verts[i + 2] = (float)y - N * 0.5f;
        }
    }
    int quads = (N - 1) * (N - 1);
    uint32_t *idx = malloc((size_t)quads * 6 * sizeof(uint32_t));
    CHECK(idx != NULL);
    int k = 0;
    for (int y = 0; y < N - 1; y++) {
        for (int x = 0; x < N - 1; x++) {
            uint32_t a = (uint32_t)(y * N + x);
            uint32_t b = (uint32_t)(y * N + x + 1);
            uint32_t c = (uint32_t)((y + 1) * N + x);
            uint32_t d = (uint32_t)((y + 1) * N + x + 1);
            idx[k++] = a; idx[k++] = b; idx[k++] = c;
            idx[k++] = b; idx[k++] = d; idx[k++] = c;
        }
    }
    int tri_count = quads * 2;
    efx_phys_mesh *m = efx_phys_mesh_create(verts, N * N, idx, tri_count);
    CHECK(m != NULL);

    /* compare BVH query against brute force over a few boxes */
    for (int q = 0; q < 5; q++) {
        efx_aabb box;
        box.min = efx_v3(-3 + q, -0.5f, -3);
        box.max = efx_v3(3 - q, 0.5f, 1 + q);
        int bvh = 0;
        efx_phys_mesh_query_aabb(m, box, bvh_collect_cb, &bvh);
        int brute = 0;
        for (int t = 0; t < tri_count; t++) {
            efx_vec3 v[3];
            efx_phys_mesh_tri(m, t, v);
            efx_aabb tb = efx_aabb_add_point(efx_aabb_empty(), v[0]);
            tb = efx_aabb_add_point(tb, v[1]);
            tb = efx_aabb_add_point(tb, v[2]);
            if (efx_aabb_overlap(tb, box)) brute++;
        }
        CHECK_MSG(bvh == brute, "BVH count must match brute force");
    }
    free(idx);
    efx_phys_mesh_free(m);
    return 0;
}
