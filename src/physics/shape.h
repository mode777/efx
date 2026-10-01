#ifndef EFX_PHYS_SHAPE_H
#define EFX_PHYS_SHAPE_H

/*
 * F12 collision shape descriptors (ADR 0040). The physics core speaks
 * plain C shape structs; the bindings translate the script option objects
 * into these. Four shape types: sphere, axis-aligned box (full extent), a
 * *vertical* capsule (segment + radius), and a triangle mesh collider
 * (borrowed, built by the broadphase).
 */

#include "physics/efx_phys_vec.h"

#define EFX_PHYS_SHAPE_SPHERE 0
#define EFX_PHYS_SHAPE_BOX 1
#define EFX_PHYS_SHAPE_CAPSULE 2
#define EFX_PHYS_SHAPE_MESH 3

struct efx_phys_mesh;

typedef struct efx_shape {
    int type;
    float radius;      /* sphere/capsule */
    efx_vec3 half;     /* box half-extents */
    float half_height; /* capsule segment half-length (height/2 - radius) */
    float height;      /* capsule total tip-to-tip height */
    const struct efx_phys_mesh *mesh; /* borrowed; EFX_PHYS_SHAPE_MESH only */
} efx_shape;

/* fills a sphere descriptor */
efx_shape efx_shape_sphere(float radius);

/* fills a box descriptor from its full [x,y,z] extent */
efx_shape efx_shape_box(efx_vec3 size);

/* fills a capsule descriptor from total height + radius */
efx_shape efx_shape_capsule(float radius, float height);

/* capsule segment endpoints for a capsule centered at `center`; the segment
 * is always on the world up (Y) axis for shape-vs-body tests */
void efx_shape_capsule_segment(const efx_shape *s, efx_vec3 center,
                               efx_vec3 *a, efx_vec3 *b);

/* world AABB of a shape placed at `position` (mesh shapes need their bounds) */
void efx_shape_bounds(const efx_shape *s, efx_vec3 position, efx_aabb *out);

/* 1 when the descriptor is structurally valid (used before world insertion) */
int efx_shape_valid(const efx_shape *s);

#endif /* EFX_PHYS_SHAPE_H */
