#include "noise/efx_noise.h"

#include "FastNoiseLite.h"

#include <stddef.h>

/* FNL's fnl_state is a plain value struct of settings (queries take it
 * const), so building one per call is the whole "generator" — no hidden
 * state, and the wrapper stays re-entrant. */

void efx_noise_desc_default(efx_noise_desc *d) {
    if (!d) {
        return;
    }
    d->seed = 0;
    d->type = EFX_NOISE_OPENSIMPLEX2;
    d->frequency = 1.0f;
    d->octaves = 1;
    d->lacunarity = 2.0f;
    d->gain = 0.5f;
    d->warp = EFX_NOISE_WARP_NONE;
}

/* defensive clamps for direct C callers; the script layer validates with
 * canonical errors before marshaling (ADR 0049) */
static efx_noise_desc desc_sanitize(const efx_noise_desc *in) {
    efx_noise_desc d = *in;
    if (!(d.frequency > 0.0f)) {
        d.frequency = 1.0f;
    }
    if (d.octaves < 1) {
        d.octaves = 1;
    } else if (d.octaves > EFX_NOISE_MAX_OCTAVES) {
        d.octaves = EFX_NOISE_MAX_OCTAVES;
    }
    return d;
}

static fnl_state state_build(const efx_noise_desc *d) {
    fnl_state s = fnlCreateState();
    s.seed = (int)d->seed;
    s.frequency = d->frequency;
    s.noise_type = (fnl_noise_type)d->type;
    if (d->octaves > 1) {
        s.fractal_type = FNL_FRACTAL_FBM;
        s.octaves = d->octaves;
        s.lacunarity = d->lacunarity;
        s.gain = d->gain;
    }
    s.domain_warp_type = (d->warp == EFX_NOISE_WARP_GRADIENT)
                             ? FNL_DOMAIN_WARP_BASICGRID
                             : FNL_DOMAIN_WARP_OPENSIMPLEX2;
    /* domain_warp_amp keeps the FNL default (1.0); warp intensity is not
     * a script-facing option (design D6/D8) */
    return s;
}

float efx_noise2(const efx_noise_desc *in, float x, float y) {
    efx_noise_desc d = desc_sanitize(in);
    fnl_state s = state_build(&d);
    if (d.warp != EFX_NOISE_WARP_NONE) {
        fnlDomainWarp2D(&s, &x, &y);
    }
    return fnlGetNoise2D(&s, x, y);
}

float efx_noise3(const efx_noise_desc *in, float x, float y, float z) {
    efx_noise_desc d = desc_sanitize(in);
    fnl_state s = state_build(&d);
    if (d.warp != EFX_NOISE_WARP_NONE) {
        fnlDomainWarp3D(&s, &x, &y, &z);
    }
    return fnlGetNoise3D(&s, x, y, z);
}

void efx_noise_fill2(const efx_noise_desc *in, float *out, int w, int h) {
    if (!out || w < 1 || h < 1) {
        return;
    }
    efx_noise_desc d = desc_sanitize(in);
    fnl_state s = state_build(&d);
    for (int y = 0; y < h; y++) {
        float fy = (float)y;
        size_t row = (size_t)y * (size_t)w;
        if (d.warp != EFX_NOISE_WARP_NONE) {
            for (int x = 0; x < w; x++) {
                float fx = (float)x;
                float fyw = fy;
                fnlDomainWarp2D(&s, &fx, &fyw);
                out[row + (size_t)x] = fnlGetNoise2D(&s, fx, fyw);
            }
        } else {
            for (int x = 0; x < w; x++) {
                out[row + (size_t)x] = fnlGetNoise2D(&s, (float)x, fy);
            }
        }
    }
}
