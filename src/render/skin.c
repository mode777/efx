#include "render/skin.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------- matrix helpers */

void efx_skin_mat_mul(float out[16], const float a[16], const float b[16]) {
    float r[16];
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            float v = 0.0f;
            for (int k = 0; k < 4; k++) {
                v += a[k * 4 + row] * b[c * 4 + k];
            }
            r[c * 4 + row] = v;
        }
    }
    memcpy(out, r, sizeof(r));
}

/* general 4x4 inverse (column-major); 0 when singular (determinant ~ 0) */
int efx_skin_mat_inverse(float out[16], const float m[16]) {
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] -
             m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] +
             m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] -
             m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] +
              m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] +
             m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] -
             m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] +
             m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] -
              m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] -
             m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] +
             m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] -
              m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] +
              m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] +
             m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] -
             m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] +
              m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] -
              m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] +
                m[3] * inv[12];
    if (fabsf(det) < 1e-12f) {
        return 0;
    }
    float d = 1.0f / det;
    for (int i = 0; i < 16; i++) {
        out[i] = inv[i] * d;
    }
    return 1;
}

static void mat_identity(float m[16]) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/* local = T * R(q) * S, column-major */
static void trs_compose(float m[16], const efx_skin_trs *trs) {
    float x = trs->r[0], y = trs->r[1], z = trs->r[2], w = trs->r[3];
    float r00 = 1.0f - 2.0f * (y * y + z * z);
    float r10 = 2.0f * (x * y + w * z);
    float r20 = 2.0f * (x * z - w * y);
    float r01 = 2.0f * (x * y - w * z);
    float r11 = 1.0f - 2.0f * (x * x + z * z);
    float r21 = 2.0f * (y * z + w * x);
    float r02 = 2.0f * (x * z + w * y);
    float r12 = 2.0f * (y * z - w * x);
    float r22 = 1.0f - 2.0f * (x * x + y * y);
    float sx = trs->s[0], sy = trs->s[1], sz = trs->s[2];
    m[0] = r00 * sx;  m[1] = r10 * sx;  m[2] = r20 * sx;  m[3] = 0.0f;
    m[4] = r01 * sy;  m[5] = r11 * sy;  m[6] = r21 * sy;  m[7] = 0.0f;
    m[8] = r02 * sz;  m[9] = r12 * sz;  m[10] = r22 * sz; m[11] = 0.0f;
    m[12] = trs->t[0]; m[13] = trs->t[1]; m[14] = trs->t[2]; m[15] = 1.0f;
}

static void quat_from_mat(float q[4], const float m[16]) {
    float r00 = m[0], r10 = m[1], r20 = m[2];
    float r01 = m[4], r11 = m[5], r21 = m[6];
    float r02 = m[8], r12 = m[9], r22 = m[10];
    float trace = r00 + r11 + r22;
    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f) * 2.0f;
        q[3] = 0.25f * s;
        q[0] = (r21 - r12) / s;
        q[1] = (r02 - r20) / s;
        q[2] = (r10 - r01) / s;
    } else if (r00 > r11 && r00 > r22) {
        float s = sqrtf(1.0f + r00 - r11 - r22) * 2.0f;
        q[3] = (r21 - r12) / s;
        q[0] = 0.25f * s;
        q[1] = (r01 + r10) / s;
        q[2] = (r02 + r20) / s;
    } else if (r11 > r22) {
        float s = sqrtf(1.0f + r11 - r00 - r22) * 2.0f;
        q[3] = (r02 - r20) / s;
        q[0] = (r01 + r10) / s;
        q[1] = 0.25f * s;
        q[2] = (r12 + r21) / s;
    } else {
        float s = sqrtf(1.0f + r22 - r00 - r11) * 2.0f;
        q[3] = (r10 - r01) / s;
        q[0] = (r02 + r20) / s;
        q[1] = (r12 + r21) / s;
        q[2] = 0.25f * s;
    }
    float len = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (len > 0.0f) {
        q[0] /= len;
        q[1] /= len;
        q[2] /= len;
        q[3] /= len;
    } else {
        q[0] = q[1] = q[2] = 0.0f;
        q[3] = 1.0f;
    }
}

static void trs_decompose(efx_skin_trs *trs, const float m[16]) {
    trs->t[0] = m[12];
    trs->t[1] = m[13];
    trs->t[2] = m[14];
    float c0[3] = {m[0], m[1], m[2]};
    float c1[3] = {m[4], m[5], m[6]};
    float c2[3] = {m[8], m[9], m[10]};
    float sx = sqrtf(c0[0] * c0[0] + c0[1] * c0[1] + c0[2] * c0[2]);
    float sy = sqrtf(c1[0] * c1[0] + c1[1] * c1[1] + c1[2] * c1[2]);
    float sz = sqrtf(c2[0] * c2[0] + c2[1] * c2[1] + c2[2] * c2[2]);
    if (sx <= 0.0f) sx = 1.0f;
    if (sy <= 0.0f) sy = 1.0f;
    if (sz <= 0.0f) sz = 1.0f;
    /* negative determinant: mirror the third axis so R is a proper rotation */
    float det = c0[0] * (c1[1] * c2[2] - c1[2] * c2[1]) -
                c1[0] * (c0[1] * c2[2] - c0[2] * c2[1]) +
                c2[0] * (c0[1] * c1[2] - c0[2] * c1[1]);
    if (det < 0.0f) {
        sz = -sz;
    }
    trs->s[0] = sx;
    trs->s[1] = sy;
    trs->s[2] = sz;
    float rot[16];
    rot[0] = c0[0] / sx; rot[1] = c0[1] / sx; rot[2] = c0[2] / sx; rot[3] = 0;
    rot[4] = c1[0] / sy; rot[5] = c1[1] / sy; rot[6] = c1[2] / sy; rot[7] = 0;
    rot[8] = c2[0] / sz; rot[9] = c2[1] / sz; rot[10] = c2[2] / sz; rot[11] = 0;
    rot[12] = rot[13] = rot[14] = 0; rot[15] = 1;
    quat_from_mat(trs->r, rot);
}

/* --------------------------------------------------- bind-local derivation */

int efx_skin_bind_local(const efx_rig *rig, float *out) {
    if (!rig || !out || rig->joint_count <= 0 || !rig->inverse_bind ||
        !rig->joint_parents) {
        return -1;
    }
    int n = rig->joint_count;
    float *world = malloc((size_t)n * 16 * sizeof(float));
    if (!world) {
        return -1;
    }
    for (int j = 0; j < n; j++) {
        if (!efx_skin_mat_inverse(world + (size_t)j * 16,
                                  rig->inverse_bind + (size_t)j * 16)) {
            mat_identity(world + (size_t)j * 16);
        }
    }
    for (int j = 0; j < n; j++) {
        int p = rig->joint_parents[j];
        if (p < 0 || p >= n) {
            memcpy(out + (size_t)j * 16, world + (size_t)j * 16,
                   sizeof(float) * 16);
        } else {
            float invp[16];
            if (!efx_skin_mat_inverse(invp, world + (size_t)p * 16)) {
                mat_identity(invp);
            }
            efx_skin_mat_mul(out + (size_t)j * 16, invp,
                             world + (size_t)j * 16);
        }
    }
    free(world);
    return 0;
}

int efx_skin_bind_trs(const efx_rig *rig, efx_skin_trs *out) {
    if (!rig || !out || rig->joint_count <= 0) {
        return -1;
    }
    int n = rig->joint_count;
    float *local = malloc((size_t)n * 16 * sizeof(float));
    if (!local) {
        return -1;
    }
    if (efx_skin_bind_local(rig, local) != 0) {
        free(local);
        return -1;
    }
    for (int j = 0; j < n; j++) {
        trs_decompose(&out[j], local + (size_t)j * 16);
    }
    free(local);
    return 0;
}

/* ------------------------------------------------------------ sampling */

float efx_skin_clip_length(const efx_animation_clip *clip) {
    if (!clip) {
        return 0.0f;
    }
    float len = 0.0f;
    for (int i = 0; i < clip->channel_count; i++) {
        const efx_anim_channel *ch = &clip->channels[i];
        if (ch->times_len > 0 && ch->times &&
            ch->times[ch->times_len - 1] > len) {
            len = ch->times[ch->times_len - 1];
        }
    }
    return len;
}

static float wrap_time(float t, float len) {
    if (!(len > 0.0f) || !isfinite(t)) {
        return 0.0f;
    }
    float w = fmodf(t, len);
    if (w < 0.0f) {
        w += len;
    }
    return w;
}

static void lerp_n(float *out, const float *a, const float *b, float u, int n) {
    for (int i = 0; i < n; i++) {
        out[i] = a[i] + (b[i] - a[i]) * u;
    }
}

static void slerp4(float *out, const float *a, const float *b, float u) {
    float bx = b[0], by = b[1], bz = b[2], bw = b[3];
    float dot = a[0] * bx + a[1] * by + a[2] * bz + a[3] * bw;
    if (dot < 0.0f) {
        bx = -bx; by = -by; bz = -bz; bw = -bw;
        dot = -dot;
    }
    if (dot > 0.9995f) {
        float r[4] = {a[0] + (bx - a[0]) * u, a[1] + (by - a[1]) * u,
                      a[2] + (bz - a[2]) * u, a[3] + (bw - a[3]) * u};
        float len = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]);
        if (len > 0.0f) {
            for (int i = 0; i < 4; i++) out[i] = r[i] / len;
        } else {
            memcpy(out, a, sizeof(r));
        }
        return;
    }
    float theta0 = acosf(dot);
    float theta = theta0 * u;
    float s0 = sinf(theta0 - theta) / sinf(theta0);
    float s1 = sinf(theta) / sinf(theta0);
    out[0] = a[0] * s0 + bx * s1;
    out[1] = a[1] * s0 + by * s1;
    out[2] = a[2] * s0 + bz * s1;
    out[3] = a[3] * s0 + bw * s1;
}

/* sample one channel at wrapped time into `out` (components 3 or 4) */
static void sample_channel(const efx_anim_channel *ch, float t, float *out) {
    int n = ch->times_len;
    int c = ch->components;
    if (n <= 0 || !ch->times || !ch->values) {
        return;
    }
    if (n == 1) {
        memcpy(out, ch->values, (size_t)c * sizeof(float));
        return;
    }
    if (t <= ch->times[0]) {
        memcpy(out, ch->values, (size_t)c * sizeof(float));
        return;
    }
    if (t >= ch->times[n - 1]) {
        memcpy(out, ch->values + (size_t)(n - 1) * c, (size_t)c * sizeof(float));
        return;
    }
    int i = 0;
    for (i = 0; i < n - 1; i++) {
        if (t < ch->times[i + 1]) {
            break;
        }
    }
    const float *v0 = ch->values + (size_t)i * c;
    if (ch->interpolation == EFX_ANIM_INTERP_STEP) {
        memcpy(out, v0, (size_t)c * sizeof(float));
        return;
    }
    const float *v1 = ch->values + (size_t)(i + 1) * c;
    float t0 = ch->times[i];
    float t1 = ch->times[i + 1];
    float u = t1 > t0 ? (t - t0) / (t1 - t0) : 0.0f;
    if (c == 4) {
        slerp4(out, v0, v1, u);
    } else {
        lerp_n(out, v0, v1, u, c);
    }
}

/* sample a clip into `local` (joint_count entries), leaving bind TRS for
 * channels that are absent or target a non-joint node */
static void sample_clip(const efx_animation_clip *clip, float t,
                        const int *joint_of_node, efx_skin_trs *local) {
    if (!clip) {
        return;
    }
    for (int k = 0; k < clip->channel_count; k++) {
        const efx_anim_channel *ch = &clip->channels[k];
        if (ch->target_node < 0 || !joint_of_node) {
            continue;
        }
        int j = joint_of_node[ch->target_node];
        if (j < 0) {
            continue;
        }
        float v[4];
        sample_channel(ch, t, v);
        switch (ch->path) {
        case EFX_ANIM_PATH_TRANSLATION:
            local[j].t[0] = v[0];
            local[j].t[1] = v[1];
            local[j].t[2] = v[2];
            break;
        case EFX_ANIM_PATH_ROTATION:
            local[j].r[0] = v[0];
            local[j].r[1] = v[1];
            local[j].r[2] = v[2];
            local[j].r[3] = v[3];
            break;
        case EFX_ANIM_PATH_SCALE:
            local[j].s[0] = v[0];
            local[j].s[1] = v[1];
            local[j].s[2] = v[2];
            break;
        default:
            break;
        }
    }
}

int efx_skin_evaluate(const efx_rig *rig, const efx_pose_sample *samples,
                      int count, float *palette) {
    if (!rig || !palette || rig->joint_count <= 0 || count < 0) {
        return -1;
    }
    int n = rig->joint_count;
    if (rig->clip_count < 0) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (samples[i].clip < 0 || samples[i].clip >= rig->clip_count) {
            return -2;
        }
        if (count > 1 && samples[i].weight < 0.0f) {
            return -3;
        }
    }
    /* node -> joint map (bounds by the largest joint node index) */
    int max_node = -1;
    for (int j = 0; j < n; j++) {
        if (rig->joint_nodes[j] > max_node) {
            max_node = rig->joint_nodes[j];
        }
    }
    int node_count = max_node + 1;
    int *joint_of_node = NULL;
    if (node_count > 0) {
        joint_of_node = malloc((size_t)node_count * sizeof(int));
        if (!joint_of_node) {
            return -1;
        }
        for (int i = 0; i < node_count; i++) {
            joint_of_node[i] = -1;
        }
        for (int j = 0; j < n; j++) {
            int nd = rig->joint_nodes[j];
            if (nd >= 0 && nd < node_count) {
                joint_of_node[nd] = j;
            }
        }
    }

    efx_skin_trs *bind = malloc((size_t)n * sizeof(efx_skin_trs));
    efx_skin_trs *acc = malloc((size_t)n * sizeof(efx_skin_trs));
    efx_skin_trs *scratch = malloc((size_t)n * sizeof(efx_skin_trs));
    float *local_mat = malloc((size_t)n * 16 * sizeof(float));
    float *world = malloc((size_t)n * 16 * sizeof(float));
    if (!bind || !acc || !scratch || !local_mat || !world) {
        free(joint_of_node);
        free(bind);
        free(acc);
        free(scratch);
        free(local_mat);
        free(world);
        return -1;
    }
    if (efx_skin_bind_trs(rig, bind) != 0) {
        free(joint_of_node);
        free(bind);
        free(acc);
        free(scratch);
        free(local_mat);
        free(world);
        return -1;
    }

    if (count == 1) {
        memcpy(acc, bind, (size_t)n * sizeof(efx_skin_trs));
        float len = efx_skin_clip_length(&rig->clips[samples[0].clip]);
        sample_clip(&rig->clips[samples[0].clip],
                    wrap_time(samples[0].time, len), joint_of_node, acc);
    } else if (count > 1) {
        float wsum = 0.0f;
        for (int i = 0; i < count; i++) {
            wsum += samples[i].weight;
        }
        if (wsum > 0.0f) {
            /* start from the first sample, then blend the rest in */
            memcpy(acc, bind, (size_t)n * sizeof(efx_skin_trs));
            float len0 = efx_skin_clip_length(&rig->clips[samples[0].clip]);
            sample_clip(&rig->clips[samples[0].clip],
                        wrap_time(samples[0].time, len0), joint_of_node, acc);
            float wacc = samples[0].weight;
            for (int i = 1; i < count; i++) {
                memcpy(scratch, bind, (size_t)n * sizeof(efx_skin_trs));
                float len = efx_skin_clip_length(&rig->clips[samples[i].clip]);
                sample_clip(&rig->clips[samples[i].clip],
                            wrap_time(samples[i].time, len), joint_of_node,
                            scratch);
                float w = samples[i].weight;
                float total = wacc + w;
                float u = total > 0.0f ? w / total : 0.0f;
                for (int j = 0; j < n; j++) {
                    for (int c = 0; c < 3; c++) {
                        acc[j].t[c] += (scratch[j].t[c] - acc[j].t[c]) * u;
                        acc[j].s[c] += (scratch[j].s[c] - acc[j].s[c]) * u;
                    }
                    slerp4(acc[j].r, acc[j].r, scratch[j].r, u);
                }
                wacc = total;
            }
        } else {
            /* all-zero weights: bind pose */
            memcpy(acc, bind, (size_t)n * sizeof(efx_skin_trs));
        }
    } else {
        memcpy(acc, bind, (size_t)n * sizeof(efx_skin_trs));
    }

    /* compose locals, then FK in hierarchy order and build the palette */
    for (int j = 0; j < n; j++) {
        trs_compose(local_mat + (size_t)j * 16, &acc[j]);
    }
    for (int j = 0; j < n; j++) {
        int p = rig->joint_parents[j];
        if (p >= 0 && p < n) {
            efx_skin_mat_mul(world + (size_t)j * 16, world + (size_t)p * 16,
                             local_mat + (size_t)j * 16);
        } else {
            memcpy(world + (size_t)j * 16, local_mat + (size_t)j * 16,
                   sizeof(float) * 16);
        }
        efx_skin_mat_mul(palette + (size_t)j * 16, world + (size_t)j * 16,
                         rig->inverse_bind + (size_t)j * 16);
    }

    free(joint_of_node);
    free(bind);
    free(acc);
    free(scratch);
    free(local_mat);
    free(world);
    return 0;
}

/* ------------------------------------------------------ linear-blend skin */

void efx_skin_surface(const float *bind, int vertex_count,
                      const uint32_t *joints, const float *weights,
                      const float *palette, int joint_count, float *out) {
    if (!bind || !out || vertex_count <= 0) {
        return;
    }
    memcpy(out, bind, (size_t)vertex_count * 12 * sizeof(float));
    if (!joints || !weights || !palette || joint_count <= 0) {
        return;
    }
    for (int v = 0; v < vertex_count; v++) {
        const uint32_t *j = joints + (size_t)v * 4;
        const float *w = weights + (size_t)v * 4;
        float wsum = w[0] + w[1] + w[2] + w[3];
        if (!(wsum > 0.0f)) {
            continue; /* zero-sum vertex stays at bind */
        }
        float p[3] = {0.0f, 0.0f, 0.0f};
        float nrm[3] = {0.0f, 0.0f, 0.0f};
        const float *bp = bind + (size_t)v * 12;
        for (int i = 0; i < 4; i++) {
            if (w[i] == 0.0f) {
                continue;
            }
            if ((int)j[i] >= joint_count) {
                continue;
            }
            const float *M = palette + (size_t)j[i] * 16;
            float px = bp[0], py = bp[1], pz = bp[2];
            float nx = bp[3], ny = bp[4], nz = bp[5];
            float wi = w[i] / wsum;
            p[0] += wi * (M[0] * px + M[4] * py + M[8] * pz + M[12]);
            p[1] += wi * (M[1] * px + M[5] * py + M[9] * pz + M[13]);
            p[2] += wi * (M[2] * px + M[6] * py + M[10] * pz + M[14]);
            nrm[0] += wi * (M[0] * nx + M[4] * ny + M[8] * nz);
            nrm[1] += wi * (M[1] * nx + M[5] * ny + M[9] * nz);
            nrm[2] += wi * (M[2] * nx + M[6] * ny + M[10] * nz);
        }
        float len = sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
        float *op = out + (size_t)v * 12;
        op[0] = p[0];
        op[1] = p[1];
        op[2] = p[2];
        if (len > 0.0f) {
            op[3] = nrm[0] / len;
            op[4] = nrm[1] / len;
            op[5] = nrm[2] / len;
        } else {
            op[3] = 0.0f;
            op[4] = 0.0f;
            op[5] = 0.0f;
        }
    }
}
