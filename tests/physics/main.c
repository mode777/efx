/*
 * F12 headless physics test runner. Usage: efx_physics_tests [<case>]
 * With no argument every case runs. Exit 0 = pass.
 */
#include "../test_support.h"

int t_support_basic(void);
int t_shapes(void);
int t_narrow_triangle(void);
int t_narrow_pairs(void);
int t_ray_shapes(void);
int t_sweep_shapes(void);
int t_bvh_query(void);
int t_world_registry(void);
int t_broadphase_filter(void);
int t_determinism(void);
int t_integrate(void);
int t_contact_manifold(void);
int t_restitution_friction(void);
int t_settle(void);
int t_step_dt(void);
int t_thin_floor_large_dt(void);
int t_fast_body_thin_floor(void);
int t_force_substep(void);
int t_stress(void);
int t_sensors(void);
int t_contact_report(void);
int t_raycast_query(void);
int t_overlap_query(void);
int t_shapecast_query(void);
int t_character_create(void);
int t_move_slide(void);
int t_floor_classify(void);
int t_floor_snap(void);
int t_step_up(void);
int t_character_push(void);

static const efx_test_case cases[] = {
    EFX_CASE(t_support_basic),
    EFX_CASE(t_shapes),
    EFX_CASE(t_narrow_triangle),
    EFX_CASE(t_narrow_pairs),
    EFX_CASE(t_ray_shapes),
    EFX_CASE(t_sweep_shapes),
    EFX_CASE(t_bvh_query),
    EFX_CASE(t_world_registry),
    EFX_CASE(t_broadphase_filter),
    EFX_CASE(t_determinism),
    EFX_CASE(t_integrate),
    EFX_CASE(t_contact_manifold),
    EFX_CASE(t_restitution_friction),
    EFX_CASE(t_settle),
    EFX_CASE(t_step_dt),
    EFX_CASE(t_thin_floor_large_dt),
    EFX_CASE(t_fast_body_thin_floor),
    EFX_CASE(t_force_substep),
    EFX_CASE(t_stress),
    EFX_CASE(t_sensors),
    EFX_CASE(t_contact_report),
    EFX_CASE(t_raycast_query),
    EFX_CASE(t_overlap_query),
    EFX_CASE(t_shapecast_query),
    EFX_CASE(t_character_create),
    EFX_CASE(t_move_slide),
    EFX_CASE(t_floor_classify),
    EFX_CASE(t_floor_snap),
    EFX_CASE(t_step_up),
    EFX_CASE(t_character_push),
};

int main(int argc, char **argv) {
    return efx_test_main(cases, sizeof(cases) / sizeof(cases[0]), argc, argv);
}
