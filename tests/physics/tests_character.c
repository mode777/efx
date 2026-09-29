#include "test_support.h"

static efx_character_desc test_char_desc(efx_vec3 pos) {
    efx_character_desc d;
    memset(&d, 0, sizeof(d));
    d.radius = 0.4f;
    d.height = 1.8f;
    d.position = pos;
    d.up = efx_v3(0, 1, 0);
    d.floor_max_angle = 45.0f;
    d.floor_snap_length = 0.1f;
    d.step_height = 0.3f;
    d.safe_margin = 0.001f;
    d.max_slides = 6;
    d.layer = 0xFFFFFFFFu;
    d.mask = 0xFFFFFFFFu;
    return d;
}

/* ---- 7.1 creation + validation + destroy ---- */
int t_character_create(void) {
    efx_physics_world *w = test_world();
    efx_character_desc d = test_char_desc(efx_v3(0, 1, 0));
    efx_phys_character ch = efx_physics_create_character(w, &d);
    CHECK(ch != 0);
    CHECK(efx_physics_character_alive(w, ch));
    efx_vec3 p;
    CHECK(efx_physics_character_position(w, ch, &p));
    CHECK(v3_near(p, efx_v3(0, 1, 0), 1e-5f));
    CHECK(efx_physics_character_on_floor(w, ch) == 0);

    /* invalid descriptors are rejected by the core */
    d.height = 0.5f;
    CHECK(efx_physics_create_character(w, &d) == 0);
    d = test_char_desc(efx_v3(0, 1, 0));
    d.max_slides = 0;
    CHECK(efx_physics_create_character(w, &d) == 0);
    d = test_char_desc(efx_v3(0, 1, 0));
    d.up = efx_v3(0, 0, 0);
    CHECK(efx_physics_create_character(w, &d) == 0);

    CHECK(efx_physics_destroy_character(w, ch) == 1);
    CHECK(efx_physics_destroy_character(w, ch) == 0);
    efx_move_result mr;
    CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(1, 0, 0), &mr) ==
          0); /* use after destroy */
    efx_physics_world_free(w);
    return 0;
}

/* ---- 7.2 move and slide ---- */
int t_move_slide(void) {
    efx_physics_world *w = test_world();
    efx_character_desc d = test_char_desc(efx_v3(0, 1, 0));
    efx_phys_character ch = efx_physics_create_character(w, &d);
    efx_move_result mr;

    /* free move is applied in full */
    CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(0, 0, 2), &mr));
    CHECK(v3_near(mr.position, efx_v3(0, 1, 2), 1e-4f));
    CHECK(!mr.on_floor && !mr.on_wall && !mr.on_ceiling);

    /* wall: motion diagonal into a wall at x=2 loses the into-wall part */
    test_static_box(w, efx_v3(2, 1, 0), efx_v3(0.5f, 3, 4));
    efx_physics_character_set_position(w, ch, efx_v3(0, 1, 0));
    CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(2, 0, 1), &mr));
    CHECK(mr.on_wall);
    CHECK(mr.position.x < 1.5f);   /* stopped before the wall face */
    CHECK(mr.position.z > 0.5f);   /* tangential component preserved */
    CHECK(mr.collision_count >= 1);

    /* dynamic bodies do not block moveAndSlide */
    efx_phys_body crate = test_dynamic_box(w, efx_v3(4, 1, 0),
                                           efx_v3(1, 1, 1), 1.0f);
    efx_physics_character_set_position(w, ch, efx_v3(3, 1, 0));
    CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(2, 0, 0), &mr));
    CHECK(mr.position.x > 4.5f); /* passed through the crate */
    (void)crate;
    efx_physics_world_free(w);
    return 0;
}

/* ---- 7.3 floor/wall/ceiling classification ---- */
int t_floor_classify(void) {
    /* floor */
    {
        efx_physics_world *w = test_world();
        test_static_box(w, efx_v3(0, -0.5f, 0), efx_v3(20, 1, 20));
        efx_character_desc d = test_char_desc(efx_v3(0, 2, 0));
        efx_phys_character ch = efx_physics_create_character(w, &d);
        efx_move_result mr;
        CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(0, -1.5f, 0),
                                                   &mr));
        CHECK(mr.on_floor);
        CHECK(mr.floor_normal.y > 0.9f);
        CHECK(!mr.on_ceiling);
        efx_physics_world_free(w);
    }
    /* steep slope is a wall */
    {
        efx_physics_world *w = test_world();
        float verts[9] = {0, 0, -5, 0, 0, 5, 5, 8, 0};
        uint32_t idx[3] = {0, 1, 2};
        efx_static_mesh_desc md;
        memset(&md, 0, sizeof(md));
        md.friction = 0.6f;
        md.layer = 0xFFFFFFFFu;
        md.mask = 0xFFFFFFFFu;
        efx_physics_create_static_mesh(w, verts, 3, idx, 1, &md);
        efx_character_desc d = test_char_desc(efx_v3(-2, 1, 0));
        efx_phys_character ch = efx_physics_create_character(w, &d);
        efx_move_result mr;
        CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(3, 0, 0),
                                                   &mr));
        CHECK(mr.on_wall);
        CHECK(!mr.on_floor);
        efx_physics_world_free(w);
    }
    /* ceiling */
    {
        efx_physics_world *w = test_world();
        test_static_box(w, efx_v3(0, 2, 0), efx_v3(4, 0.5f, 4));
        efx_character_desc d = test_char_desc(efx_v3(0, 0.8f, 0));
        efx_phys_character ch = efx_physics_create_character(w, &d);
        efx_move_result mr;
        CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(0, 0.6f, 0),
                                                   &mr));
        CHECK(mr.on_ceiling);
        efx_physics_world_free(w);
    }
    return 0;
}

/* ---- 7.4 floor snapping ---- */
int t_floor_snap(void) {
    efx_physics_world *w = test_world();
    /* gentle ramp: rises with +x (normal ~20 degrees from up) */
    float verts[9] = {0, 0, -5, 0, 0, 5, 5, 1.8f, 0};
    uint32_t idx[3] = {0, 1, 2};
    efx_static_mesh_desc md;
    memset(&md, 0, sizeof(md));
    md.friction = 0.6f;
    md.layer = 0xFFFFFFFFu;
    md.mask = 0xFFFFFFFFu;
    efx_physics_create_static_mesh(w, verts, 3, idx, 1, &md);

    efx_character_desc d = test_char_desc(efx_v3(2, 3, 0));
    efx_phys_character ch = efx_physics_create_character(w, &d);
    efx_move_result mr;
    /* land on the ramp */
    CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(0, -2, 0), &mr));
    CHECK(mr.on_floor);
    /* walk downhill: snapping keeps it attached */
    CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(-1, -0.05f, 0),
                                               &mr));
    CHECK(mr.on_floor);

    /* jumping detaches */
    CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(0, 2, 0), &mr));
    CHECK(!mr.on_floor);
    CHECK(efx_physics_character_on_floor(w, ch) == 0);
    efx_physics_world_free(w);
    return 0;
}

/* ---- 7.5 step-up ---- */
int t_step_up(void) {
    /* low ledge is climbed */
    {
        efx_physics_world *w = test_world();
        test_static_box(w, efx_v3(0, -0.5f, 0), efx_v3(20, 1, 20));
        test_static_box(w, efx_v3(2, 0.1f, 0), efx_v3(1, 0.2f, 4));
        efx_character_desc d = test_char_desc(efx_v3(0, 0.9f, 0));
        efx_phys_character ch = efx_physics_create_character(w, &d);
        efx_move_result mr;
        /* settle onto the ground first so step-up is active */
        CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(0, -0.1f, 0),
                                                   &mr));
        CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(2, 0, 0), &mr));
        CHECK(mr.position.x > 1.5f);
        CHECK(mr.position.y > 1.0f); /* ended on top of the ledge */
        efx_physics_world_free(w);
    }
    /* tall obstacle blocks */
    {
        efx_physics_world *w = test_world();
        test_static_box(w, efx_v3(0, -0.5f, 0), efx_v3(20, 1, 20));
        test_static_box(w, efx_v3(2, 1.0f, 0), efx_v3(1, 2, 4));
        efx_character_desc d = test_char_desc(efx_v3(0, 0.9f, 0));
        efx_phys_character ch = efx_physics_create_character(w, &d);
        efx_move_result mr;
        CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(0, -0.1f, 0),
                                                   &mr));
        CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(2, 0, 0), &mr));
        CHECK(mr.position.x < 1.5f);
        CHECK(mr.on_wall);
        efx_physics_world_free(w);
    }
    /* step-up disabled blocks a low ledge */
    {
        efx_physics_world *w = test_world();
        test_static_box(w, efx_v3(0, -0.5f, 0), efx_v3(20, 1, 20));
        test_static_box(w, efx_v3(2, 0.1f, 0), efx_v3(1, 0.2f, 4));
        efx_character_desc d = test_char_desc(efx_v3(0, 0.9f, 0));
        d.step_height = 0;
        efx_phys_character ch = efx_physics_create_character(w, &d);
        efx_move_result mr;
        CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(0, -0.1f, 0),
                                                   &mr));
        CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(2, 0, 0), &mr));
        CHECK(mr.position.x < 1.5f);
        CHECK(mr.on_wall);
        efx_physics_world_free(w);
    }
    return 0;
}

/* ---- 7.6 one-way push ---- */
int t_character_push(void) {
    efx_physics_world *w = test_world();
    test_static_box(w, efx_v3(0, -0.5f, 0), efx_v3(20, 1, 20));
    efx_phys_body crate = test_dynamic_box(w, efx_v3(1.5f, 0.5f, 0),
                                           efx_v3(1, 1, 1), 1.0f);
    efx_character_desc d = test_char_desc(efx_v3(0, 0.9f, 0));
    efx_phys_character ch = efx_physics_create_character(w, &d);
    efx_move_result mr;
    CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(0, -0.1f, 0), &mr));
    efx_vec3 before;
    efx_physics_body_position(w, crate, &before);
    CHECK(efx_physics_character_move_and_slide(w, ch, efx_v3(1.2f, 0, 0), &mr));
    /* the character passed through; the crate is then pushed by step. The
     * character's script-set velocity is what drives the one-way push. */
    efx_physics_character_set_velocity(w, ch, efx_v3(3, 0, 0));
    test_step_n(w, 1.0f / 60.0f, 20);
    efx_vec3 after, cv;
    efx_physics_body_position(w, crate, &after);
    efx_physics_body_velocity(w, crate, &cv);
    CHECK(after.x > before.x + 0.05f || cv.x > 0.1f);

    /* a fast dynamic body does not move the character */
    efx_phys_body bullet = test_dynamic_sphere(w, efx_v3(0, 1, 5), 0.3f, 1.0f);
    efx_physics_body_set_velocity(w, bullet, efx_v3(0, 0, -8));
    efx_physics_character_set_position(w, ch, efx_v3(0, 0.9f, 0));
    efx_vec3 cbefore;
    efx_physics_character_position(w, ch, &cbefore);
    test_step_n(w, 1.0f / 60.0f, 20);
    efx_vec3 cafter;
    efx_physics_character_position(w, ch, &cafter);
    CHECK(v3_near(cbefore, cafter, 1e-5f));
    efx_physics_world_free(w);
    return 0;
}
