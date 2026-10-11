#ifndef EFX_NOISE_H
#define EFX_NOISE_H

/*
 * Stateless procedural noise over the vendored FastNoise Lite C99 port
 * (vendor/fastnoise-lite, ADR 0059). Pure functions of a seed,
 * coordinates, and a descriptor: no generator objects, no hidden state,
 * bit-identical output on every target (receipt: the noise golden scene,
 * ADR 0020).
 *
 * Coordinates are world units scaled by `frequency` (frequency 1 = unit
 * scale). fill2 samples the integer grid: value (col, row) lands at
 * out[row * w + col] and matches efx_noise2 at (col, row) exactly.
 *
 * All enumeration values deliberately match the vendored header's
 * fnl_noise_type / fnl_domain_warp_type order so the script bindings can
 * marshal validated integers straight through.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EFX_NOISE_OPENSIMPLEX2 = 0,
    EFX_NOISE_OPENSIMPLEX2S = 1,
    EFX_NOISE_CELLULAR = 2,
    EFX_NOISE_PERLIN = 3,
    EFX_NOISE_VALUE_CUBIC = 4,
    EFX_NOISE_VALUE = 5
} efx_noise_type;

typedef enum {
    EFX_NOISE_WARP_NONE = 0,
    EFX_NOISE_WARP_OPENSIMPLEX2 = 1,
    EFX_NOISE_WARP_GRADIENT = 2 /* FNL basic-grid warp */
} efx_noise_warp;

/* defensive clamp bound for `octaves`; the script layer rejects values
 * outside 1..EFX_NOISE_MAX_OCTAVES with canonical errors, this bound only
 * protects direct C callers */
#define EFX_NOISE_MAX_OCTAVES 16

typedef struct efx_noise_desc {
    uint32_t seed;
    efx_noise_type type;
    float frequency;
    int octaves; /* 1..EFX_NOISE_MAX_OCTAVES; >1 stacks fBm */
    float lacunarity;
    float gain;
    efx_noise_warp warp; /* domain warp applied before sampling */
} efx_noise_desc;

/* seed 0, opensimplex2, frequency 1, octaves 1, lacunarity 2, gain 0.5,
 * warp none — the documented script-facing defaults */
void efx_noise_desc_default(efx_noise_desc *d);

/* raw generator value, never remapped: ~[-1, 1] for the gradient/value
 * types; cellular is the nearest-feature distance in FNL's centered
 * convention (distance minus one cell unit), ~[-1, 0.5] in practice */
float efx_noise2(const efx_noise_desc *d, float x, float y);
float efx_noise3(const efx_noise_desc *d, float x, float y, float z);

/* write exactly w*h values row-major into out; one shared evaluator with
 * efx_noise2, so out[row * w + col] == efx_noise2(d, col, row) exactly */
void efx_noise_fill2(const efx_noise_desc *d, float *out, int w, int h);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
