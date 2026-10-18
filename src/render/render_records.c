#include "render_internal.h"

render_state R;

void efx_render_install_sink(const efx_render_sink *sink) {
    R.sink = sink;
}

void efx_render_set_viewport(int w, int h) {
    R.viewport_w = w;
    R.viewport_h = h;
}

void efx_render_viewport(int *out_w, int *out_h) {
    if (out_w) *out_w = R.viewport_w;
    if (out_h) *out_h = R.viewport_h;
}

/* documented defaults apply once, lazily — the script configures state
   at eval time, before the GPU sink exists, and installing the sink must
   not wipe that configuration */
static int state_ready;

static const efx_camera2d DEFAULT_CAMERA2D = {.zoom = 1.0f};
static const efx_camera3d DEFAULT_CAMERA3D = {
    .pos = {0.0f, 0.0f, 1.0f}, .fov = 60.0f, .near_z = 0.1f, .far_z = 100.0f};
static const efx_material DEFAULT_MATERIAL = {
    .ambient = {0.0f, 0.0f, 0.0f, 1.0f},
    .diffuse = {1.0f, 1.0f, 1.0f, 1.0f},
    .specular = {0.0f, 0.0f, 0.0f, 1.0f},
    .emissive = {0.0f, 0.0f, 0.0f, 1.0f},
    .shininess = 32.0f,
    .blend = EFX_BLEND_INHERIT,
    .unlit = 0,
    .alpha_mode = EFX_ALPHA_OPAQUE};

static void apply_default_state(void) {
    R.camera = DEFAULT_CAMERA2D;
    R.camera3d = DEFAULT_CAMERA3D;
    R.camera3d_set = 0;
    memset(&R.lights, 0, sizeof(R.lights)); /* every light disabled */
    memset(&R.fog, 0, sizeof(R.fog));       /* fog disabled */
    R.clear_color[0] = 0.0f;
    R.clear_color[1] = 0.0f;
    R.clear_color[2] = 0.0f;
    R.clear_color[3] = 1.0f;
    R.blend = EFX_BLEND_ALPHA;
}

void ensure_state(void) {
    if (state_ready) {
        return;
    }
    apply_default_state();
    state_ready = 1;
}

void efx_material_default(efx_material *m) {
    if (m) {
        *m = DEFAULT_MATERIAL;
    }
}

void efx_render_reset_state(void) {
    apply_default_state();
    post_reset();
}

void efx_render_set_camera(const efx_camera2d *cam) {
    ensure_state();
    if (cam) {
        R.camera = *cam;
    }
}

void efx_render_set_camera3d(const efx_camera3d *cam) {
    ensure_state();
    if (cam) {
        R.camera3d = *cam;
        R.camera3d_set = 1;
    }
}

void efx_render_camera3d(float out_pos[3], float out_target[3], float *out_fov,
                         float *out_near, float *out_far) {
    ensure_state();
    if (out_pos) {
        out_pos[0] = R.camera3d.pos[0];
        out_pos[1] = R.camera3d.pos[1];
        out_pos[2] = R.camera3d.pos[2];
    }
    if (out_target) {
        out_target[0] = R.camera3d.target[0];
        out_target[1] = R.camera3d.target[1];
        out_target[2] = R.camera3d.target[2];
    }
    if (out_fov) *out_fov = R.camera3d.fov;
    if (out_near) *out_near = R.camera3d.near_z;
    if (out_far) *out_far = R.camera3d.far_z;
}

/* current 2D frame extent (the most recent setCamera2D frame; 0 = default) —
 * the frame driver publishes it to the input module each frame */
void efx_render_frame_extent(float *out_w, float *out_h) {
    ensure_state();
    if (out_w) {
        *out_w = R.camera.frame_w;
    }
    if (out_h) {
        *out_h = R.camera.frame_h;
    }
}

/* the active rendering surface extent (viewport) */
void efx_render_viewport_size(int *out_w, int *out_h) {
    ensure_state();
    if (out_w) {
        *out_w = R.viewport_w;
    }
    if (out_h) {
        *out_h = R.viewport_h;
    }
}

/* whether a 3D camera was ever set (screenRay's state guard) */
int efx_render_camera3d_active(void) {
    ensure_state();
    return R.camera3d_set;
}

/* ---- mesh frustum culling (add-mesh-frustum-culling, ADR 0065) ----
 * Fail-open by contract: the test only ever says "cull" for a box it can
 * prove fully outside one frustum plane; any degenerate or non-finite state
 * keeps the draw. */

#include <float.h>

/* world-space AABB of the local bounds box transformed by the model matrix
 * (8 corners, w-divide). Returns 0 on any non-finite or near-zero w
 * (projective/degenerate matrix) — the caller keeps the draw. */
static int mesh_world_box(const float m[16], const float bmin[3],
                          const float bmax[3], float out_min[3],
                          float out_max[3]) {
    float wmn[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float wmx[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (int i = 0; i < 8; i++) {
        float c[3] = {(i & 1) ? bmax[0] : bmin[0],
                      (i & 2) ? bmax[1] : bmin[1],
                      (i & 4) ? bmax[2] : bmin[2]};
        float w = m[3] * c[0] + m[7] * c[1] + m[11] * c[2] + m[15];
        if (w > -1e-6f && w < 1e-6f) {
            return 0;
        }
        float p[3] = {(m[0] * c[0] + m[4] * c[1] + m[8] * c[2] + m[12]) / w,
                      (m[1] * c[0] + m[5] * c[1] + m[9] * c[2] + m[13]) / w,
                      (m[2] * c[0] + m[6] * c[1] + m[10] * c[2] + m[14]) / w};
        for (int k = 0; k < 3; k++) {
            if (!isfinite(p[k])) {
                return 0;
            }
            if (p[k] < wmn[k]) wmn[k] = p[k];
            if (p[k] > wmx[k]) wmx[k] = p[k];
        }
    }
    memcpy(out_min, wmn, sizeof(wmn));
    memcpy(out_max, wmx, sizeof(wmx));
    return 1;
}

/* 1 = the world box is fully outside at least one frustum plane (cull);
 * 0 = keep (inside, straddling, or any state the test cannot prove).
 * Planes use the same basis construction as efx_render_screen_ray
 * (forward = target - pos, right = fwd × worldUp, up = right × fwd);
 * aspect is the active rendering surface's (efx_render_surface_size). */
static int mesh_frustum_cull(const efx_camera3d *cam, float aspect,
                             const float bmin[3], const float bmax[3]) {
    float f[3] = {cam->target[0] - cam->pos[0], cam->target[1] - cam->pos[1],
                  cam->target[2] - cam->pos[2]};
    float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (!(fl > 0.0f) || !isfinite(fl)) {
        return 0; /* degenerate camera: fail open */
    }
    f[0] /= fl; f[1] /= fl; f[2] /= fl;
    float r[3] = {-f[2], 0.0f, f[0]};
    float rl = sqrtf(r[0] * r[0] + r[2] * r[2]);
    if (!(rl > 0.0f) || !isfinite(rl)) {
        return 0; /* forward parallel to world up: fail open */
    }
    r[0] /= rl; r[2] /= rl;
    float u[3] = {r[1] * f[2] - r[2] * f[1],
                  r[2] * f[0] - r[0] * f[2],
                  r[0] * f[1] - r[1] * f[0]};
    if (!(cam->fov > 0.0f) || !isfinite(cam->fov) || !(aspect > 0.0f) ||
        !isfinite(aspect) || !(cam->near_z >= 0.0f) ||
        !isfinite(cam->near_z) || !(cam->far_z > cam->near_z) ||
        !isfinite(cam->far_z)) {
        return 0;
    }
    float t = tanf(cam->fov * DEG2RAD * 0.5f);
    float ts = t * aspect;
    if (!isfinite(t) || !isfinite(ts) || t <= 0.0f) {
        return 0;
    }
    /* planes: (n, d), inside = dot(n, p) + d >= 0 */
    float planes[6][4];
    const float dot_eye_f = f[0] * cam->pos[0] + f[1] * cam->pos[1] +
                            f[2] * cam->pos[2];
    /* near / far */
    planes[0][0] = f[0];  planes[0][1] = f[1];  planes[0][2] = f[2];
    planes[0][3] = -(dot_eye_f + cam->near_z);
    planes[1][0] = -f[0]; planes[1][1] = -f[1]; planes[1][2] = -f[2];
    planes[1][3] = dot_eye_f + cam->far_z;
    /* left (n = r + f·ts) / right (n = f·ts − r), normalized */
    float nrm[3] = {r[0] + f[0] * ts, r[1] + f[1] * ts, r[2] + f[2] * ts};
    float nl = sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (!(nl > 0.0f) || !isfinite(nl)) return 0;
    planes[2][0] = nrm[0] / nl; planes[2][1] = nrm[1] / nl;
    planes[2][2] = nrm[2] / nl;
    planes[2][3] = -(planes[2][0] * cam->pos[0] + planes[2][1] * cam->pos[1] +
                     planes[2][2] * cam->pos[2]);
    nrm[0] = f[0] * ts - r[0]; nrm[1] = f[1] * ts - r[1]; nrm[2] = f[2] * ts - r[2];
    nl = sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (!(nl > 0.0f) || !isfinite(nl)) return 0;
    planes[3][0] = nrm[0] / nl; planes[3][1] = nrm[1] / nl;
    planes[3][2] = nrm[2] / nl;
    planes[3][3] = -(planes[3][0] * cam->pos[0] + planes[3][1] * cam->pos[1] +
                     planes[3][2] * cam->pos[2]);
    /* top (n = f·t − u) / bottom (n = u + f·t), normalized */
    nrm[0] = f[0] * t - u[0]; nrm[1] = f[1] * t - u[1]; nrm[2] = f[2] * t - u[2];
    nl = sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (!(nl > 0.0f) || !isfinite(nl)) return 0;
    planes[4][0] = nrm[0] / nl; planes[4][1] = nrm[1] / nl;
    planes[4][2] = nrm[2] / nl;
    planes[4][3] = -(planes[4][0] * cam->pos[0] + planes[4][1] * cam->pos[1] +
                     planes[4][2] * cam->pos[2]);
    nrm[0] = u[0] + f[0] * t; nrm[1] = u[1] + f[1] * t; nrm[2] = u[2] + f[2] * t;
    nl = sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (!(nl > 0.0f) || !isfinite(nl)) return 0;
    planes[5][0] = nrm[0] / nl; planes[5][1] = nrm[1] / nl;
    planes[5][2] = nrm[2] / nl;
    planes[5][3] = -(planes[5][0] * cam->pos[0] + planes[5][1] * cam->pos[1] +
                     planes[5][2] * cam->pos[2]);
    /* p-vertex test per plane with early-out */
    for (int i = 0; i < 6; i++) {
        const float *n = planes[i];
        float px = n[0] >= 0.0f ? bmax[0] : bmin[0];
        float py = n[1] >= 0.0f ? bmax[1] : bmin[1];
        float pz = n[2] >= 0.0f ? bmax[2] : bmin[2];
        float dist = n[0] * px + n[1] * py + n[2] * pz + planes[i][3];
        if (!isfinite(dist)) {
            return 0;
        }
        if (dist < 0.0f) {
            return 1; /* entirely beyond this plane */
        }
    }
    return 0;
}

/* world-space ray through a screen point under the current 3D camera
 * (screenRay, script-api-ergonomics). `sx, sy` are frame pixels within
 * `extent_w × extent_h` (top-left origin, y down). rc 0 ok; -1 no 3D camera;
 * -2 degenerate camera or extent. */
int efx_render_screen_ray(double sx, double sy, double extent_w,
                          double extent_h, float out_origin[3],
                          float out_dir[3]) {
    if (!R.camera3d_set) {
        return -1;
    }
    if (!(extent_w > 0) || !(extent_h > 0)) {
        return -2;
    }
    /* camera basis: forward/target, world-up right/up (matches setCamera3D) */
    const double px = R.camera3d.pos[0], py = R.camera3d.pos[1],
                 pz = R.camera3d.pos[2];
    double f[3] = {R.camera3d.target[0] - px, R.camera3d.target[1] - py,
                   R.camera3d.target[2] - pz};
    double fl = sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (!(fl > 0)) {
        return -2;
    }
    f[0] /= fl; f[1] /= fl; f[2] /= fl;
    /* right = normalize(fwd × worldUp) — the screen-right basis vector */
    double r[3] = {-f[2], 0, f[0]};
    double rl = sqrt(r[0] * r[0] + r[2] * r[2]);
    if (!(rl > 0)) {
        return -2; /* forward is parallel to world up */
    }
    r[0] /= rl; r[2] /= rl;
    /* up = right × fwd */
    double u[3] = {r[1] * f[2] - r[2] * f[1],
                   r[2] * f[0] - r[0] * f[2],
                   r[0] * f[1] - r[1] * f[0]};
    /* NDC within the frame extent (frame pixels, y down) */
    const double nx = (sx / extent_w) * 2.0 - 1.0;
    const double ny = 1.0 - (sy / extent_h) * 2.0;
    const double aspect = extent_w / extent_h;
    const double t = tan(R.camera3d.fov * (3.141592653589793 / 180.0) / 2.0);
    double d[3] = {f[0] + r[0] * nx * t * aspect + u[0] * ny * t,
                   f[1] + r[1] * nx * t * aspect + u[1] * ny * t,
                   f[2] + r[2] * nx * t * aspect + u[2] * ny * t};
    double dl = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(dl > 0)) {
        return -2;
    }
    out_origin[0] = (float)px; out_origin[1] = (float)py;
    out_origin[2] = (float)pz;
    out_dir[0] = (float)(d[0] / dl); out_dir[1] = (float)(d[1] / dl);
    out_dir[2] = (float)(d[2] / dl);
    return 0;
}

void efx_render_set_clear_color(const float rgba[4]) {
    ensure_state();
    if (rgba) {
        for (int i = 0; i < 4; i++) {
            R.clear_color[i] = rgba[i];
        }
    }
}

void efx_render_set_point_light(int slot, const efx_point_light *light) {
    ensure_state();
    if (slot < 0 || slot >= EFX_MAX_POINT_LIGHTS) {
        return;
    }
    if (!light || !light->enabled) {
        R.lights.points[slot].enabled = 0;
        return;
    }
    R.lights.points[slot] = *light;
    R.lights.points[slot].enabled = 1;
}

void efx_render_set_directional_light(const efx_dir_light *light) {
    ensure_state();
    if (!light || !light->enabled) {
        R.lights.directional.enabled = 0;
        return;
    }
    R.lights.directional = *light;
    R.lights.directional.enabled = 1;
}

void efx_render_lights(efx_light_set *out) {
    ensure_state();
    if (out) {
        *out = R.lights;
    }
}

void efx_render_set_fog(const efx_fog *fog) {
    ensure_state();
    if (!fog || !fog->enabled) {
        R.fog.enabled = 0;
        return;
    }
    R.fog = *fog;
    R.fog.enabled = 1;
}

void efx_render_fog(efx_fog *out) {
    ensure_state();
    if (out) {
        *out = R.fog;
    }
}

void efx_render_clear_color(float out_rgba[4]) {
    ensure_state();
    for (int i = 0; i < 4; i++) {
        out_rgba[i] = R.clear_color[i];
    }
}

int efx_render_set_blend(int mode) {
    ensure_state();
    if (mode < EFX_BLEND_ALPHA || mode > EFX_BLEND_SUBTRACTIVE) {
        return -1;
    }
    R.blend = mode;
    return 0;
}

efx_affine efx_affine_mul(efx_affine f, efx_affine g) {
    efx_affine r;
    r.a = f.a * g.a + f.c * g.b;
    r.b = f.b * g.a + f.d * g.b;
    r.c = f.a * g.c + f.c * g.d;
    r.d = f.b * g.c + f.d * g.d;
    r.tx = f.a * g.tx + f.c * g.ty + f.tx;
    r.ty = f.b * g.tx + f.d * g.ty + f.ty;
    return r;
}

/* world -> frame; zoom and rotation pivot on the frame center (x,y is the
 * world point shown at the frame center). Standard rotation matrices read
 * as clockwise in the y-down frame. */
efx_affine efx_camera_matrix(const efx_camera2d *cam, float fw, float fh) {
    float t = cam->rotation * DEG2RAD;
    float cs = cosf(t) * cam->zoom;
    float sn = sinf(t) * cam->zoom;
    efx_affine v;
    v.a = cs;
    v.b = sn;
    v.c = -sn;
    v.d = cs;
    float cx = fw * 0.5f;
    float cy = fh * 0.5f;
    v.tx = cx - (v.a * cam->x + v.c * cam->y);
    v.ty = cy - (v.b * cam->x + v.d * cam->y);
    return v;
}

/* local -> world; corner-anchored placement, rotation/scale pivot on the
 * caller-provided point (quad-local, relative to the quad top-left; the
 * legacy center pivot is origin = size/2) */
efx_affine efx_quad_matrix(float x, float y,
                           float origin_x, float origin_y,
                           float rotation_deg, float scale) {
    float t = rotation_deg * DEG2RAD;
    float cs = cosf(t) * scale;
    float sn = sinf(t) * scale;
    efx_affine m;
    m.a = cs;
    m.b = sn;
    m.c = -sn;
    m.d = cs;
    float px = x + origin_x;
    float py = y + origin_y;
    m.tx = px - (m.a * origin_x + m.c * origin_y);
    m.ty = py - (m.b * origin_x + m.d * origin_y);
    return m;
}

/* ------------------------------------------------------------- records */

void color_or_white(float out[4], const float color[4]) {
    for (int i = 0; i < 4; i++) {
        out[i] = color ? color[i] : 1.0f;
    }
}

/* sort key = record index: playback order equals record order */
int record_push(efx_record *rec) {
    rec->target = R.active_target;
    rec->sort_key = (uint32_t)R.record_count;
    if ((size_t)(R.record_count + 1) * sizeof(efx_record) >
        EFX_RENDER_RECORD_BUDGET_BYTES) {
        return EFX_RENDER_ERR_BUDGET;
    }
    if (R.record_count >= R.record_cap) {
        if (!pool_grow((void **)&R.records, &R.record_cap, R.record_count + 1,
                       sizeof(efx_record), 256)) {
            return EFX_RENDER_ERR_NOMEM;
        }
    }
    R.records[R.record_count++] = *rec;
    return EFX_RENDER_OK;
}

int efx_render_quad(float x, float y, float w, float h, uint64_t texture,
                    const float color[4], float rotation_deg, float scale,
                    const float src_rect[4], int has_src,
                    float origin_x, float origin_y, int blend_override) {
    ensure_state();
    /* F5a: the active render target is the rendering surface — the default
       camera frame follows it exactly as it follows the window */
    int surf_w = 0, surf_h = 0;
    efx_render_surface_size(&surf_w, &surf_h);
    float fw = R.camera.frame_w > 0.0f
                   ? R.camera.frame_w
                   : (float)(surf_w ? surf_w : 640);
    float fh = R.camera.frame_h > 0.0f
                   ? R.camera.frame_h
                   : (float)(surf_h ? surf_h : 480);
    float cx = R.camera.frame_w > 0.0f ? R.camera.x : fw * 0.5f;
    float cy = R.camera.frame_h > 0.0f ? R.camera.y : fh * 0.5f;

    efx_camera2d cam = R.camera;
    cam.x = cx;
    cam.y = cy;
    efx_affine view = efx_camera_matrix(&cam, fw, fh);
    efx_affine model = efx_quad_matrix(x, y, origin_x, origin_y,
                                       rotation_deg, scale);

    if (texture && texture == R.active_target) {
        /* feedback-loop guard (ADR 0028): a target is never sampled
           while it is the active attachment */
        return EFX_RENDER_ERR_FEEDBACK;
    }

    efx_record rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = EFX_RECORD_QUAD;
    efx_quad_record *q = &rec.u.quad;
    q->m = efx_affine_mul(view, model);
    q->frame_w = fw;
    q->frame_h = fh;
    q->w = w;
    q->h = h;

    if (!texture) {
        texture = efx_render_white_texture();
        if (!texture) {
            return EFX_RENDER_ERR_SINK;
        }
    }
    q->texture = texture;
    {
        /* F5a texture coercion: the sampled source may be a Texture or a
           RenderTarget — resolve the size through the unified lookup */
        int tw = 0, th = 0;
        efx_render_sample_size(texture, &tw, &th);
        q->tw = (float)tw;
        q->th = (float)th;
    }

    if (has_src) {
        q->sx = src_rect[0];
        q->sy = src_rect[1];
        q->sw = src_rect[2];
        q->sh = src_rect[3];
    } else {
        q->sx = 0.0f;
        q->sy = 0.0f;
        q->sw = (float)q->tw;
        q->sh = (float)q->th;
    }
    color_or_white(q->color, color);
    q->blend = (uint8_t)(blend_override < 0 ? R.blend : blend_override);
    return record_push(&rec);
}

int efx_render_mesh(uint64_t mesh, const float transform[16],
                    const float color[4], int skinned, int depth_write,
                    int cull) {
    ensure_state();
    if (!mesh || !efx_render_mesh_alive(mesh)) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (skinned && !efx_render_mesh_skinned(mesh)) {
        return EFX_RENDER_ERR_RIG;
    }
    /* feedback-loop guard (ADR 0028): a mesh draw samples its bound
       maps — none of them may be the target being drawn into */
    if (R.active_target) {
        int surfaces = efx_render_mesh_surface_count(mesh);
        for (int i = 0; i < surfaces; i++) {
            efx_material mat;
            efx_render_mesh_surface_material(mesh, i, &mat);
            const uint64_t maps[5] = {mat.ambient_map, mat.diffuse_map,
                                      mat.specular_map, mat.emissive_map,
                                      mat.alpha_mask};
            for (int k = 0; k < 5; k++) {
                if (maps[k] && maps[k] == R.active_target) {
                    return EFX_RENDER_ERR_FEEDBACK;
                }
            }
        }
    }
    /* frustum culling (ADR 0065): skip the record entirely for a draw that
       is provably fully outside the active camera's view. Posed (skinned)
       draws bypass the test — bind-pose bounds do not describe posed
       geometry — and every ambiguous state keeps the draw (fail open). */
    if (cull && !skinned) {
        mesh_slot *ms = mesh_get(mesh);
        float bmin[3], bmax[3];
        memcpy(bmin, ms->bmin, sizeof(bmin));
        memcpy(bmax, ms->bmax, sizeof(bmax));
        /* inverted override box (min > max): degenerate bounds, fail open */
        int inverted = 0;
        for (int k = 0; k < 3; k++) {
            if (bmin[k] > bmax[k]) {
                inverted = 1;
            }
        }
        int sw = 0, sh = 0;
        float aspect = 0.0f;
        efx_render_surface_size(&sw, &sh);
        if (!inverted && sw > 0 && sh > 0) {
            /* the draw's model matrix: the option transform, else identity
               (the record builds the same default below) */
            float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            const float *model = transform ? transform : ident;
            aspect = (float)sw / (float)sh;
            if (aspect > 0.0f && isfinite(aspect)) {
                float wmin[3], wmax[3];
                if (mesh_world_box(
                        model, bmin, bmax, wmin, wmax) &&
                    mesh_frustum_cull(&R.camera3d, aspect, wmin, wmax)) {
                    return EFX_RENDER_OK; /* culled: no record, no budget */
                }
            }
        }
    }
    efx_record rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = EFX_RECORD_MESH;
    efx_mesh_record *mr = &rec.u.mesh;
    mr->mesh = mesh;
    for (int i = 0; i < 16; i++) {
        mr->transform[i] = transform ? transform[i] : 0.0f;
    }
    if (!transform) {
        mr->transform[0] = 1.0f;
        mr->transform[5] = 1.0f;
        mr->transform[10] = 1.0f;
        mr->transform[15] = 1.0f;
    }
    color_or_white(mr->color, color);
    mr->camera = R.camera3d;
    mr->lights = R.lights;   /* value snapshot (ADR 0026) */
    mr->fog = R.fog;         /* value snapshot (ADR 0019) */
    mr->blend = (uint8_t)R.blend;
    {
        /* D4: resolve each surface's blend at record time — its material
           override when set, else the frame blend state — so a later
           setBlendMode or material rebind cannot change the record */
        int surfaces = efx_render_mesh_surface_count(mesh);
        for (int i = 0; i < surfaces && i < EFX_MESH_MAX_SURFACES; i++) {
            efx_material mat;
            int has = efx_render_mesh_surface_material(mesh, i, &mat);
            int b = (has && mat.blend >= 0) ? mat.blend : R.blend;
            mr->surface_blend[i] = (uint8_t)b;
            /* material-alpha-modes: the transparent-pass route is snapshotted
               like the blend, so a later rebind cannot change the record */
            mr->surface_alpha[i] =
                (uint8_t)(has ? mat.alpha_mode : EFX_ALPHA_OPAQUE);
        }
    }
    mr->skinned = skinned ? 1 : 0;
    mr->depth_write = depth_write ? 1 : 0;
    return record_push(&rec);
}

/* --------------------------------------------- F11 particles + billboards */
const efx_record *efx_render_records(int *count) {
    if (count) {
        *count = R.record_count;
    }
    return R.records;
}

const efx_draw_run *efx_render_runs(int *count) {
    /* batch consecutive same-texture, same-blend quad records (design
       D3/D10); mesh records break runs */
    static efx_draw_run *runs = NULL;
    static int run_cap = 0;
    int n = 0;
    for (int i = 0; i < R.record_count; i++) {
        efx_record *r = &R.records[i];
        if (r->type != EFX_RECORD_QUAD) {
            continue;
        }
        if (n > 0 && runs[n - 1].texture == r->u.quad.texture &&
            runs[n - 1].blend == r->u.quad.blend &&
            runs[n - 1].start + runs[n - 1].count == i) {
            runs[n - 1].count++;
        } else {
            if (n >= run_cap) {
                if (!pool_grow((void **)&runs, &run_cap, n + 1,
                               sizeof(efx_draw_run), 64)) {
                    if (count) *count = 0;
                    return NULL;
                }
            }
            runs[n].start = i;
            runs[n].count = 1;
            runs[n].texture = r->u.quad.texture;
            runs[n].blend = r->u.quad.blend;
            n++;
        }
    }
    if (count) {
        *count = n;
    }
    return runs;
}

const efx_billboard_run *efx_render_billboard_runs(int *count) {
    /* batch record-adjacent billboard records sharing texture, blend and
       depth-test setting (playback-quick-wins D1/D2); any intervening
       record of another kind breaks adjacency and the run */
    static efx_billboard_run *runs = NULL;
    static int run_cap = 0;
    int n = 0;
    for (int i = 0; i < R.record_count; i++) {
        efx_record *r = &R.records[i];
        if (r->type != EFX_RECORD_BILLBOARD) {
            continue;
        }
        if (n > 0 && runs[n - 1].texture == r->u.billboard.texture &&
            runs[n - 1].blend == r->u.billboard.blend &&
            runs[n - 1].depth_test == r->u.billboard.depth_test &&
            runs[n - 1].start + runs[n - 1].count == i) {
            runs[n - 1].count++;
        } else {
            if (n >= run_cap) {
                if (!pool_grow((void **)&runs, &run_cap, n + 1,
                               sizeof(efx_billboard_run), 64)) {
                    if (count) *count = 0;
                    return NULL;
                }
            }
            runs[n].start = i;
            runs[n].count = 1;
            runs[n].texture = r->u.billboard.texture;
            runs[n].blend = r->u.billboard.blend;
            runs[n].depth_test = r->u.billboard.depth_test;
            n++;
        }
    }
    if (count) {
        *count = n;
    }
    return runs;
}

void efx_render_begin_frame(void) {
    ensure_state();
    R.record_count = 0;
    R.active_target = 0; /* a new frame never inherits an open segment */
    /* blend is frame-local render state: a mode set at load time must not
       leak into later frames (per-object overrides still win per draw) */
    R.blend = EFX_BLEND_ALPHA;
}

void efx_render_end_frame(void) {
    /* release deferred meshes first: destroying a mesh releases its material
       map references, which may schedule texture/target releases for this
       frame; textures next, render targets last (they may have been queued
       by the mesh releases above) */
    if (R.sink && R.sink->destroy_mesh) {
        for (int i = 0; i < R.deferred_mesh_count; i++) {
            int idx = R.deferred_mesh[i];
            R.sink->destroy_mesh(R.sink->ud, R.meshes[idx].native);
            R.meshes[idx].native = NULL;
            mesh_materials_free(&R.meshes[idx]);
            R.meshes[idx].used = 0;
        }
    }
    R.deferred_mesh_count = 0;
    /* F11: release deferred particle systems before textures/targets — a
       system drop may release a texture it retained */
    for (int i = 0; i < R.deferred_ps_count; i++) {
        int idx = R.deferred_ps[i];
        ps_free_pool(&R.ps[idx]);
        R.ps[idx].used = 0;
    }
    R.deferred_ps_count = 0;
    if (R.sink && R.sink->destroy_texture) {
        for (int i = 0; i < R.deferred_tex_count; i++) {
            int idx = R.deferred_tex[i];
            R.sink->destroy_texture(R.sink->ud, R.slots[idx].native);
            R.slots[idx].native = NULL;
            R.slots[idx].bind_refs = 0;
            R.slots[idx].release_pending = 0;
            R.slots[idx].used = 0;
        }
    }
    R.deferred_tex_count = 0;
    if (R.sink && R.sink->destroy_render_target) {
        for (int i = 0; i < R.deferred_rt_count; i++) {
            int idx = R.deferred_rt[i];
            R.sink->destroy_render_target(R.sink->ud, R.targets[idx].native);
            R.targets[idx].native = NULL;
            R.targets[idx].bind_refs = 0;
            R.targets[idx].release_pending = 0;
            R.targets[idx].used = 0;
        }
    }
    R.deferred_rt_count = 0;
}

/* ------------------------------------------------------------- shutdown */

/* Shared teardown for shutdown and the in-place swap reset (ADR 0057):
 * release every registered GPU resource and free the arenas, then zero R and
 * the post state. `sink_shutdown` tells the sink it is going away (shutdown);
 * the reset path keeps the sink alive so the same sg context can keep
 * drawing. */
static void render_teardown_resources(int sink_shutdown) {
    if (R.sink) {
        for (int i = 0; i < R.slot_count; i++) {
            if (R.slots[i].used && R.slots[i].native) {
                R.sink->destroy_texture(R.sink->ud, R.slots[i].native);
            }
        }
        for (int i = 0; i < R.mesh_count; i++) {
            if (R.meshes[i].used && R.meshes[i].native &&
                R.sink->destroy_mesh) {
                R.sink->destroy_mesh(R.sink->ud, R.meshes[i].native);
            }
            pending_free(&R.meshes[i].pending);
            mesh_materials_free(&R.meshes[i]);
        }
        if (R.sink->destroy_render_target) {
            for (int i = 0; i < R.target_count; i++) {
                if (R.targets[i].used && R.targets[i].native) {
                    R.sink->destroy_render_target(R.sink->ud,
                                                  R.targets[i].native);
                }
            }
        }
        if (sink_shutdown && R.sink->shutdown) {
            R.sink->shutdown(R.sink->ud);
        }
    }
    free(R.slots);
    free(R.meshes);
    free(R.targets);
    for (int i = 0; i < R.ps_count; i++) {
        free(R.ps[i].parts);
        free(R.ps[i].views);
    }
    free(R.ps);
    free(R.deferred_ps);
    free(R.records);
    free(R.deferred_tex);
    free(R.deferred_mesh);
    free(R.deferred_rt);
    memset(&R, 0, sizeof(R));
    post_clear();
}

void efx_render_shutdown(void) {
    render_teardown_resources(1);
    state_ready = 0;
}

void efx_render_reset(void) {
    /* In-place game swap (ADR 0057): drop every game-owned resource and
     * re-apply the documented defaults while keeping the installed GPU sink
     * (and the sg context/pipelines behind it) alive. The engine-owned white
     * texture is released too; the caller rebinds the pipeline
     * (efx_pipeline_rebind) to recreate it before the next draw. */
    const efx_render_sink *sink = R.sink;
    int vw = R.viewport_w;
    int vh = R.viewport_h;
    render_teardown_resources(0);
    R.sink = sink;
    R.viewport_w = vw;
    R.viewport_h = vh;
    apply_default_state();
    state_ready = 1;
}

/* ------------------------------------------------------- CPU lighting (F4a) */

