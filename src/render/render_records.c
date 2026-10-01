#include "render_internal.h"

render_state R;

void efx_render_install_sink(const efx_render_sink *sink) {
    R.sink = sink;
    flush_pending_uploads();
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
    .shininess = 32.0f};

static void apply_default_state(void) {
    R.camera = DEFAULT_CAMERA2D;
    R.camera3d = DEFAULT_CAMERA3D;
    memset(&R.lights, 0, sizeof(R.lights)); /* every light disabled */
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
                    float origin_x, float origin_y) {
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
        /* feedback-loop guard (F5a design D4): a target is never sampled
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
    q->blend = (uint8_t)R.blend;
    return record_push(&rec);
}

int efx_render_mesh(uint64_t mesh, const float transform[16],
                    const float color[4], int skinned) {
    ensure_state();
    if (!mesh || !efx_render_mesh_alive(mesh)) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (skinned && !efx_render_mesh_skinned(mesh)) {
        return EFX_RENDER_ERR_RIG;
    }
    /* feedback-loop guard (F5a design D4): a mesh draw samples its bound
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
    mr->lights = R.lights;   /* value snapshot (F4a design D4) */
    mr->blend = (uint8_t)R.blend;
    mr->skinned = skinned ? 1 : 0;
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

void efx_render_begin_frame(void) {
    ensure_state();
    R.record_count = 0;
    R.active_target = 0; /* a new frame never inherits an open segment */
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

void efx_render_shutdown(void) {
    if (R.sink) {
        for (int i = 0; i < R.slot_count; i++) {
            if (R.slots[i].used && R.slots[i].native) {
                R.sink->destroy_texture(R.sink->ud, R.slots[i].native);
            }
            free(R.slots[i].pending);
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
        if (R.sink->shutdown) {
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
    state_ready = 0;
}

/* ------------------------------------------------------- CPU lighting (F4a) */

