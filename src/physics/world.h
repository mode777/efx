#ifndef EFX_PHYS_WORLD_H
#define EFX_PHYS_WORLD_H

/*
 * F12 world model (design D2/D3): one engine-owned world holding every
 * collider, keyed by a stable monotonically increasing id, with a per-slot
 * free list. The solver and character controller operate on this model; the
 * public C surface in physics.h is implemented over it.
 */

#include "physics/broadphase.h"
#include "physics/narrow.h"
#include "physics/physics.h"

/* collider kinds */
#define EFX_PBODY_STATIC 0
#define EFX_PBODY_DYNAMIC 1

/* solver tuning (design D6, open questions settled here) */
#define EFX_PHYS_PEN_SLOP 0.001f
#define EFX_PHYS_BAUMGARTE 0.0f
#define EFX_PHYS_RESTITUTION_THRESHOLD 1.0f
#define EFX_PHYS_MAX_CORRECTION 0.2f
#define EFX_PHYS_MAX_DT 0.1f
#define EFX_PHYS_BROAD_MARGIN 0.02f

typedef struct efx_contact_record {
    efx_phys_body other;          /* 0 when a static mesh */
    efx_phys_character character; /* nonzero when the other is a character */
    int sensor;
    efx_vec3 normal;
    efx_vec3 point;
    float depth;
    float impulse;
} efx_contact_record;

typedef struct efx_pbody {
    int alive;
    int used; /* slot has ever been used (distinguishes free slots) */
    uint32_t id;
    int kind; /* EFX_PBODY_* */
    int sensor;
    int owns_mesh;
    efx_shape shape;
    efx_vec3 position;
    efx_vec3 velocity;
    float mass;
    float inv_mass;
    float friction;
    float restitution;
    uint32_t layer, mask;
    efx_vec3 force;
    efx_aabb aabb;
    efx_contact_record *contacts;
    int contact_count;
    int contact_cap;
} efx_pbody;

typedef struct efx_pcharacter {
    int alive;
    int used;
    uint32_t id;
    efx_vec3 position;
    efx_vec3 velocity;
    float radius;
    float half_height; /* segment half-length */
    efx_vec3 up;
    float floor_cos;   /* cos(floorMaxAngle) */
    float floor_snap_length;
    float step_height;
    float safe_margin;
    int max_slides;
    uint32_t layer, mask;
    int on_floor;
    efx_vec3 floor_normal;
    efx_move_collision *collisions;
    int collision_count;
    int collision_cap;
} efx_pcharacter;

/* one generated contact for a step; normal pushes a away from b */
typedef struct efx_contact_pair {
    int a_type; /* 0 body, 1 character */
    int a_index;
    int b_type;
    int b_index;
    int sensor;
    efx_vec3 normal;
    efx_vec3 point;
    float depth;
    float friction;
    float restitution;
    float bounce; /* target separating speed from restitution (pre-solve) */
    float normal_impulse;
    float tangent_impulse;
} efx_contact_pair;

struct efx_physics_world {
    efx_pbody *bodies;
    int body_count;
    int body_cap;
    int *body_free;
    int body_free_count;
    int body_free_cap;

    efx_pcharacter *chars;
    int char_count;
    int char_cap;
    int *char_free;
    int char_free_count;
    int char_free_cap;

    efx_phys_mesh **meshes;
    int mesh_count;
    int mesh_cap;

    efx_contact_pair *pairs;
    int pair_count;
    int pair_cap;

    uint32_t next_id;
    efx_vec3 gravity;
    int iterations;
};

/* internal lookups (-1 when dead) */
int efx_world_body_slot(const efx_physics_world *w, efx_phys_body b);
int efx_world_char_slot(const efx_physics_world *w, efx_phys_character c);

/* internal free-list slot allocators (shared by world.c and character.c) */
int efx_world_alloc_body_slot(efx_physics_world *w);
int efx_world_alloc_char_slot(efx_physics_world *w);

/* layers: both directions must include each other */
static inline int efx_world_layers_match(uint32_t la, uint32_t ma, uint32_t lb,
                                         uint32_t mb) {
    return (la & mb) != 0 && (lb & ma) != 0;
}

/* solver: applies sequential impulses + Baumgarte correction across pairs.
 * Called by step once contacts are generated. */
void efx_solver_solve(efx_physics_world *w, float dt);

/* contact generation: fills w->pairs for the current positions and resets
 * each dynamic body's contact report. */
void efx_world_generate_contacts(efx_physics_world *w);
void efx_world_report_contacts(efx_physics_world *w);

/* character controller entry point (declared here to avoid a cycle) */
int efx_character_move(efx_physics_world *w, efx_pcharacter *ch,
                       efx_vec3 motion, efx_move_result *out);

#endif /* EFX_PHYS_WORLD_H */
