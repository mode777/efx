/*
 * Private contract shared by the src/render/render_*.c fragments (P14). The
 * public surface is render/render.h; this header holds the engine-owned
 * registries (`R`, `POST`), the slot types, the pool helpers, and the handful
 * of helpers that cross fragment boundaries.
 */
#ifndef EFX_RENDER_INTERNAL_H
#define EFX_RENDER_INTERNAL_H

#include "render/render.h"
#include "render/skin.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG2RAD (float)(M_PI / 180.0)

/* ---------------------------------------------------------------- state */

typedef struct {
    int used;
    int alive;
    int permanent;
    int bind_refs;       /* F4b: material map retain count (design D6) */
    int release_pending; /* F4b: destroy() called while retained */
    uint32_t gen;
    int w, h;
    int wrap, filter; /* F6b sampler (immutable creation state) */
    int mipmaps;      /* F6e: build + use a mip chain (immutable) */
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
    efx_rig *rig;             /* owned deep copy of MeshData rig (F6c) */
    int skinned;              /* F7: rig present => posed buffers exist */
    float **posed;            /* surface_count interleaved posed arrays (F7) */
    int *vert_count;          /* surface_count CPU vertex counts (F7) */
    uint32_t **joints;        /* surface_count influence indices (F7) */
    float **weights;          /* surface_count influence weights (F7) */
    uint32_t pose_revision;   /* bumped on each successful pose (F7) */
} mesh_slot;

/* F11 particle instance (engine pool entry) */
typedef struct {
    float pos[3];
    float vel[3];
    float origin[3];
    float life, lifetime;
    float lin_acc[3];
    float radial_acc, tangential_acc, damping;
    float size_scale;   /* sizeVariation factor chosen at emission */
    float rotation;     /* accumulated degrees */
    float spin_start, spin_end;
    int quad;           /* current atlas frame index */
} efx_particle;

/* F11 particle system: engine-owned pool + configuration snapshot. The
 * texture is retained while the system is alive (F4b retention mechanism). */
typedef struct {
    int used;
    int alive;
    uint32_t gen;
    efx_particle_config cfg;
    float tw, th;        /* sampled texture size (uv normalization) */
    int active;          /* emitter running */
    int paused;
    float emit_counter;
    float emitter_life;  /* remaining emitter seconds, or -1 = infinite */
    int count;
    efx_particle *parts;
    efx_particle_view *views;
    uint32_t rng;
} ps_slot;

typedef struct {
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
    ps_slot *ps;              /* F11 particle systems */
    int ps_count, ps_cap;
    uint64_t white_handle;
    efx_record *records;
    int record_count, record_cap;
    /* deferred texture/mesh/target/material destroys (slot indexes) */
    int *deferred_tex;
    int deferred_tex_count, deferred_tex_cap;
    int *deferred_mesh;
    int deferred_mesh_count, deferred_mesh_cap;
    int *deferred_rt;
    int deferred_rt_count, deferred_rt_cap;
    int *deferred_ps;         /* F11 */
    int deferred_ps_count, deferred_ps_cap;
} render_state;

extern render_state R;

/* F5b post-processing state: the declarative chain (plain value state,
 * applied at frame resolve), the render scale, and the engine-owned implicit
 * scene target + ping-pong temporaries. None of the target handles ever
 * cross the binding (no script-visible resource). */
typedef struct {
    efx_post_entry entries[EFX_POST_MAX_ENTRIES];
    int count;
    float scale;
    int filter;
    uint64_t scene;
    int scene_w, scene_h;
    uint64_t temps[4];   /* full-size temps: 0/1 entry ping-pong, 2/3 blur internals */
    int temp_w[4], temp_h[4];
    uint64_t half[2];    /* half-size temporaries (downsample chain) */
    int half_w[2], half_h[2];
} render_post_state;

extern render_post_state POST;

/* --------------------------------------------------------- pool helpers */

/* grow a pool to hold at least `need` elements, doubling from `initial`
 * (P11). Returns 1 on success, 0 on OOM; `*arr`/`*cap` update only on
 * success, so each caller keeps its own OOM branch. */
static inline int pool_grow(void **arr, int *cap, int need, size_t elem,
                            int initial) {
    if (need <= *cap) {
        return 1;
    }
    int next = *cap > 0 ? *cap : initial;
    while (next < need) {
        next *= 2;
    }
    void *grown = realloc(*arr, (size_t)next * elem);
    if (!grown) {
        return 0;
    }
    *arr = grown;
    *cap = next;
    return 1;
}

/* decode a pool handle `gen << 32 | idx` (P11). `tag` is the 4-bit namespace
 * in the index half (0 for textures, 1 for render targets); pass
 * EFX_HANDLE_NO_TAG to skip the tag check (meshes/particles, legacy). Returns
 * 0 when the tag does not match, else writes the 1-based index and generation. */
#define EFX_HANDLE_NO_TAG 0xFFFFFFFFu
static inline int handle_decode(uint64_t h, uint32_t tag, uint32_t *out_idx,
                                uint32_t *out_gen) {
    if (tag != EFX_HANDLE_NO_TAG && (h & 0xF0000000ull) != (uint64_t)tag) {
        return 0;
    }
    uint32_t mask = (tag == 0x10000000u) ? 0x0FFFFFFFu : 0xFFFFFFFFu;
    *out_idx = (uint32_t)(h & mask);
    *out_gen = (uint32_t)(h >> 32);
    return 1;
}

static inline tex_slot *slot_get(uint64_t h) {
    uint32_t idx = 0, gen = 0;
    if (!handle_decode(h, 0, &idx, &gen)) {
        return NULL; /* render-target tag: never a texture handle */
    }
    if (idx == 0 || (size_t)idx > (size_t)R.slot_count) {
        return NULL;
    }
    tex_slot *s = &R.slots[idx - 1];
    if (!s->used || s->gen != gen) {
        return NULL;
    }
    return s;
}

static inline mesh_slot *mesh_get(uint64_t h) {
    uint32_t idx = 0, gen = 0;
    handle_decode(h, EFX_HANDLE_NO_TAG, &idx, &gen);
    if (idx == 0 || (size_t)idx > (size_t)R.mesh_count) {
        return NULL;
    }
    mesh_slot *m = &R.meshes[idx - 1];
    if (!m->used || m->gen != gen) {
        return NULL;
    }
    return m;
}

static inline rt_slot *rt_get(uint64_t h) {
    /* F5a handle namespaces are disjoint: render-target handles carry tag
     * bits 28..31 (value 1) in the index half, keeping every handle small
     * enough to stay exact through the web bridge's double wire format */
    uint32_t idx = 0, gen = 0;
    if (!handle_decode(h, 0x10000000u, &idx, &gen)) {
        return NULL;
    }
    if (idx == 0 || (size_t)idx > (size_t)R.target_count) {
        return NULL;
    }
    rt_slot *t = &R.targets[idx - 1];
    if (!t->used || t->gen != gen) {
        return NULL;
    }
    return t;
}

static inline ps_slot *ps_get(uint64_t h) {
    uint32_t idx = 0, gen = 0;
    handle_decode(h, EFX_HANDLE_NO_TAG, &idx, &gen);
    if (idx == 0 || (size_t)idx > (size_t)R.ps_count) {
        return NULL;
    }
    ps_slot *p = &R.ps[idx - 1];
    if (!p->used || p->gen != gen) {
        return NULL;
    }
    return p;
}

/* ------------------------------------------- cross-fragment declarations */

void ensure_state(void);
void flush_pending_uploads(void);
void post_reset(void);
void post_clear(void);
int post_scene_size(int surface);
void texture_bind_retain(uint64_t h);
void texture_bind_release(uint64_t h);
void material_retain_maps(const efx_material *m);
void material_release_maps(const efx_material *m);
void finalize_target_release(rt_slot *t);
int record_push(efx_record rec, size_t bytes);
void pending_free(mesh_pending *p);
void mesh_materials_free(mesh_slot *m);
void ps_free_pool(ps_slot *p);

#endif
