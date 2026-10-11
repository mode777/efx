#include "bridge_internal.h"

#include "noise/efx_noise.h"

/* ================================================= efx.noise bridge
 * (ADR 0059) Marshal-only wire exports: the shared prelude validates the
 * option bag and passes fully-validated scalars — same wire layouts as
 * the desktop binding. */

EMSCRIPTEN_KEEPALIVE double efx_bridge_noise2(int32_t seed, int32_t type,
                                              double frequency,
                                              int32_t octaves,
                                              double lacunarity, double gain,
                                              int32_t warp, double x,
                                              double y) {
    efx_noise_desc d;
    d.seed = (uint32_t)seed;
    d.type = (efx_noise_type)type;
    d.frequency = (float)frequency;
    d.octaves = octaves;
    d.lacunarity = (float)lacunarity;
    d.gain = (float)gain;
    d.warp = (efx_noise_warp)warp;
    return efx_noise2(&d, (float)x, (float)y);
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_noise3(int32_t seed, int32_t type,
                                              double frequency,
                                              int32_t octaves,
                                              double lacunarity, double gain,
                                              int32_t warp, double x,
                                              double y, double z) {
    efx_noise_desc d;
    d.seed = (uint32_t)seed;
    d.type = (efx_noise_type)type;
    d.frequency = (float)frequency;
    d.octaves = octaves;
    d.lacunarity = (float)lacunarity;
    d.gain = (float)gain;
    d.warp = (efx_noise_warp)warp;
    return efx_noise3(&d, (float)x, (float)y, (float)z);
}

/* writes w*h floats row-major into the caller-allocated wasm-heap buffer */
EMSCRIPTEN_KEEPALIVE void efx_bridge_noise_fill2(int32_t seed, int32_t type,
                                                 double frequency,
                                                 int32_t octaves,
                                                 double lacunarity,
                                                 double gain, int32_t warp,
                                                 int32_t w, int32_t h,
                                                 float *out) {
    if (!out || w < 1 || h < 1) {
        return;
    }
    efx_noise_desc d;
    d.seed = (uint32_t)seed;
    d.type = (efx_noise_type)type;
    d.frequency = (float)frequency;
    d.octaves = octaves;
    d.lacunarity = (float)lacunarity;
    d.gain = (float)gain;
    d.warp = (efx_noise_warp)warp;
    efx_noise_fill2(&d, out, w, h);
}
