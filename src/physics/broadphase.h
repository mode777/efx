#ifndef EFX_PHYS_BROADPHASE_H
#define EFX_PHYS_BROADPHASE_H

/*
 * F12 broadphase: a static triangle-mesh BVH (ADR 0040). A mesh collider is
 * an owned triangle soup with a median-split AABB tree; ray/sweep/overlap
 * queries descend it and test candidate triangles. Dynamic bodies are few and
 * are paired by AABB in world.c, so this file owns only the static structure.
 */

#include "physics/efx_phys_vec.h"

#include <stdint.h>

typedef struct efx_bvh_node {
    efx_aabb bounds;
    int32_t left;     /* child index, or -1 for a leaf */
    int32_t right;    /* child index, or -1 for a leaf */
    int32_t start;    /* leaf: first index into tri_order */
    int32_t count;    /* leaf: triangle count */
} efx_bvh_node;

typedef struct efx_phys_mesh {
    efx_vec3 *verts;    /* owned */
    int vert_count;
    uint32_t *indices;  /* owned; 3 per triangle */
    int tri_count;
    efx_bvh_node *nodes;
    int node_count;
    int32_t *tri_order; /* owned permutation of triangle indices */
    efx_aabb bounds;
} efx_phys_mesh;

/* builds a mesh from a positions blob (3 floats/vertex) and optional indices
 * (3 per triangle; NULL means non-indexed, tri_count = vert_count/3). Returns
 * NULL on invalid/empty input or allocation failure. */
efx_phys_mesh *efx_phys_mesh_create(const float *positions, int vert_count,
                                    const uint32_t *indices, int tri_count);

void efx_phys_mesh_free(efx_phys_mesh *m);

/* world-space AABB of the whole mesh */
void efx_phys_mesh_bounds(const efx_phys_mesh *m, efx_aabb *out);

/* reads the three vertices of a triangle into v[0..2] */
void efx_phys_mesh_tri(const efx_phys_mesh *m, int tri, efx_vec3 v[3]);

/* visits every triangle whose AABB overlaps `box`. The callback returns
 * nonzero to stop the traversal. */
typedef int (*efx_phys_tri_cb)(void *ud, const efx_phys_mesh *m, int tri);
void efx_phys_mesh_query_aabb(const efx_phys_mesh *m, efx_aabb box,
                              efx_phys_tri_cb cb, void *ud);

#endif /* EFX_PHYS_BROADPHASE_H */
