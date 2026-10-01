#include "bridge_internal.h"

/* ------------------------------------------- F11 (billboards + particles) */

/* particle wire layout (floats); kept in sync with src/web/entry.js:
 *   0 max, 1 space, 2 facing, 3 blend, 4 lifeMin, 5 lifeMax, 6 emissionRate,
 *   7 emitterLifetime, 8 speedScale, 9 spread, 10 sizeCount, 11 sizeVariation,
 *   12 colorCount, 13 relativeRotation, 14 shape, 15 quadCount,
 *   16 rotMin, 17 rotMax, 18 spinStart, 19 spinEnd, 20 spinVariation,
 *   21..23 position, 24..26 direction, 27 speedMin, 28 speedMax,
 *   29..31 gravity, 32..34 linAccMin, 35..37 linAccMax,
 *   38 radialMin, 39 radialMax, 40 tangMin, 41 tangMax, 42 dampMin, 43 dampMax,
 *   44..51 sizes[8], 52..83 colors[8][4], 84..86 shapeSize, 87..342 quads[64][4],
 *   343..345 normal, 346 insertMode */
#define EFX_PART_WIRE_LEN 352

static void web_particles_read(const float *w, double texture,
                               efx_particle_config *c) {
    memset(c, 0, sizeof(*c));
    c->texture = (uint64_t)texture;
    c->max = (int)w[0];
    c->space = (int)w[1];
    c->facing = (int)w[2];
    c->blend = (int)w[3];
    c->life_min = w[4];
    c->life_max = w[5];
    c->emission_rate = w[6];
    c->emitter_lifetime = w[7];
    c->speed_scale = w[8];
    c->spread = w[9];
    c->size_count = (int)w[10];
    c->size_variation = w[11];
    c->color_count = (int)w[12];
    c->relative_rotation = (int)w[13];
    c->shape = (int)w[14];
    c->quad_count = (int)w[15];
    c->rotation_min = w[16];
    c->rotation_max = w[17];
    c->spin_start = w[18];
    c->spin_end = w[19];
    c->spin_variation = w[20];
    for (int i = 0; i < 3; i++) c->position[i] = w[21 + i];
    for (int i = 0; i < 3; i++) c->direction[i] = w[24 + i];
    c->speed_min = w[27];
    c->speed_max = w[28];
    for (int i = 0; i < 3; i++) c->gravity[i] = w[29 + i];
    for (int i = 0; i < 3; i++) c->lin_acc_min[i] = w[32 + i];
    for (int i = 0; i < 3; i++) c->lin_acc_max[i] = w[35 + i];
    c->radial_acc_min = w[38];
    c->radial_acc_max = w[39];
    c->tangential_acc_min = w[40];
    c->tangential_acc_max = w[41];
    c->damping_min = w[42];
    c->damping_max = w[43];
    for (int i = 0; i < 8; i++) c->sizes[i] = w[44 + i];
    for (int i = 0; i < 8; i++) {
        for (int k = 0; k < 4; k++) c->colors[i][k] = w[52 + i * 4 + k];
    }
    for (int i = 0; i < 3; i++) c->shape_size[i] = w[84 + i];
    for (int i = 0; i < 64; i++) {
        for (int k = 0; k < 4; k++) c->quads[i][k] = w[87 + i * 4 + k];
    }
    for (int i = 0; i < 3; i++) c->normal[i] = w[343 + i];
    c->insert_mode = (int)w[346];
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_particles_create(const float *wire,
                                                        double texture) {
    if (!wire) {
        return 0;
    }
    efx_particle_config c;
    web_particles_read(wire, texture, &c);
    return (double)efx_render_particles_create(&c, NULL);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_particles_set(double handle,
                                                  const float *wire,
                                                  double texture) {
    if (!wire) {
        return EFX_RENDER_ERR_SIZE;
    }
    efx_particle_config c;
    web_particles_read(wire, texture, &c);
    return efx_render_particles_set((uint64_t)handle, &c);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_destroy(double handle) {
    efx_render_particles_destroy((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_particles_count(double handle) {
    return efx_render_particles_count((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_particles_emit(double handle, int n) {
    return efx_render_particles_emit((uint64_t)handle, n);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_start(double handle) {
    efx_render_particles_start((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_stop(double handle) {
    efx_render_particles_stop((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_pause(double handle) {
    efx_render_particles_pause((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_reset(double handle) {
    efx_render_particles_reset((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_particles_speed_scale(double handle) {
    return efx_render_particles_speed_scale((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_particles_set_speed_scale(double handle,
                                                               float s) {
    efx_render_particles_set_speed_scale((uint64_t)handle, s);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_particles_draw(double handle) {
    return efx_render_particles_draw((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_draw_billboard(double texture,
                                                   const float *pos, float w,
                                                   float h, float cr, float cg,
                                                   float cb, float ca,
                                                   float rotation, int facing,
                                                   const float *normal,
                                                   int depth_test, float sx,
                                                   float sy, float sw, float sh,
                                                   int has_src) {
    if (!pos || !normal) {
        return EFX_RENDER_ERR_HANDLE;
    }
    float color[4] = {cr, cg, cb, ca};
    float src[4] = {sx, sy, sw, sh};
    return efx_render_billboard((uint64_t)texture, pos, w, h, color, rotation,
                                facing, normal, depth_test, src, has_src);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_draw_sprite(double texture, float x,
                                                float y, float w, float h,
                                                float cr, float cg, float cb,
                                                float ca, float rotation,
                                                float scale, float sx, float sy,
                                                float sw, float sh, int has_src,
                                                float ox, float oy) {
    float color[4] = {cr, cg, cb, ca};
    float src[4] = {sx, sy, sw, sh};
    return efx_render_quad(x, y, w, h, (uint64_t)texture, color, rotation, scale,
                           src, has_src, ox, oy);
}

