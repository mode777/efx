#ifndef EFX_PHYSICS_H
#define EFX_PHYSICS_H

/*
 * F12 public C surface for the collision + linear-dynamics + character core
 * (design D1/D3/D9). This is the pure-C API the two script bindings (desktop
 * `src/api`, web `src/web`) call; it depends only on libc and the physics
 * module's own math, never the renderer, platform layer, script runtime, or
 * GLM (ADR 0040).
 *
 * Handles are opaque stable uint32 ids; 0 means "none" (a static mesh contact
 * reports body 0, matching the script contract's `null`).
 */

#include <stdint.h>

#include "physics/efx_phys_vec.h"
#include "physics/shape.h"

typedef struct efx_physics_world efx_physics_world;
typedef uint32_t efx_phys_body;
typedef uint32_t efx_phys_character;

/* world */
efx_physics_world *efx_physics_world_new(void);
void efx_physics_world_free(efx_physics_world *w);
void efx_physics_clear(efx_physics_world *w);
void efx_physics_set_gravity(efx_physics_world *w, efx_vec3 g);
efx_vec3 efx_physics_gravity(const efx_physics_world *w);
void efx_physics_set_iterations(efx_physics_world *w, int iterations);
int efx_physics_iterations(const efx_physics_world *w);
void efx_physics_step(efx_physics_world *w, float dt);

/* body descriptors */
typedef struct efx_body_desc {
    int dynamic; /* 1 = impulse-simulated, 0 = static */
    int sensor;
    efx_shape shape;
    efx_vec3 position;
    float mass;
    float friction;
    float restitution;
    uint32_t layer, mask;
} efx_body_desc;

typedef struct efx_static_mesh_desc {
    efx_vec3 position;
    int sensor;
    float friction;
    float restitution;
    uint32_t layer, mask;
} efx_static_mesh_desc;

typedef struct efx_character_desc {
    float radius;
    float height;
    efx_vec3 position;
    efx_vec3 up;
    float floor_max_angle; /* degrees */
    float floor_snap_length;
    float step_height;
    float safe_margin;
    int max_slides;
    uint32_t layer, mask;
} efx_character_desc;

typedef struct efx_contact_info {
    efx_phys_body body;           /* 0 when the other is a static mesh */
    efx_phys_character character; /* nonzero when the other is a character */
    int sensor;
    efx_vec3 normal;
    efx_vec3 point;
    float depth;
    float impulse;
} efx_contact_info;

typedef struct efx_ray_hit {
    efx_vec3 point;
    efx_vec3 normal;
    float distance;
    efx_phys_body body;
    efx_phys_character character;
    int sensor;
} efx_ray_hit;

typedef struct efx_overlap_hit {
    efx_phys_body body;
    efx_phys_character character;
    int sensor;
} efx_overlap_hit;

typedef struct efx_shape_hit {
    efx_vec3 point;
    efx_vec3 normal;
    float fraction;
    efx_phys_body body;
    efx_phys_character character;
    int sensor;
} efx_shape_hit;

typedef struct efx_move_collision {
    efx_phys_body body;
    int sensor;
    efx_vec3 normal;
    efx_vec3 point;
} efx_move_collision;

typedef struct efx_move_result {
    efx_vec3 position;
    int on_floor;
    int on_wall;
    int on_ceiling;
    efx_vec3 floor_normal;
    int collision_count;
} efx_move_result;

/* factories (return 0 on failure) */
efx_phys_body efx_physics_create_body(efx_physics_world *w,
                                      const efx_body_desc *desc);
efx_phys_body efx_physics_create_static_mesh(efx_physics_world *w,
                                             const float *positions,
                                             int vert_count,
                                             const uint32_t *indices,
                                             int tri_count,
                                             const efx_static_mesh_desc *desc);
efx_phys_character efx_physics_create_character(efx_physics_world *w,
                                                const efx_character_desc *desc);

/* destruction: returns 1 when it released something, 0 when already gone */
int efx_physics_destroy_body(efx_physics_world *w, efx_phys_body b);
int efx_physics_destroy_character(efx_physics_world *w, efx_phys_character c);
int efx_physics_body_alive(const efx_physics_world *w, efx_phys_body b);
int efx_physics_character_alive(const efx_physics_world *w,
                                efx_phys_character c);

/* body state (all return 0 when the handle is dead) */
int efx_physics_body_position(efx_physics_world *w, efx_phys_body b,
                              efx_vec3 *out);
int efx_physics_body_set_position(efx_physics_world *w, efx_phys_body b,
                                  efx_vec3 p);
int efx_physics_body_velocity(efx_physics_world *w, efx_phys_body b,
                              efx_vec3 *out);
int efx_physics_body_set_velocity(efx_physics_world *w, efx_phys_body b,
                                  efx_vec3 v);
int efx_physics_body_apply_impulse(efx_physics_world *w, efx_phys_body b,
                                   efx_vec3 impulse);
int efx_physics_body_apply_force(efx_physics_world *w, efx_phys_body b,
                                 efx_vec3 force);
int efx_physics_body_is_dynamic(const efx_physics_world *w, efx_phys_body b);
int efx_physics_body_is_sensor(const efx_physics_world *w, efx_phys_body b);
int efx_physics_body_is_mesh(const efx_physics_world *w, efx_phys_body b);
int efx_physics_body_contact_count(efx_physics_world *w, efx_phys_body b);
int efx_physics_body_contact(efx_physics_world *w, efx_phys_body b, int index,
                             efx_contact_info *out);

/* character state */
int efx_physics_character_position(efx_physics_world *w, efx_phys_character c,
                                   efx_vec3 *out);
int efx_physics_character_set_position(efx_physics_world *w,
                                       efx_phys_character c, efx_vec3 p);
int efx_physics_character_velocity(efx_physics_world *w, efx_phys_character c,
                                   efx_vec3 *out);
int efx_physics_character_set_velocity(efx_physics_world *w,
                                       efx_phys_character c, efx_vec3 v);
int efx_physics_character_on_floor(efx_physics_world *w, efx_phys_character c);
int efx_physics_character_move_and_slide(efx_physics_world *w,
                                         efx_phys_character c, efx_vec3 motion,
                                         efx_move_result *out);
int efx_physics_move_collision_count(efx_physics_world *w,
                                     efx_phys_character c);
int efx_physics_move_collision(efx_physics_world *w, efx_phys_character c,
                               int index, efx_move_collision *out);

/* queries. raycast: direction normalized by the callee; max_distance is a
 * positive distance. `all` returns every hit sorted ascending (capped at
 * `cap`), otherwise at most the nearest. `sensors` includes sensor colliders.
 * Returns the number of hits written. */
int efx_physics_raycast(efx_physics_world *w, efx_vec3 origin, efx_vec3 dir,
                        float max_distance, uint32_t mask, int sensors, int all,
                        efx_ray_hit *out, int cap);

/* overlap: count then fill (call with out=NULL/cap=0 for the count). */
int efx_physics_overlap(efx_physics_world *w, const efx_shape *shape,
                        efx_vec3 position, uint32_t mask, int sensors,
                        efx_overlap_hit *out, int cap);

/* shape cast: sweeps `shape` from `from` by `motion`; returns 1 on a hit.
 * `sensors` includes sensor colliders. */
int efx_physics_shape_cast(efx_physics_world *w, const efx_shape *shape,
                           efx_vec3 from, efx_vec3 motion, uint32_t mask,
                           int sensors, efx_shape_hit *out);

/* shape mesh helper: tells whether a script mesh value may be used here */
int efx_physics_shape_is_mesh(const efx_shape *s);

#endif /* EFX_PHYSICS_H */
