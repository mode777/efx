/*
 * F12 headless physics test runner. Usage: efx_physics_tests [<case>]
 * With no argument every case runs. Exit 0 = pass.
 */
#include <stdio.h>
#include <string.h>

#define CASE(fn) {#fn, fn}

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

static const struct {
    const char *name;
    int (*fn)(void);
} cases[] = {
    CASE(t_support_basic),      CASE(t_shapes),
    CASE(t_narrow_triangle),    CASE(t_narrow_pairs),
    CASE(t_ray_shapes),         CASE(t_sweep_shapes),
    CASE(t_bvh_query),          CASE(t_world_registry),
    CASE(t_broadphase_filter),  CASE(t_determinism),
    CASE(t_integrate),          CASE(t_contact_manifold),
    CASE(t_restitution_friction), CASE(t_settle),
    CASE(t_step_dt),            CASE(t_stress),
    CASE(t_sensors),            CASE(t_contact_report),
    CASE(t_raycast_query),      CASE(t_overlap_query),
    CASE(t_shapecast_query),    CASE(t_character_create),
    CASE(t_move_slide),         CASE(t_floor_classify),
    CASE(t_floor_snap),         CASE(t_step_up),
    CASE(t_character_push),
};

int main(int argc, char **argv) {
    int failures = 0;
    if (argc > 1) {
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            if (strcmp(cases[i].name, argv[1]) == 0) {
                return cases[i].fn();
            }
        }
        fprintf(stderr, "unknown case: %s\n", argv[1]);
        return 2;
    }
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int rc = cases[i].fn();
        if (rc != 0) {
            fprintf(stderr, "case failed: %s\n", cases[i].name);
            failures++;
        }
    }
    printf("%zu cases, %d failures\n",
           sizeof(cases) / sizeof(cases[0]), failures);
    return failures ? 1 : 0;
}
