#include "render/render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG2RAD (float)(M_PI / 180.0)

/* ---------------------------------------------------------------- state */

static void flush_pending_uploads(void);
static int record_push(efx_record rec, size_t bytes);

typedef struct {
    int used;
    int alive;
    int permanent;
    int bind_refs;       /* F4b: material map retain count (design D6) */
    int release_pending; /* F4b: destroy() called while retained */
    uint32_t gen;
    int w, h;
    void *native;
    uint8_t *pending; /* RGBA bytes queued before a sink existed */
} tex_slot;

/* F5a render target: an offscreen color+depth attachment pair; native is
 * owned by the sink and doubles as the sampling source (texture coercion,
 * design D1). No CPU-side pixel payload: before a sink exists only the
 * slot (with its size) is allocated. */
typedef struct {
    int used;
    int alive;
    int bind_refs;
    int release_pending;
    uint32_t gen;
    int w, h;
    void *native;
} rt_slot;

static void finalize_target_release(rt_slot *t);

/* pending mesh upload: interleaved surfaces queued before a sink existed */
typedef struct {
    int count;
    efx_mesh_gpu_surface *surfs; /* count entries; pointers into data */
    uint8_t *data;               /* single backing block */
} mesh_pending;

typedef struct {
    int used;
    int alive;
    uint32_t gen;
    int surface_count;
    void *native;
    mesh_pending pending;
    efx_material *materials;  /* surface_count entries (F4a) */
    uint8_t *has_material;    /* surface_count flags */
} mesh_slot;

static struct {
    const efx_render_sink *sink;
    int viewport_w, viewport_h;
    efx_camera2d camera;      /* frame_w == 0 => default camera */
    efx_camera3d camera3d;
    efx_light_set lights;     /* fixed bank; all disabled until set (F4a) */
    float clear_color[4];
    int blend;
    uint64_t active_target;   /* F5a: rendering surface for new records */
    tex_slot *slots;
    int slot_count, slot_cap;
    mesh_slot *meshes;
    int mesh_count, mesh_cap;
    rt_slot *targets;
    int target_count, target_cap;
    uint64_t white_handle;
    efx_record *records;
    int record_count, record_cap;
    /* deferred texture/mesh/target destroys (slot indexes) */
    int *deferred_tex;
    int deferred_tex_count, deferred_tex_cap;
    int *deferred_mesh;
    int deferred_mesh_count, deferred_mesh_cap;
    int *deferred_rt;
    int deferred_rt_count, deferred_rt_cap;
} R;

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
static void default_camera(efx_camera2d *cam);
static void default_camera3d(efx_camera3d *cam);
static void default_lights(efx_light_set *lights);

static void ensure_state(void) {
    if (state_ready) {
        return;
    }
    default_camera(&R.camera);
    default_camera3d(&R.camera3d);
    default_lights(&R.lights);
    R.clear_color[0] = 0.0f;
    R.clear_color[1] = 0.0f;
    R.clear_color[2] = 0.0f;
    R.clear_color[3] = 1.0f;
    R.blend = EFX_BLEND_ALPHA;
    state_ready = 1;
}

static void default_camera(efx_camera2d *cam) {
    cam->frame_w = 0.0f;
    cam->frame_h = 0.0f;
    cam->x = 0.0f;
    cam->y = 0.0f;
    cam->zoom = 1.0f;
    cam->rotation = 0.0f;
}

static void default_camera3d(efx_camera3d *cam) {
    cam->pos[0] = 0.0f;
    cam->pos[1] = 0.0f;
    cam->pos[2] = 1.0f;
    cam->target[0] = 0.0f;
    cam->target[1] = 0.0f;
    cam->target[2] = 0.0f;
    cam->fov = 60.0f;
    cam->near_z = 0.1f;
    cam->far_z = 100.0f;
}

static void default_lights(efx_light_set *lights) {
    memset(lights, 0, sizeof(*lights)); /* every light disabled */
}

void efx_material_default(efx_material *m) {
    if (!m) {
        return;
    }
    /* ambient/specular/emissive black, diffuse white, shininess 32 */
    m->ambient[0] = 0.0f; m->ambient[1] = 0.0f;
    m->ambient[2] = 0.0f; m->ambient[3] = 1.0f;
    m->diffuse[0] = 1.0f; m->diffuse[1] = 1.0f;
    m->diffuse[2] = 1.0f; m->diffuse[3] = 1.0f;
    m->specular[0] = 0.0f; m->specular[1] = 0.0f;
    m->specular[2] = 0.0f; m->specular[3] = 1.0f;
    m->emissive[0] = 0.0f; m->emissive[1] = 0.0f;
    m->emissive[2] = 0.0f; m->emissive[3] = 1.0f;
    m->shininess = 32.0f;
    m->ambient_map = 0;
    m->diffuse_map = 0;
    m->specular_map = 0;
    m->emissive_map = 0;
    m->alpha_mask = 0;
}

void efx_render_reset_state(void) {
    default_camera(&R.camera);
    default_camera3d(&R.camera3d);
    default_lights(&R.lights);
    R.clear_color[0] = 0.0f;
    R.clear_color[1] = 0.0f;
    R.clear_color[2] = 0.0f;
    R.clear_color[3] = 1.0f;
    R.blend = EFX_BLEND_ALPHA;
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

/* ------------------------------------------------------------ textures */

static tex_slot *slot_get(uint64_t h) {
    if ((h & 0xF0000000ull) != 0) {
        return NULL; /* render-target tag: never a texture handle */
    }
    uint32_t idx = (uint32_t)(h & 0xffffffffu);
    uint32_t gen = (uint32_t)(h >> 32);
    if (idx == 0 || (size_t)idx > (size_t)R.slot_count) {
        return NULL;
    }
    tex_slot *s = &R.slots[idx - 1];
    if (!s->used || s->gen != gen) {
        return NULL;
    }
    return s;
}

static mesh_slot *mesh_get(uint64_t h) {
    uint32_t idx = (uint32_t)(h & 0xffffffffu);
    uint32_t gen = (uint32_t)(h >> 32);
    if (idx == 0 || (size_t)idx > (size_t)R.mesh_count) {
        return NULL;
    }
    mesh_slot *m = &R.meshes[idx - 1];
    if (!m->used || m->gen != gen) {
        return NULL;
    }
    return m;
}

static rt_slot *rt_get(uint64_t h) {
    /* F5a handle namespaces are disjoint: render-target handles carry tag
     * bits 28..31 (value 1) in the index half, keeping every handle small
     * enough to stay exact through the web bridge's double wire format */
    if ((h & 0xF0000000ull) != 0x10000000ull) {
        return NULL;
    }
    uint32_t idx = (uint32_t)(h & 0x0FFFFFFFu);
    uint32_t gen = (uint32_t)(h >> 32);
    if (idx == 0 || (size_t)idx > (size_t)R.target_count) {
        return NULL;
    }
    rt_slot *t = &R.targets[idx - 1];
    if (!t->used || t->gen != gen) {
        return NULL;
    }
    return t;
}

static void flush_pending_uploads(void) {
    if (!R.sink || !R.sink->create_texture) {
        return;
    }
    for (int i = 0; i < R.slot_count; i++) {
        tex_slot *s = &R.slots[i];
        /* upload queued textures, including ones destroyed while retained
           maps still reference them (F4b design D6) */
        if (s->used && (s->alive || s->bind_refs > 0) && !s->native &&
            s->pending) {
            s->native = R.sink->create_texture(R.sink->ud, s->w, s->h, s->pending);
            free(s->pending);
            s->pending = NULL;
        }
    }
    if (!R.sink->create_mesh) {
        return;
    }
    for (int i = 0; i < R.mesh_count; i++) {
        mesh_slot *m = &R.meshes[i];
        if (m->used && m->alive && !m->native && m->pending.data) {
            m->native = R.sink->create_mesh(R.sink->ud, m->pending.surfs,
                                            m->pending.count);
            free(m->pending.data);
            free(m->pending.surfs);
            m->pending.data = NULL;
            m->pending.surfs = NULL;
            m->pending.count = 0;
        }
    }
    if (!R.sink->create_render_target) {
        return;
    }
    for (int i = 0; i < R.target_count; i++) {
        rt_slot *t = &R.targets[i];
        /* create queued targets, including ones destroyed while retained
           maps still reference them (the F4b retention rule, extended) */
        if (t->used && (t->alive || t->bind_refs > 0) && !t->native) {
            t->native = R.sink->create_render_target(R.sink->ud, t->w, t->h);
        }
    }
}

uint64_t efx_render_texture_create(int w, int h, const uint8_t *rgba) {
    if (!R.sink || !R.sink->create_texture) {
        /* no GPU surface yet: queue the upload (top-level main.js code) */
        int idx = -1;
        if (R.slot_count >= R.slot_cap) {
            int cap = R.slot_cap ? R.slot_cap * 2 : 64;
            tex_slot *grown = realloc(R.slots, (size_t)cap * sizeof(tex_slot));
            if (!grown) {
                return 0;
            }
            R.slots = grown;
            R.slot_cap = cap;
        }
        tex_slot *s = &R.slots[R.slot_count];
        s->pending = malloc((size_t)w * h * 4);
        if (!s->pending) {
            return 0;
        }
        memcpy(s->pending, rgba, (size_t)w * h * 4);
        s->used = 1;
        s->alive = 1;
        s->permanent = 0;
        s->bind_refs = 0;
        s->release_pending = 0;
        s->gen++;
        s->w = w;
        s->h = h;
        s->native = NULL;
        idx = R.slot_count++;
        return ((uint64_t)s->gen << 32) | (uint64_t)(idx + 1);
    }
    void *native = R.sink->create_texture(R.sink->ud, w, h, rgba);
    if (!native) {
        return 0;
    }
    /* find a free slot or grow */
    tex_slot *s = NULL;
    for (int i = 0; i < R.slot_count; i++) {
        if (!R.slots[i].used) {
            s = &R.slots[i];
            break;
        }
    }
    if (!s) {
        if (R.slot_count >= R.slot_cap) {
            int cap = R.slot_cap ? R.slot_cap * 2 : 64;
            tex_slot *grown = realloc(R.slots, (size_t)cap * sizeof(tex_slot));
            if (!grown) {
                R.sink->destroy_texture(R.sink->ud, native);
                return 0;
            }
            R.slots = grown;
            R.slot_cap = cap;
        }
        s = &R.slots[R.slot_count++];
        s->gen = 0;
    }
    s->used = 1;
    s->alive = 1;
    s->permanent = 0;
    s->bind_refs = 0;
    s->release_pending = 0;
    s->gen++;
    s->w = w;
    s->h = h;
    s->native = native;
    s->pending = NULL;
    uint32_t idx = (uint32_t)(s - R.slots) + 1;
    return ((uint64_t)s->gen << 32) | (uint64_t)idx;
}

/* schedule the native release of a texture slot at frame end (records may
 * reference the texture until playback finishes — js-api lifecycle rules) */
static void schedule_texture_native_release(tex_slot *s) {
    if (!R.sink || !R.sink->destroy_texture || !s->native) {
        return;
    }
    if (R.deferred_tex_count >= R.deferred_tex_cap) {
        int cap = R.deferred_tex_cap ? R.deferred_tex_cap * 2 : 16;
        int *grown = realloc(R.deferred_tex, (size_t)cap * sizeof(int));
        if (!grown) {
            return; /* best effort; slot stays until shutdown */
        }
        R.deferred_tex = grown;
        R.deferred_tex_cap = cap;
    }
    R.deferred_tex[R.deferred_tex_count++] = (int)(s - R.slots);
}

/* finish a release once no material references the slot (design D6) */
static void finalize_texture_release(tex_slot *s) {
    if (s->pending) {
        free(s->pending);
        s->pending = NULL;
        return;
    }
    schedule_texture_native_release(s);
}

/* F4b map retention (design D6): material bindings keep their maps alive.
 * F5a: maps may reference a Texture or a RenderTarget — the retain/ref
 * helpers dispatch on whichever registry holds the handle. */
static void map_bind_retain(uint64_t h) {
    tex_slot *ts = slot_get(h);
    if (ts) {
        ts->bind_refs++;
        return;
    }
    rt_slot *t = rt_get(h);
    if (t) {
        t->bind_refs++;
    }
}

static void texture_bind_retain(uint64_t h) {
    map_bind_retain(h);
}

static void map_bind_release(uint64_t h) {
    tex_slot *ts = slot_get(h);
    if (ts) {
        if (ts->bind_refs > 0) {
            ts->bind_refs--;
        }
        if (ts->bind_refs == 0 && ts->release_pending) {
            ts->release_pending = 0;
            finalize_texture_release(ts);
        }
        return;
    }
    rt_slot *t = rt_get(h);
    if (t && t->bind_refs > 0) {
        t->bind_refs--;
        if (t->bind_refs == 0 && t->release_pending) {
            t->release_pending = 0;
            finalize_target_release(t);
        }
    }
}

static void texture_bind_release(uint64_t h) {
    map_bind_release(h);
}

static int texture_release(uint64_t h, tex_slot **out_slot) {
    tex_slot *s = slot_get(h);
    if (!s) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (s->permanent) {
        return EFX_RENDER_ERR_PERMANENT;
    }
    if (!s->alive) {
        return EFX_RENDER_OK; /* destroy() is idempotent */
    }
    s->alive = 0;
    if (s->bind_refs > 0) {
        /* a bound map keeps the texture alive until the binding is released */
        s->release_pending = 1;
        if (out_slot) {
            *out_slot = s;
        }
        return EFX_RENDER_OK;
    }
    finalize_texture_release(s);
    if (out_slot) {
        *out_slot = s;
    }
    return EFX_RENDER_OK;
}

int efx_render_texture_destroy(uint64_t h) {
    return texture_release(h, NULL);
}

int efx_render_texture_alive(uint64_t h) {
    tex_slot *s = slot_get(h);
    return s && s->alive;
}

int efx_render_texture_ref_count(uint64_t h) {
    tex_slot *s = slot_get(h);
    return s ? s->bind_refs : -1;
}

/* F4b: retain/release every map referenced by a material snapshot (0 = none) */
static void material_retain_maps(const efx_material *m) {
    if (!m) {
        return;
    }
    texture_bind_retain(m->ambient_map);
    texture_bind_retain(m->diffuse_map);
    texture_bind_retain(m->specular_map);
    texture_bind_retain(m->emissive_map);
    texture_bind_retain(m->alpha_mask);
}

static void material_release_maps(const efx_material *m) {
    if (!m) {
        return;
    }
    texture_bind_release(m->ambient_map);
    texture_bind_release(m->diffuse_map);
    texture_bind_release(m->specular_map);
    texture_bind_release(m->emissive_map);
    texture_bind_release(m->alpha_mask);
}

void efx_render_texture_size(uint64_t handle, int *out_w, int *out_h) {
    tex_slot *s = slot_get(handle);
    if (s) {
        if (out_w) *out_w = s->w;
        if (out_h) *out_h = s->h;
    }
}

void *efx_render_texture_native(uint64_t h) {
    tex_slot *s = slot_get(h);
    return s ? s->native : NULL;
}

uint64_t efx_render_white_texture(void) {
    if (R.white_handle) {
        return R.white_handle;
    }
    if (!R.sink || !R.sink->create_texture) {
        return 0;
    }
    static const uint8_t white[4] = {255, 255, 255, 255};
    uint64_t h = efx_render_texture_create(1, 1, white);
    if (h) {
        tex_slot *s = slot_get(h);
        s->permanent = 1;
        R.white_handle = h;
    }
    return h;
}

/* ------------------------------------------------------ render targets */

uint64_t efx_render_target_create(int w, int h) {
    if (w <= 0 || h <= 0 || w > EFX_RENDER_MAX_TARGET_SIZE ||
        h > EFX_RENDER_MAX_TARGET_SIZE) {
        return 0;
    }
    rt_slot *t = NULL;
    for (int i = 0; i < R.target_count; i++) {
        if (!R.targets[i].used) {
            t = &R.targets[i];
            break;
        }
    }
    if (!t) {
        if (R.target_count >= R.target_cap) {
            int cap = R.target_cap ? R.target_cap * 2 : 16;
            rt_slot *grown = realloc(R.targets, (size_t)cap * sizeof(rt_slot));
            if (!grown) {
                return 0;
            }
            R.targets = grown;
            R.target_cap = cap;
        }
        t = &R.targets[R.target_count++];
        t->gen = 0;
    }
    void *native = NULL;
    if (R.sink && R.sink->create_render_target) {
        native = R.sink->create_render_target(R.sink->ud, w, h);
        if (!native) {
            return 0;
        }
    }
    t->used = 1;
    t->alive = 1;
    t->bind_refs = 0;
    t->release_pending = 0;
    t->gen++;
    t->w = w;
    t->h = h;
    t->native = native;
    uint32_t idx = (uint32_t)(t - R.targets) + 1;
    return ((uint64_t)t->gen << 32) | 0x10000000ull | (uint64_t)idx;
}

static void schedule_target_native_release(rt_slot *t) {
    if (!R.sink || !R.sink->destroy_render_target || !t->native) {
        return;
    }
    if (R.deferred_rt_count >= R.deferred_rt_cap) {
        int cap = R.deferred_rt_cap ? R.deferred_rt_cap * 2 : 16;
        int *grown = realloc(R.deferred_rt, (size_t)cap * sizeof(int));
        if (!grown) {
            return; /* best effort; slot stays until shutdown */
        }
        R.deferred_rt = grown;
        R.deferred_rt_cap = cap;
    }
    R.deferred_rt[R.deferred_rt_count++] = (int)(t - R.targets);
}

static void finalize_target_release(rt_slot *t) {
    schedule_target_native_release(t);
}

int efx_render_target_destroy(uint64_t h) {
    rt_slot *t = rt_get(h);
    if (!t) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (!t->alive) {
        return EFX_RENDER_OK; /* destroy() is idempotent */
    }
    t->alive = 0;
    if (t->bind_refs > 0) {
        /* a bound map keeps the target alive until the binding is released */
        t->release_pending = 1;
        return EFX_RENDER_OK;
    }
    finalize_target_release(t);
    return EFX_RENDER_OK;
}

int efx_render_target_alive(uint64_t h) {
    rt_slot *t = rt_get(h);
    return t && t->alive;
}

int efx_render_target_ref_count(uint64_t h) {
    rt_slot *t = rt_get(h);
    return t ? t->bind_refs : -1;
}

void efx_render_target_size(uint64_t h, int *out_w, int *out_h) {
    rt_slot *t = rt_get(h);
    if (t) {
        if (out_w) *out_w = t->w;
        if (out_h) *out_h = t->h;
    }
}

void *efx_render_target_native(uint64_t h) {
    rt_slot *t = rt_get(h);
    return t ? t->native : NULL;
}

/* ------------------------------------------------- render redirection */

uint64_t efx_render_active_target(void) {
    return R.active_target;
}

int efx_render_begin_target(uint64_t h) {
    ensure_state();
    rt_slot *t = rt_get(h);
    if (!t || !t->alive) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (R.active_target) {
        return EFX_RENDER_ERR_NESTED;
    }
    efx_record rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = EFX_RECORD_BEGIN_TARGET;
    rec.target = h;
    rec.u.begin_target.target = h;
    efx_render_clear_color(rec.u.begin_target.clear); /* snapshot (design D3) */
    rec.sort_key = (uint32_t)R.record_count;
    int rc = record_push(rec, sizeof(efx_record));
    if (rc != EFX_RENDER_OK) {
        return rc;
    }
    R.active_target = h;
    return EFX_RENDER_OK;
}

int efx_render_end_target(void) {
    ensure_state();
    if (!R.active_target) {
        return EFX_RENDER_ERR_STATE;
    }
    efx_record rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = EFX_RECORD_END_TARGET;
    rec.target = R.active_target;
    rec.u.begin_target.target = R.active_target;
    rec.sort_key = (uint32_t)R.record_count;
    int rc = record_push(rec, sizeof(efx_record));
    if (rc != EFX_RENDER_OK) {
        return rc;
    }
    R.active_target = 0;
    return EFX_RENDER_OK;
}

/* --------------------------------------------------- sample coercion */

int efx_render_sample_alive(uint64_t h) {
    if (!h) {
        return 0;
    }
    tex_slot *ts = slot_get(h);
    if (ts) {
        return ts->alive;
    }
    rt_slot *t = rt_get(h);
    return t && t->alive;
}

void efx_render_sample_size(uint64_t h, int *out_w, int *out_h) {
    tex_slot *ts = slot_get(h);
    if (ts) {
        if (out_w) *out_w = ts->w;
        if (out_h) *out_h = ts->h;
        return;
    }
    efx_render_target_size(h, out_w, out_h);
}

void *efx_render_sample_native(uint64_t h) {
    tex_slot *ts = slot_get(h);
    if (ts) {
        return ts->native;
    }
    return efx_render_target_native(h);
}

void efx_render_surface_size(int *out_w, int *out_h) {
    ensure_state();
    if (R.active_target) {
        efx_render_target_size(R.active_target, out_w, out_h);
        return;
    }
    if (out_w) *out_w = R.viewport_w;
    if (out_h) *out_h = R.viewport_h;
}

/* ------------------------------------------------------ CPU mesh data */

efx_meshdata *efx_meshdata_create(const efx_surface_src *src, int count,
                                  int *err) {
    if (err) *err = EFX_MESHERR_OK;
    if (count < 1 || count > EFX_MESH_MAX_SURFACES) {
        if (err) *err = EFX_MESHERR_COUNT;
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        const efx_surface_src *s = &src[i];
        if (s->positions_len <= 0 || s->positions_len % 3 != 0) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        if (s->normals_len % 3 != 0 || s->uvs_len % 2 != 0 ||
            s->colors_len % 4 != 0) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        int vcount = s->positions_len / 3;
        if ((s->normals_len && s->normals_len / 3 != vcount) ||
            (s->uvs_len && s->uvs_len / 2 != vcount) ||
            (s->colors_len && s->colors_len / 4 != vcount)) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        if (s->indices_len % 3 != 0) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        if (s->indices_len == 0 && vcount % 3 != 0) {
            /* non-indexed surfaces draw as a triangle list */
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        for (int j = 0; j < s->indices_len; j++) {
            if ((size_t)s->indices[j] >= (size_t)vcount) {
                if (err) *err = EFX_MESHERR_INDEX;
                return NULL;
            }
        }
    }

    efx_meshdata *md = calloc(1, sizeof(efx_meshdata));
    if (!md) {
        if (err) *err = EFX_MESHERR_NOMEM;
        return NULL;
    }
    md->surface_count = count;
    md->surfaces = calloc((size_t)count, sizeof(efx_surface));
    if (!md->surfaces) {
        free(md);
        if (err) *err = EFX_MESHERR_NOMEM;
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        const efx_surface_src *s = &src[i];
        efx_surface *d = &md->surfaces[i];
        d->vertex_count = s->positions_len / 3;
        d->index_count = s->indices_len;
        size_t fb = sizeof(float);
        d->positions = malloc((size_t)s->positions_len * fb);
        if (d->positions) memcpy(d->positions, s->positions,
                                 (size_t)s->positions_len * fb);
        if (s->normals_len) {
            d->normals = malloc((size_t)s->normals_len * fb);
            if (d->normals) memcpy(d->normals, s->normals,
                                   (size_t)s->normals_len * fb);
        }
        if (s->uvs_len) {
            d->uvs = malloc((size_t)s->uvs_len * fb);
            if (d->uvs) memcpy(d->uvs, s->uvs, (size_t)s->uvs_len * fb);
        }
        if (s->colors_len) {
            d->colors = malloc((size_t)s->colors_len * fb);
            if (d->colors) memcpy(d->colors, s->colors,
                                  (size_t)s->colors_len * fb);
        }
        if (s->indices_len) {
            d->indices = malloc((size_t)s->indices_len * sizeof(uint32_t));
            if (d->indices) memcpy(d->indices, s->indices,
                                   (size_t)s->indices_len * sizeof(uint32_t));
        }
        int bad = (s->positions_len && !d->positions) ||
                  (s->normals_len && !d->normals) ||
                  (s->uvs_len && !d->uvs) ||
                  (s->colors_len && !d->colors) ||
                  (s->indices_len && !d->indices);
        if (bad) {
            efx_meshdata_destroy(md);
            if (err) *err = EFX_MESHERR_NOMEM;
            return NULL;
        }
    }
    return md;
}

void efx_meshdata_destroy(efx_meshdata *md) {
    if (!md) {
        return;
    }
    if (md->surfaces) {
        for (int i = 0; i < md->surface_count; i++) {
            efx_surface *s = &md->surfaces[i];
            material_release_maps(&s->material);
            free(s->positions);
            free(s->normals);
            free(s->uvs);
            free(s->colors);
            free(s->indices);
        }
        free(md->surfaces);
    }
    free(md);
}

void efx_meshdata_set_material(efx_meshdata *md, int index,
                               const efx_material *mat, int has) {
    if (!md || index < 0 || index >= md->surface_count) {
        return;
    }
    efx_surface *s = &md->surfaces[index];
    material_release_maps(&s->material);
    if (has && mat) {
        s->material = *mat;
        s->has_material = 1;
        material_retain_maps(&s->material);
    } else {
        s->has_material = 0;
        efx_material_default(&s->material);
    }
}

/* ------------------------------------------------------------- meshes */

/* build the interleaved GPU layout (design D1) into a pending block: one
 * backing allocation holding every surface's interleaved vertices followed
 * by its indices; p->surfs points into it */
static int pending_build(mesh_pending *p, const efx_meshdata *md) {
    if (!md || md->surface_count < 1 ||
        md->surface_count > EFX_MESH_MAX_SURFACES) {
        return EFX_RENDER_ERR_NOMEM;
    }
    size_t floats = 0;
    for (int i = 0; i < md->surface_count; i++) {
        floats += (size_t)md->surfaces[i].vertex_count * 12;
    }
    /* non-indexed surfaces draw through a synthesized identity index
       buffer (the pipeline is always-indexed; see pipeline.c) */
    size_t index_words = 0;
    for (int i = 0; i < md->surface_count; i++) {
        const efx_surface *s = &md->surfaces[i];
        index_words += s->index_count ? (size_t)s->index_count
                                      : (size_t)s->vertex_count;
    }
    size_t total_words = floats + index_words;
    p->data = malloc(total_words * sizeof(uint32_t));
    p->surfs = calloc((size_t)md->surface_count, sizeof(efx_mesh_gpu_surface));
    if (!p->data || !p->surfs) {
        free(p->data);
        free(p->surfs);
        p->data = NULL;
        p->surfs = NULL;
        return EFX_RENDER_ERR_NOMEM;
    }
    uint32_t *w = (uint32_t *)p->data;
    for (int i = 0; i < md->surface_count; i++) {
        const efx_surface *s = &md->surfaces[i];
        efx_mesh_gpu_surface *g = &p->surfs[i];
        g->vertex_count = s->vertex_count;
        g->index_count = s->index_count ? s->index_count : s->vertex_count;
        g->interleaved = (const float *)w;
        /* defaults: normal +z, uv 0, color white (design D1) */
        for (int v = 0; v < s->vertex_count; v++) {
            float *dst = (float *)w + v * 12;
            dst[0] = s->positions[v * 3];
            dst[1] = s->positions[v * 3 + 1];
            dst[2] = s->positions[v * 3 + 2];
            if (s->normals) {
                dst[3] = s->normals[v * 3];
                dst[4] = s->normals[v * 3 + 1];
                dst[5] = s->normals[v * 3 + 2];
            } else {
                dst[3] = 0.0f;
                dst[4] = 0.0f;
                dst[5] = 1.0f;
            }
            if (s->uvs) {
                dst[6] = s->uvs[v * 2];
                dst[7] = s->uvs[v * 2 + 1];
            } else {
                dst[6] = 0.0f;
                dst[7] = 0.0f;
            }
            if (s->colors) {
                dst[8] = s->colors[v * 4];
                dst[9] = s->colors[v * 4 + 1];
                dst[10] = s->colors[v * 4 + 2];
                dst[11] = s->colors[v * 4 + 3];
            } else {
                dst[8] = 1.0f;
                dst[9] = 1.0f;
                dst[10] = 1.0f;
                dst[11] = 1.0f;
            }
        }
        w += (size_t)s->vertex_count * 12;
        if (s->indices) {
            g->indices = (const uint32_t *)w;
            memcpy(w, s->indices, (size_t)s->index_count * sizeof(uint32_t));
            w += s->index_count;
        } else {
            g->indices = (const uint32_t *)w;
            for (int j = 0; j < s->vertex_count; j++) {
                *w++ = (uint32_t)j;
            }
        }
    }
    p->count = md->surface_count;
    return EFX_RENDER_OK;
}

static void mesh_materials_free(mesh_slot *m);

static void pending_free(mesh_pending *p) {
    free(p->data);
    free(p->surfs);
    p->data = NULL;
    p->surfs = NULL;
    p->count = 0;
}

uint64_t efx_render_mesh_create(const efx_meshdata *md) {
    if (!md || md->surface_count < 1 ||
        md->surface_count > EFX_MESH_MAX_SURFACES) {
        return 0;
    }
    /* reuse a released slot (generation bump invalidates stale handles),
       else grow */
    mesh_slot *m = NULL;
    for (int i = 0; i < R.mesh_count; i++) {
        if (!R.meshes[i].used) {
            m = &R.meshes[i];
            break;
        }
    }
    if (!m) {
        if (R.mesh_count >= R.mesh_cap) {
            int cap = R.mesh_cap ? R.mesh_cap * 2 : 16;
            mesh_slot *grown = realloc(R.meshes, (size_t)cap * sizeof(mesh_slot));
            if (!grown) {
                return 0;
            }
            R.meshes = grown;
            R.mesh_cap = cap;
        }
        m = &R.meshes[R.mesh_count];
        memset(m, 0, sizeof(*m));
    }
    uint32_t gen = m->gen + 1;
    mesh_pending pending = {0, NULL, NULL};
    void *native = NULL;
    if (R.sink && R.sink->create_mesh) {
        if (pending_build(&pending, md) != EFX_RENDER_OK) {
            return 0;
        }
        native = R.sink->create_mesh(R.sink->ud, pending.surfs, pending.count);
        pending_free(&pending);
        if (!native) {
            return 0;
        }
    } else {
        /* queue the upload (top-level main.js code, headless scripts) */
        if (pending_build(&pending, md) != EFX_RENDER_OK) {
            return 0;
        }
    }
    m->used = 1;
    m->alive = 1;
    m->gen = gen;
    m->surface_count = md->surface_count;
    m->native = native;
    m->pending = pending;
    /* per-surface material bindings (F4a): copy the MeshData snapshot */
    m->materials = calloc((size_t)md->surface_count, sizeof(efx_material));
    m->has_material = calloc((size_t)md->surface_count, sizeof(uint8_t));
    if (!m->materials || !m->has_material) {
        mesh_materials_free(m);
        pending_free(&m->pending);
        if (native && R.sink && R.sink->destroy_mesh) {
            R.sink->destroy_mesh(R.sink->ud, native);
        }
        m->native = NULL;
        m->used = 0;
        return 0;
    }
    for (int i = 0; i < md->surface_count; i++) {
        efx_material_default(&m->materials[i]);
        if (md->surfaces[i].has_material) {
            m->materials[i] = md->surfaces[i].material;
            m->has_material[i] = 1;
        }
        material_retain_maps(&m->materials[i]);
    }
    uint32_t idx = (uint32_t)(m - R.meshes) + 1;
    if ((int)idx > R.mesh_count) {
        R.mesh_count = (int)idx;
    }
    return ((uint64_t)gen << 32) | (uint64_t)idx;
}

static void mesh_materials_free(mesh_slot *m) {
    if (m->materials) {
        for (int i = 0; i < m->surface_count; i++) {
            material_release_maps(&m->materials[i]);
        }
    }
    free(m->materials);
    free(m->has_material);
    m->materials = NULL;
    m->has_material = NULL;
}

static int mesh_release(uint64_t h, mesh_slot **out) {
    mesh_slot *m = mesh_get(h);
    if (!m) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (!m->alive) {
        return EFX_RENDER_OK; /* idempotent */
    }
    m->alive = 0;
    if (m->pending.data) {
        /* upload never happened; release the slot right away */
        pending_free(&m->pending);
        mesh_materials_free(m);
        m->used = 0;
        return EFX_RENDER_OK;
    }
    /* deferred native release at frame end (resource lifecycle rules) */
    if (R.sink && R.sink->destroy_mesh) {
        if (R.deferred_mesh_count >= R.deferred_mesh_cap) {
            int cap = R.deferred_mesh_cap ? R.deferred_mesh_cap * 2 : 16;
            int *grown = realloc(R.deferred_mesh, (size_t)cap * sizeof(int));
            if (!grown) {
                return EFX_RENDER_ERR_NOMEM;
            }
            R.deferred_mesh = grown;
            R.deferred_mesh_cap = cap;
        }
        R.deferred_mesh[R.deferred_mesh_count++] = (int)(m - R.meshes);
    }
    if (out) {
        *out = m;
    }
    return EFX_RENDER_OK;
}

int efx_render_mesh_destroy(uint64_t h) {
    return mesh_release(h, NULL);
}

int efx_render_mesh_alive(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return m && m->alive;
}

int efx_render_mesh_surface_count(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return m && m->alive ? m->surface_count : -1;
}

void *efx_render_mesh_native(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return m ? m->native : NULL;
}

int efx_render_mesh_set_material(uint64_t h, int surface,
                                 const efx_material *mat, int has) {
    mesh_slot *m = mesh_get(h);
    if (!m || !m->alive) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (surface < 0 || surface >= m->surface_count) {
        return EFX_RENDER_ERR_INDEX;
    }
    material_release_maps(&m->materials[surface]);
    if (has && mat) {
        m->materials[surface] = *mat;
        m->has_material[surface] = 1;
        material_retain_maps(&m->materials[surface]);
    } else {
        efx_material_default(&m->materials[surface]);
        m->has_material[surface] = 0;
    }
    return EFX_RENDER_OK;
}

int efx_render_mesh_surface_material(uint64_t h, int surface,
                                     efx_material *out) {
    /* no alive check: playback may still reference a mesh destroyed earlier
     * in the same frame (native and materials release at frame end) */
    mesh_slot *m = mesh_get(h);
    if (!m || surface < 0 || surface >= m->surface_count) {
        if (out) {
            efx_material_default(out);
        }
        return -1;
    }
    if (out) {
        *out = m->materials[surface];
    }
    return m->has_material[surface] ? 1 : 0;
}

/* -------------------------------------------------------------- affine */

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

static int record_push(efx_record rec, size_t bytes) {
    if ((size_t)(R.record_count + 1) * bytes > EFX_RENDER_RECORD_BUDGET_BYTES) {
        return EFX_RENDER_ERR_BUDGET;
    }
    if (R.record_count >= R.record_cap) {
        int cap = R.record_cap ? R.record_cap * 2 : 256;
        efx_record *grown = realloc(R.records, (size_t)cap * sizeof(efx_record));
        if (!grown) {
            return EFX_RENDER_ERR_NOMEM;
        }
        R.records = grown;
        R.record_cap = cap;
    }
    R.records[R.record_count++] = rec;
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
    rec.target = R.active_target;
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
        int tw = 0, th = 0;
        efx_render_texture_size(texture, &tw, &th);
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
    for (int i = 0; i < 4; i++) {
        q->color[i] = color ? color[i] : 1.0f;
    }
    q->blend = (uint8_t)R.blend;
    /* sort key: record index — playback order equals record order in F2
       (design D3); the stable sort below generalizes when keys change */
    rec.sort_key = (uint32_t)R.record_count;

    return record_push(rec, sizeof(efx_record));
}

int efx_render_mesh(uint64_t mesh, const float transform[16],
                    const float color[4]) {
    ensure_state();
    if (!mesh || !efx_render_mesh_alive(mesh)) {
        return EFX_RENDER_ERR_HANDLE;
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
    rec.target = R.active_target;
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
    for (int i = 0; i < 4; i++) {
        mr->color[i] = color ? color[i] : 1.0f;
    }
    mr->camera = R.camera3d;
    mr->lights = R.lights;   /* value snapshot (F4a design D4) */
    mr->blend = (uint8_t)R.blend;
    rec.sort_key = (uint32_t)R.record_count;
    return record_push(rec, sizeof(efx_record));
}

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
                int cap = run_cap ? run_cap * 2 : 64;
                efx_draw_run *grown = realloc(runs, (size_t)cap * sizeof(efx_draw_run));
                if (!grown) {
                    if (count) *count = 0;
                    return NULL;
                }
                runs = grown;
                run_cap = cap;
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
    free(R.records);
    free(R.deferred_tex);
    free(R.deferred_mesh);
    free(R.deferred_rt);
    memset(&R, 0, sizeof(R));
    state_ready = 0;
}

/* ------------------------------------------------------- CPU lighting (F4a) */

static float lclampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void lnormalize3(float out[3], const float v[3]) {
    float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 0.0f) {
        out[0] = v[0] / len;
        out[1] = v[1] / len;
        out[2] = v[2] / len;
    } else {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
    }
}

/* one light's contribution (shares the exact formula with the shader;
 * channel maps scale the diffuse/specular colors, design D1/D8) */
static void lighting_term(const efx_material *mat, const efx_map_samples *maps,
                          const float contrib[3], float atten, const float N[3],
                          const float V[3], const float L[3],
                          const float albedo[3], float out[3]) {
    float ndl = N[0] * L[0] + N[1] * L[1] + N[2] * L[2];
    if (ndl < 0.0f) {
        ndl = 0.0f;
    }
    float H[3] = {L[0] + V[0], L[1] + V[1], L[2] + V[2]};
    float Hn[3];
    lnormalize3(Hn, H);
    float ndh = N[0] * Hn[0] + N[1] * Hn[1] + N[2] * Hn[2];
    if (ndh < 0.0f) {
        ndh = 0.0f;
    }
    float sp = powf(ndh, mat->shininess);
    float scale = atten;
    for (int c = 0; c < 3; c++) {
        float dc = mat->diffuse[c] * maps->diffuse[c];
        float sc = mat->specular[c] * maps->specular[c];
        float d = dc * albedo[c] * ndl;
        float s = sc * sp;
        out[c] += (d + s) * contrib[c] * scale;
    }
}

int efx_lighting_shade(const efx_material *mat, const efx_light_set *lights,
                       const float world_pos[3], const float normal[3],
                       const float camera_pos[3], const float albedo[4],
                       const efx_map_samples *maps, float out[4]) {
    efx_material def;
    efx_light_set empty;
    efx_map_samples neutral;
    if (!mat) {
        efx_material_default(&def);
        mat = &def;
    }
    if (!lights) {
        memset(&empty, 0, sizeof(empty));
        lights = &empty;
    }
    if (!maps) {
        for (int c = 0; c < 3; c++) {
            neutral.ambient[c] = 1.0f;
            neutral.diffuse[c] = 1.0f;
            neutral.specular[c] = 1.0f;
            neutral.emissive[c] = 1.0f;
        }
        neutral.mask_alpha = 1.0f;
        neutral.has_mask = 0;
        maps = &neutral;
    }
    if (maps->has_mask && maps->mask_alpha < 0.5f) {
        return 1; /* alpha-mask cutout (design D2) */
    }
    float N[3];
    lnormalize3(N, normal);
    float V[3] = {camera_pos[0] - world_pos[0],
                  camera_pos[1] - world_pos[1],
                  camera_pos[2] - world_pos[2]};
    lnormalize3(V, V);

    float col[3];
    for (int c = 0; c < 3; c++) {
        col[c] = mat->ambient[c] * maps->ambient[c] * albedo[c] +
                 mat->emissive[c] * maps->emissive[c];
    }
    for (int i = 0; i < EFX_MAX_POINT_LIGHTS; i++) {
        const efx_point_light *p = &lights->points[i];
        if (!p->enabled) {
            continue;
        }
        float toL[3] = {p->pos[0] - world_pos[0],
                        p->pos[1] - world_pos[1],
                        p->pos[2] - world_pos[2]};
        float d = sqrtf(toL[0] * toL[0] + toL[1] * toL[1] + toL[2] * toL[2]);
        float L[3];
        if (d > 0.0f) {
            L[0] = toL[0] / d;
            L[1] = toL[1] / d;
            L[2] = toL[2] / d;
        } else {
            L[0] = 0.0f; L[1] = 1.0f; L[2] = 0.0f;
        }
        float atten = p->range > 0.0f ? lclampf(1.0f - d / p->range, 0.0f, 1.0f)
                                      : 1.0f;
        lighting_term(mat, maps, p->color, atten, N, V, L, albedo, col);
    }
    if (lights->directional.enabled) {
        const efx_dir_light *dl = &lights->directional;
        float L[3] = {-dl->dir[0], -dl->dir[1], -dl->dir[2]};
        lnormalize3(L, L);
        lighting_term(mat, maps, dl->color, 1.0f, N, V, L, albedo, col);
    }
    out[0] = lclampf(col[0], 0.0f, 1.0f);
    out[1] = lclampf(col[1], 0.0f, 1.0f);
    out[2] = lclampf(col[2], 0.0f, 1.0f);
    out[3] = albedo[3];
    return 0;
}
