#ifndef EFX_SKIN_H
#define EFX_SKIN_H

/*
 * F7 CPU skinning: bind-local derivation from the F6c rig payload, clip
 * sampling, joint-space FK + skin-matrix palette, and linear-blend skinning.
 *
 * Pure C, no sokol, no quickjs (ADR 0003 module walls); matrices are
 * column-major float[16] and rotations are quaternions xyzw. The functions
 * here are deterministic and shared by the renderer and the CPU-reference
 * unit tests (design D7).
 */

#include "render/render.h"

/* per-joint local TRS: bind pose or a sampled animation override */
typedef struct efx_skin_trs {
    float t[3];
    float r[4]; /* quaternion xyzw */
    float s[3];
} efx_skin_trs;

/* column-major 4x4 helpers (pure C) */
void efx_skin_mat_mul(float out[16], const float a[16], const float b[16]);
int efx_skin_mat_inverse(float out[16], const float m[16]); /* 0 = singular */

/* derive per-joint bind-local matrices (joint_count*16, column-major) from
 * inverse_bind + joint_parents: world_bind = inverse(inverse_bind) and
 * local_bind = inverse(world_bind[parent]) * world_bind (root: world_bind);
 * a singular matrix falls back to identity for that joint. 0 ok, -1 bad args */
int efx_skin_bind_local(const efx_rig *rig, float *out);

/* per-joint bind-local TRS (joint_count entries); same fallbacks as above */
int efx_skin_bind_trs(const efx_rig *rig, efx_skin_trs *out);

/* clip length in seconds (max keyframe time over its channels); 0 when empty */
float efx_skin_clip_length(const efx_animation_clip *clip);

/* Evaluate a weighted pose into the skin-matrix palette (joint_count*16):
 * local = bind local overridden by each sampled channel (joint-targeted
 * channels only; non-joint ancestors are ignored), then
 * world = world[parent] * local and palette[j] = world[j] * inverse_bind[j].
 * samples[i].clip indexes rig->clips, time wraps modulo the clip length, and
 * weights normalize engine-side (a single sample ignores its weight). Returns
 * 0 ok, -1 bad rig/palette/NOMEM, -2 bad clip index, -3 negative weight. */
int efx_skin_evaluate(const efx_rig *rig, const efx_pose_sample *samples,
                      int count, float *palette);

/* Linear-blend skin one surface: `bind` is the interleaved bind vertices
 * (12 floats/vertex), `out` receives a copy with positions/normals posed.
 * Per-vertex influence weights are normalized; a zero-sum vertex keeps its
 * bind position/normal. */
void efx_skin_surface(const float *bind, int vertex_count,
                      const uint32_t *joints, const float *weights,
                      const float *palette, int joint_count, float *out);

#endif
