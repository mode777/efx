#include "physics/shape.h"

#include "physics/broadphase.h"

efx_shape efx_shape_sphere(float radius) {
    efx_shape s;
    s.type = EFX_PHYS_SHAPE_SPHERE;
    s.radius = radius;
    s.half = efx_v3(0, 0, 0);
    s.half_height = 0;
    s.height = 0;
    s.mesh = NULL;
    return s;
}

efx_shape efx_shape_box(efx_vec3 size) {
    efx_shape s;
    s.type = EFX_PHYS_SHAPE_BOX;
    s.radius = 0;
    s.half = efx_v3_scale(size, 0.5f);
    s.half_height = 0;
    s.height = 0;
    s.mesh = NULL;
    return s;
}

efx_shape efx_shape_capsule(float radius, float height) {
    efx_shape s;
    s.type = EFX_PHYS_SHAPE_CAPSULE;
    s.radius = radius;
    s.half = efx_v3(0, 0, 0);
    s.half_height = height * 0.5f - radius;
    if (s.half_height < 0) s.half_height = 0;
    s.height = height;
    s.mesh = NULL;
    return s;
}

void efx_shape_capsule_segment(const efx_shape *s, efx_vec3 center,
                               efx_vec3 *a, efx_vec3 *b) {
    float hh = s->half_height;
    if (a) *a = efx_v3(center.x, center.y - hh, center.z);
    if (b) *b = efx_v3(center.x, center.y + hh, center.z);
}

void efx_shape_bounds(const efx_shape *s, efx_vec3 position, efx_aabb *out) {
    switch (s->type) {
    case EFX_PHYS_SHAPE_SPHERE:
        *out = efx_aabb_expand(
            (efx_aabb){position, position}, s->radius);
        break;
    case EFX_PHYS_SHAPE_BOX:
        out->min = efx_v3_sub(position, s->half);
        out->max = efx_v3_add(position, s->half);
        break;
    case EFX_PHYS_SHAPE_CAPSULE: {
        /* vertical capsule: half extents (r, half_height + r, r) */
        efx_vec3 e = efx_v3(s->radius, s->half_height + s->radius, s->radius);
        out->min = efx_v3_sub(position, e);
        out->max = efx_v3_add(position, e);
        break;
    }
    case EFX_PHYS_SHAPE_MESH:
        if (s->mesh) {
            efx_phys_mesh_bounds(s->mesh, out);
        } else {
            *out = efx_aabb_empty();
        }
        break;
    default:
        *out = efx_aabb_empty();
        break;
    }
}

int efx_shape_valid(const efx_shape *s) {
    switch (s->type) {
    case EFX_PHYS_SHAPE_SPHERE:
        return s->radius > 0 && isfinite(s->radius);
    case EFX_PHYS_SHAPE_BOX:
        return s->half.x > 0 && s->half.y > 0 && s->half.z > 0 &&
               isfinite(s->half.x) && isfinite(s->half.y) && isfinite(s->half.z);
    case EFX_PHYS_SHAPE_CAPSULE:
        return s->radius > 0 && s->height >= 2 * s->radius &&
               isfinite(s->radius) && isfinite(s->height);
    case EFX_PHYS_SHAPE_MESH:
        return s->mesh != NULL;
    default:
        return 0;
    }
}
