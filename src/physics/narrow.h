#ifndef EFX_PHYS_NARROW_H
#define EFX_PHYS_NARROW_H

/*
 * F12 narrowphase (design D4/D5): analytic primitive overlaps, ray casts, and
 * conservative-advancement sweeps over the fixed shape set (sphere, axis-
 * aligned box, vertical capsule, triangle). Contact normals point from the
 * second shape (B, usually the static/kinematic one) toward the first (A), so
 * the solver pushes A along +normal.
 */

#include "physics/efx_phys_vec.h"
#include "physics/shape.h"

typedef struct efx_narrow_contact {
    efx_vec3 normal; /* unit; pushes A away from B */
    efx_vec3 point;  /* world contact point */
    float depth;     /* positive penetration */
} efx_narrow_contact;

/* ---- analytic overlaps: return contact count (0 or 1) ---- */

int efx_narrow_sphere_sphere(efx_vec3 pa, float ra, efx_vec3 pb, float rb,
                             efx_narrow_contact *out);
int efx_narrow_sphere_box(efx_vec3 pa, float ra, efx_vec3 pb, efx_vec3 hb,
                          efx_narrow_contact *out);
int efx_narrow_sphere_capsule(efx_vec3 pa, float ra, efx_vec3 pb, float hhb,
                              float rb, efx_narrow_contact *out);
int efx_narrow_box_box(efx_vec3 pa, efx_vec3 ha, efx_vec3 pb, efx_vec3 hb,
                       efx_narrow_contact *out);
int efx_narrow_capsule_capsule(efx_vec3 pa, float hha, float ra, efx_vec3 pb,
                               float hhb, float rb, efx_narrow_contact *out);
int efx_narrow_sphere_triangle(efx_vec3 pa, float ra, efx_vec3 v0, efx_vec3 v1,
                               efx_vec3 v2, efx_narrow_contact *out);
int efx_narrow_box_triangle(efx_vec3 pa, efx_vec3 ha, efx_vec3 v0, efx_vec3 v1,
                            efx_vec3 v2, efx_narrow_contact *out);
int efx_narrow_capsule_triangle(efx_vec3 ca, float hh, float ra, efx_vec3 v0,
                                efx_vec3 v1, efx_vec3 v2,
                                efx_narrow_contact *out);

/* ---- generic dispatch: A (sphere/box/capsule) vs B (sphere/box/capsule) or
 * vs a triangle; normal pushes A ---- */

int efx_narrow_overlap(const efx_shape *a, efx_vec3 pa, const efx_shape *b,
                       efx_vec3 pb, efx_narrow_contact *out);
int efx_narrow_shape_triangle(const efx_shape *a, efx_vec3 pa, efx_vec3 v0,
                              efx_vec3 v1, efx_vec3 v2,
                              efx_narrow_contact *out);

/* ---- ray casts: return 1 on hit within maxd; t is the parameter along d
 * (d need not be normalized, t in [0, maxd]); n is the outward normal ---- */

int efx_narrow_ray_sphere(efx_vec3 o, efx_vec3 d, efx_vec3 c, float r,
                          float maxd, float *t, efx_vec3 *n);
int efx_narrow_ray_box(efx_vec3 o, efx_vec3 d, efx_vec3 c, efx_vec3 h,
                       float maxd, float *t, efx_vec3 *n);
int efx_narrow_ray_capsule(efx_vec3 o, efx_vec3 d, efx_vec3 c, float hh,
                           float r, float maxd, float *t, efx_vec3 *n);
int efx_narrow_ray_triangle(efx_vec3 o, efx_vec3 d, efx_vec3 v0, efx_vec3 v1,
                            efx_vec3 v2, float maxd, float *t, efx_vec3 *n);
int efx_narrow_ray_shape(efx_vec3 o, efx_vec3 d, const efx_shape *s,
                         efx_vec3 pos, float maxd, float *t, efx_vec3 *n);

/* ---- conservative-advancement sweeps: shape A (sphere/box/capsule) swept
 * from `from` by `motion` against B; returns 1 on hit with t in [0,1], the
 * contact point and the normal pushing A back along its motion ---- */

int efx_narrow_sweep(const efx_shape *a, efx_vec3 from, efx_vec3 motion,
                     const efx_shape *b, efx_vec3 pb, float *t,
                     efx_vec3 *point, efx_vec3 *normal);
int efx_narrow_sweep_triangle(const efx_shape *a, efx_vec3 from,
                              efx_vec3 motion, efx_vec3 v0, efx_vec3 v1,
                              efx_vec3 v2, float *t, efx_vec3 *point,
                              efx_vec3 *normal);

#endif /* EFX_PHYS_NARROW_H */
