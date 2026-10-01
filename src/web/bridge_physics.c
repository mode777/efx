#include "bridge_internal.h"

/* ================================================= F12 physics bridge */

static efx_physics_world *web_physics(void) {
    if (!W.physics) {
        W.physics = efx_physics_world_new();
    }
    return W.physics;
}

static void web_shape_from(int type, double r, double hx, double hy, double hz,
                           double height, efx_shape *out) {
    if (type == 0) {
        *out = efx_shape_sphere((float)r);
    } else if (type == 1) {
        *out = efx_shape_box(efx_v3((float)hx, (float)hy, (float)hz));
    } else if (type == 2) {
        *out = efx_shape_capsule((float)r, (float)height);
    } else {
        *out = efx_shape_sphere(0);
        out->type = EFX_PHYS_SHAPE_MESH;
    }
}

static efx_phys_mesh *web_temp_mesh(double mesh_handle) {
    uint64_t h = (uint64_t)mesh_handle;
    int verts = 0, indices = 0;
    if (!efx_render_mesh_geometry_count(h, &verts, &indices) || verts <= 0 ||
        indices < 3 || indices % 3 != 0) {
        return NULL;
    }
    float *p = malloc((size_t)verts * 3 * sizeof(float));
    uint32_t *idx = malloc((size_t)indices * sizeof(uint32_t));
    if (!p || !idx) {
        free(p);
        free(idx);
        return NULL;
    }
    efx_render_mesh_geometry(h, p, idx);
    efx_phys_mesh *m = efx_phys_mesh_create(p, verts, idx, indices / 3);
    free(p);
    free(idx);
    return m;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_set_gravity(double x, double y,
                                                         double z) {
    efx_physics_set_gravity(web_physics(), efx_v3((float)x, (float)y, (float)z));
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_physics_gravity(int i) {
    efx_vec3 g = efx_physics_gravity(web_physics());
    return i == 0 ? g.x : (i == 1 ? g.y : g.z);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_set_iterations(int n) {
    efx_physics_set_iterations(web_physics(), n);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_iterations(void) {
    return efx_physics_iterations(web_physics());
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_step(double dt) {
    efx_physics_step(web_physics(), (float)dt);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_clear(void) {
    if (W.physics) efx_physics_clear(W.physics);
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_physics_create_body(
    int dynamic, int sensor, int shape_type, double radius, double hx,
    double hy, double hz, double height, double px, double py, double pz,
    double mass, double friction, double restitution, double layer,
    double mask) {
    efx_body_desc d;
    memset(&d, 0, sizeof(d));
    d.dynamic = dynamic;
    d.sensor = sensor;
    web_shape_from(shape_type, radius, hx, hy, hz, height, &d.shape);
    d.position = efx_v3((float)px, (float)py, (float)pz);
    d.mass = (float)mass;
    d.friction = (float)friction;
    d.restitution = (float)restitution;
    d.layer = (uint32_t)layer;
    d.mask = (uint32_t)mask;
    return (double)efx_physics_create_body(web_physics(), &d);
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_physics_create_static_mesh(
    double mesh_handle, double px, double py, double pz, int sensor,
    double friction, double restitution, double layer, double mask) {
    uint64_t h = (uint64_t)mesh_handle;
    int verts = 0, indices = 0;
    if (!efx_render_mesh_geometry_count(h, &verts, &indices) || verts <= 0 ||
        indices < 3 || indices % 3 != 0) {
        return 0;
    }
    float *p = malloc((size_t)verts * 3 * sizeof(float));
    uint32_t *idx = malloc((size_t)indices * sizeof(uint32_t));
    if (!p || !idx) {
        free(p);
        free(idx);
        return 0;
    }
    efx_render_mesh_geometry(h, p, idx);
    efx_static_mesh_desc d;
    memset(&d, 0, sizeof(d));
    d.position = efx_v3((float)px, (float)py, (float)pz);
    d.sensor = sensor;
    d.friction = (float)friction;
    d.restitution = (float)restitution;
    d.layer = (uint32_t)layer;
    d.mask = (uint32_t)mask;
    efx_phys_body b =
        efx_physics_create_static_mesh(web_physics(), p, verts, idx,
                                       indices / 3, &d);
    free(p);
    free(idx);
    return (double)b;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_body_position(double h,
                                                           float *out) {
    efx_physics_body_position(web_physics(), (efx_phys_body)h,
                              (efx_vec3 *)out);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_body_get_velocity(double h,
                                                               float *out) {
    efx_physics_body_velocity(web_physics(), (efx_phys_body)h,
                              (efx_vec3 *)out);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_body_set_velocity(
    double h, double x, double y, double z) {
    efx_physics_body_set_velocity(web_physics(), (efx_phys_body)h,
                                  efx_v3((float)x, (float)y, (float)z));
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_body_apply_impulse(
    double h, double x, double y, double z) {
    return efx_physics_body_apply_impulse(web_physics(), (efx_phys_body)h,
                                          efx_v3((float)x, (float)y, (float)z));
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_body_apply_force(
    double h, double x, double y, double z) {
    return efx_physics_body_apply_force(web_physics(), (efx_phys_body)h,
                                        efx_v3((float)x, (float)y, (float)z));
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_body_contact_count(double h) {
    return efx_physics_body_contact_count(web_physics(), (efx_phys_body)h);
}

/* out: [0..2] normal, [3..5] point, [6] depth, [7] impulse, [8] other body,
 * [9] other character, [10] sensor */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_body_contact(double h, int i,
                                                         double *out) {
    efx_contact_info ci;
    if (!efx_physics_body_contact(web_physics(), (efx_phys_body)h, i, &ci)) {
        return 0;
    }
    out[0] = ci.normal.x;
    out[1] = ci.normal.y;
    out[2] = ci.normal.z;
    out[3] = ci.point.x;
    out[4] = ci.point.y;
    out[5] = ci.point.z;
    out[6] = ci.depth;
    out[7] = ci.impulse;
    out[8] = (double)ci.body;
    out[9] = (double)ci.character;
    out[10] = ci.sensor;
    return 1;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_body_destroy(double h) {
    efx_physics_destroy_body(web_physics(), (efx_phys_body)h);
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_physics_create_character(
    double radius, double height, double px, double py, double pz, double ux,
    double uy, double uz, double floor_max_angle, double floor_snap_length,
    double step_height, double safe_margin, int max_slides, double layer,
    double mask) {
    efx_character_desc d;
    memset(&d, 0, sizeof(d));
    d.radius = (float)radius;
    d.height = (float)height;
    d.position = efx_v3((float)px, (float)py, (float)pz);
    d.up = efx_v3((float)ux, (float)uy, (float)uz);
    d.floor_max_angle = (float)floor_max_angle;
    d.floor_snap_length = (float)floor_snap_length;
    d.step_height = (float)step_height;
    d.safe_margin = (float)safe_margin;
    d.max_slides = max_slides;
    d.layer = (uint32_t)layer;
    d.mask = (uint32_t)mask;
    return (double)efx_physics_create_character(web_physics(), &d);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_character_position(double h,
                                                                float *out) {
    efx_physics_character_position(web_physics(), (efx_phys_character)h,
                                   (efx_vec3 *)out);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_character_get_velocity(
    double h, float *out) {
    efx_physics_character_velocity(web_physics(), (efx_phys_character)h,
                                   (efx_vec3 *)out);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_character_set_velocity(
    double h, double x, double y, double z) {
    efx_physics_character_set_velocity(web_physics(), (efx_phys_character)h,
                                       efx_v3((float)x, (float)y, (float)z));
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_character_on_floor(double h) {
    return efx_physics_character_on_floor(web_physics(),
                                          (efx_phys_character)h);
}

/* out: [0..2] position, [3] onFloor, [4] onWall, [5] onCeiling,
 * [6..8] floorNormal, [9] collision count */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_character_move(
    double h, double mx, double my, double mz, double *out) {
    efx_move_result mr;
    if (!efx_physics_character_move_and_slide(
            web_physics(), (efx_phys_character)h,
            efx_v3((float)mx, (float)my, (float)mz), &mr)) {
        return 0;
    }
    out[0] = mr.position.x;
    out[1] = mr.position.y;
    out[2] = mr.position.z;
    out[3] = mr.on_floor;
    out[4] = mr.on_wall;
    out[5] = mr.on_ceiling;
    out[6] = mr.floor_normal.x;
    out[7] = mr.floor_normal.y;
    out[8] = mr.floor_normal.z;
    out[9] = mr.collision_count;
    return 1;
}

/* out: [0] body, [1..3] normal, [4..6] point */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_move_collision(double h, int i,
                                                           double *out) {
    efx_move_collision mc;
    if (!efx_physics_move_collision(web_physics(), (efx_phys_character)h, i,
                                    &mc)) {
        return 0;
    }
    out[0] = (double)mc.body;
    out[1] = mc.normal.x;
    out[2] = mc.normal.y;
    out[3] = mc.normal.z;
    out[4] = mc.point.x;
    out[5] = mc.point.y;
    out[6] = mc.point.z;
    return 1;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_physics_character_destroy(double h) {
    efx_physics_destroy_character(web_physics(), (efx_phys_character)h);
}

/* raycast: out is a flat array of stride 10 per hit:
 * [point3, normal3, distance, body, character, sensor]; returns the count */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_raycast(
    double ox, double oy, double oz, double dx, double dy, double dz,
    double max_distance, double mask, int sensors, int all, double *out) {
    efx_ray_hit hits[256];
    int cap = all ? 256 : 1;
    int n = efx_physics_raycast(web_physics(), efx_v3((float)ox, (float)oy,
                                                      (float)oz),
                                efx_v3((float)dx, (float)dy, (float)dz),
                                (float)max_distance, (uint32_t)mask, sensors,
                                all, hits, cap);
    if (out) {
        for (int i = 0; i < n; i++) {
            double *o = &out[i * 10];
            o[0] = hits[i].point.x;
            o[1] = hits[i].point.y;
            o[2] = hits[i].point.z;
            o[3] = hits[i].normal.x;
            o[4] = hits[i].normal.y;
            o[5] = hits[i].normal.z;
            o[6] = hits[i].distance;
            o[7] = (double)hits[i].body;
            o[8] = (double)hits[i].character;
            o[9] = hits[i].sensor;
        }
    }
    return n;
}

/* overlap: out is a flat array of stride 3 per hit: [body, character, sensor] */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_overlap(
    int shape_type, double radius, double hx, double hy, double hz,
    double height, double mesh_handle, double px, double py, double pz,
    double mask, double *out) {
    efx_shape shape;
    memset(&shape, 0, sizeof(shape));
    web_shape_from(shape_type, radius, hx, hy, hz, height, &shape);
    efx_phys_mesh *temp = NULL;
    if (shape.type == EFX_PHYS_SHAPE_MESH) {
        temp = web_temp_mesh(mesh_handle);
        if (!temp) return 0;
        shape.mesh = temp;
    }
    int count = efx_physics_overlap(web_physics(), &shape,
                                    efx_v3((float)px, (float)py, (float)pz),
                                    (uint32_t)mask, 1, NULL, 0);
    if (out && count > 0) {
        efx_overlap_hit *hits = calloc((size_t)count, sizeof(*hits));
        if (hits) {
            int n = efx_physics_overlap(web_physics(), &shape,
                                        efx_v3((float)px, (float)py, (float)pz),
                                        (uint32_t)mask, 1, hits, count);
            for (int i = 0; i < n; i++) {
                out[i * 3] = (double)hits[i].body;
                out[i * 3 + 1] = (double)hits[i].character;
                out[i * 3 + 2] = hits[i].sensor;
            }
            free(hits);
        } else {
            count = 0;
        }
    }
    efx_phys_mesh_free(temp);
    return count;
}

/* shapeCast: out is [point3, normal3, fraction, body, character, sensor] */
EMSCRIPTEN_KEEPALIVE int efx_bridge_physics_shape_cast(
    int shape_type, double radius, double hx, double hy, double hz,
    double height, double mesh_handle, double fx, double fy, double fz,
    double mx, double my, double mz, double mask, int sensors, double *out) {
    efx_shape shape;
    memset(&shape, 0, sizeof(shape));
    web_shape_from(shape_type, radius, hx, hy, hz, height, &shape);
    efx_phys_mesh *temp = NULL;
    if (shape.type == EFX_PHYS_SHAPE_MESH) {
        temp = web_temp_mesh(mesh_handle);
        if (!temp) return 0;
        shape.mesh = temp;
    }
    efx_shape_hit hit;
    int rc = efx_physics_shape_cast(
        web_physics(), &shape, efx_v3((float)fx, (float)fy, (float)fz),
        efx_v3((float)mx, (float)my, (float)mz), (uint32_t)mask, sensors,
        &hit);
    efx_phys_mesh_free(temp);
    if (rc && out) {
        out[0] = hit.point.x;
        out[1] = hit.point.y;
        out[2] = hit.point.z;
        out[3] = hit.normal.x;
        out[4] = hit.normal.y;
        out[5] = hit.normal.z;
        out[6] = hit.fraction;
        out[7] = (double)hit.body;
        out[8] = (double)hit.character;
        out[9] = hit.sensor;
    }
    return rc;
}
