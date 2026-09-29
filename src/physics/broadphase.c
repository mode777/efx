#include "physics/broadphase.h"

#include <stdlib.h>
#include <string.h>

#define EFX_BVH_LEAF 4

static const efx_phys_mesh *g_sort_mesh;
static int g_sort_axis;

static float tri_centroid_axis(const efx_phys_mesh *m, int tri, int axis) {
    const uint32_t *idx = &m->indices[(size_t)tri * 3];
    float c = 0;
    for (int k = 0; k < 3; k++) {
        c += efx_v3_get(m->verts[idx[k]], axis);
    }
    return c * (1.0f / 3.0f);
}

static int tri_centroid_cmp(const void *a, const void *b) {
    int ta = *(const int32_t *)a;
    int tb = *(const int32_t *)b;
    float ca = tri_centroid_axis(g_sort_mesh, ta, g_sort_axis);
    float cb = tri_centroid_axis(g_sort_mesh, tb, g_sort_axis);
    if (ca < cb) return -1;
    if (ca > cb) return 1;
    return ta < tb ? -1 : (ta > tb ? 1 : 0);
}

static void tri_bounds(const efx_phys_mesh *m, int tri, efx_aabb *out) {
    efx_vec3 v[3];
    efx_phys_mesh_tri(m, tri, v);
    efx_aabb b = efx_aabb_add_point(efx_aabb_empty(), v[0]);
    b = efx_aabb_add_point(b, v[1]);
    b = efx_aabb_add_point(b, v[2]);
    *out = b;
}

static int build_node(efx_phys_mesh *m, int start, int count) {
    int node = m->node_count++;
    efx_bvh_node *n = &m->nodes[node];
    efx_aabb bounds = efx_aabb_empty();
    efx_aabb centroids = efx_aabb_empty();
    for (int i = 0; i < count; i++) {
        int tri = m->tri_order[start + i];
        efx_aabb tb;
        tri_bounds(m, tri, &tb);
        bounds = efx_aabb_union(bounds, tb);
        centroids = efx_aabb_add_point(centroids, efx_aabb_center(tb));
    }
    n->bounds = bounds;
    n->left = n->right = -1;
    n->start = start;
    n->count = count;

    if (count <= EFX_BVH_LEAF) {
        return node;
    }

    efx_vec3 ce = efx_aabb_extent(centroids);
    int axis = 0;
    if (ce.y > ce.x) axis = 1;
    if (ce.z > efx_v3_get(ce, axis)) axis = 2;
    if (efx_v3_get(ce, axis) <= 1e-12f) {
        return node; /* degenerate: all centroids coincide -> leaf */
    }

    g_sort_mesh = m;
    g_sort_axis = axis;
    qsort(&m->tri_order[start], (size_t)count, sizeof(int32_t),
          tri_centroid_cmp);

    int mid = count / 2;
    n->left = build_node(m, start, mid);
    n->right = build_node(m, start + mid, count - mid);
    n->count = 0;
    n->start = 0;
    return node;
}

efx_phys_mesh *efx_phys_mesh_create(const float *positions, int vert_count,
                                    const uint32_t *indices, int tri_count) {
    if (!positions || vert_count <= 0) return NULL;
    if (tri_count <= 0) {
        if (indices) return NULL;
        tri_count = vert_count / 3;
        if (tri_count <= 0) return NULL;
    }

    efx_phys_mesh *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->vert_count = vert_count;
    m->tri_count = tri_count;
    m->verts = malloc((size_t)vert_count * sizeof(efx_vec3));
    m->indices = malloc((size_t)tri_count * 3 * sizeof(uint32_t));
    m->tri_order = malloc((size_t)tri_count * sizeof(int32_t));
    m->nodes = malloc((size_t)tri_count * 2 * sizeof(efx_bvh_node));
    if (!m->verts || !m->indices || !m->tri_order || !m->nodes) {
        efx_phys_mesh_free(m);
        return NULL;
    }
    memcpy(m->verts, positions, (size_t)vert_count * sizeof(efx_vec3));
    if (indices) {
        for (int t = 0; t < tri_count; t++) {
            for (int k = 0; k < 3; k++) {
                uint32_t i = indices[(size_t)t * 3 + k];
                if ((int)i >= vert_count) {
                    efx_phys_mesh_free(m);
                    return NULL;
                }
                m->indices[(size_t)t * 3 + k] = i;
            }
        }
    } else {
        for (int i = 0; i < tri_count * 3; i++) {
            m->indices[i] = (uint32_t)i;
        }
    }
    for (int t = 0; t < tri_count; t++) m->tri_order[t] = t;

    m->node_count = 0;
    build_node(m, 0, tri_count);

    m->bounds = efx_aabb_empty();
    for (int v = 0; v < vert_count; v++) {
        m->bounds = efx_aabb_add_point(m->bounds, m->verts[v]);
    }
    return m;
}

void efx_phys_mesh_free(efx_phys_mesh *m) {
    if (!m) return;
    free(m->verts);
    free(m->indices);
    free(m->tri_order);
    free(m->nodes);
    free(m);
}

void efx_phys_mesh_bounds(const efx_phys_mesh *m, efx_aabb *out) {
    if (!m) {
        *out = efx_aabb_empty();
        return;
    }
    *out = m->bounds;
}

void efx_phys_mesh_tri(const efx_phys_mesh *m, int tri, efx_vec3 v[3]) {
    const uint32_t *idx = &m->indices[(size_t)tri * 3];
    v[0] = m->verts[idx[0]];
    v[1] = m->verts[idx[1]];
    v[2] = m->verts[idx[2]];
}

static void query_node(const efx_phys_mesh *m, int node, efx_aabb box,
                       efx_phys_tri_cb cb, void *ud, int *stopped) {
    if (*stopped) return;
    const efx_bvh_node *n = &m->nodes[node];
    if (!efx_aabb_overlap(n->bounds, box)) return;
    if (n->left < 0 || n->right < 0) {
        for (int i = 0; i < n->count; i++) {
            int tri = m->tri_order[n->start + i];
            efx_aabb tb;
            tri_bounds(m, tri, &tb);
            if (efx_aabb_overlap(tb, box)) {
                if (cb(ud, m, tri)) {
                    *stopped = 1;
                    return;
                }
            }
        }
        return;
    }
    query_node(m, n->left, box, cb, ud, stopped);
    query_node(m, n->right, box, cb, ud, stopped);
}

void efx_phys_mesh_query_aabb(const efx_phys_mesh *m, efx_aabb box,
                              efx_phys_tri_cb cb, void *ud) {
    if (!m || !cb || m->node_count == 0 || !efx_aabb_overlap(m->bounds, box)) {
        return;
    }
    int stopped = 0;
    query_node(m, 0, box, cb, ud, &stopped);
}
