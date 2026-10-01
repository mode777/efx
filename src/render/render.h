#ifndef EFX_RENDER_H
#define EFX_RENDER_H

/*
 * Internal render module: display list (record -> sort -> playback),
 * 2D/3D camera state, texture and mesh registries, CPU mesh data.
 *
 * Pure C, no sokol, no quickjs (ADR 0003 module walls). GPU work goes
 * through a sink vtable installed by the platform side at startup.
 * Semantics per ADR 0019: frame-transient records, value-snapshot state,
 * handle-referenced resources, stable sort by explicit key.
 */

#include <stddef.h>
#include <stdint.h>

/* blend modes */
#define EFX_BLEND_ALPHA 0
#define EFX_BLEND_ADDITIVE 1
#define EFX_BLEND_SUBTRACTIVE 2

/* error codes */
#define EFX_RENDER_OK 0
#define EFX_RENDER_ERR_BUDGET 1
#define EFX_RENDER_ERR_HANDLE 2      /* unknown texture/mesh/target handle */
#define EFX_RENDER_ERR_PERMANENT 3   /* engine-owned resource */
#define EFX_RENDER_ERR_SINK 4        /* no sink installed */
#define EFX_RENDER_ERR_NOMEM 5
#define EFX_RENDER_ERR_INDEX 6       /* surface index out of range (F4a) */
#define EFX_RENDER_ERR_NESTED 7      /* beginRenderTarget while active (F5a) */
#define EFX_RENDER_ERR_STATE 8       /* endRenderTarget with none active (F5a) */
#define EFX_RENDER_ERR_FEEDBACK 9    /* draw samples the active target (F5a) */
#define EFX_RENDER_ERR_SIZE 10       /* render-target size out of range (F5a) */
#define EFX_RENDER_ERR_RIG 11        /* skinned draw/pose on a rig-less mesh (F7) */

/* fixed limit: render-target size per side (F5a, documented hard maximum) */
#define EFX_RENDER_MAX_TARGET_SIZE 4096

/* mesh data validation failures (range-level; type-level errors are
 * reported by the binding while extracting JS values) */
#define EFX_MESHERR_OK 0
#define EFX_MESHERR_COUNT 1    /* surface count outside 1..EFX_MESH_MAX_SURFACES */
#define EFX_MESHERR_LEN 2      /* array lengths: multiples / attribute mismatch */
#define EFX_MESHERR_INDEX 3    /* index out of vertex range */
#define EFX_MESHERR_NOMEM 4

/* fixed limit: surfaces per mesh (vision.md fixed limits, F3) */
#define EFX_MESH_MAX_SURFACES 16

/* hard per-frame record budget (design D4). Raised in F4a so the larger
 * mesh record (light snapshot, design D4) keeps the documented ~170k quad
 * capacity: the budget is charged per record as sizeof(efx_record), and the
 * mesh record's union member now sets that size. */
#define EFX_RENDER_RECORD_BUDGET_BYTES (64 * 1024 * 1024)

/* row-major 2D affine: x' = a*x + c*y + tx ; y' = b*x + d*y + ty */
typedef struct efx_affine {
    float a, b, c, d, tx, ty;
} efx_affine;

typedef struct efx_camera2d {
    float frame_w, frame_h; /* virtual frame in px; 0 => default (viewport) */
    float x, y;             /* world point displayed at the frame center */
    float zoom;             /* > 0 */
    float rotation;         /* degrees, clockwise in the y-down frame */
} efx_camera2d;

typedef struct efx_camera3d {
    float pos[3];
    float target[3];
    float fov;              /* vertical, degrees */
    float near_z, far_z;
} efx_camera3d;

/* Fixed light bank (vision.md limits, F4a design D4). Lights are plain
 * value state: a mesh record snapshots the whole set at record time
 * (ADR 0019). A light is disabled unless explicitly set. */
#define EFX_MAX_POINT_LIGHTS 4

typedef struct efx_point_light {
    int enabled;
    float pos[3];
    float color[4];   /* rgb used; alpha ignored */
    float range;      /* 0 = no attenuation */
} efx_point_light;

typedef struct efx_dir_light {
    int enabled;
    float dir[3];     /* direction the light travels */
    float color[4];   /* rgb used; alpha ignored */
} efx_dir_light;

typedef struct efx_light_set {
    efx_point_light points[EFX_MAX_POINT_LIGHTS];
    efx_dir_light directional;
} efx_light_set;

/* Per-surface Phong material snapshot (F4a design D5/D6, extended by F4b
 * design D5): plain values, JS-managed on the script side (no native handle,
 * no destroy). Colors/shininess are value-snapshotted; channel maps and the
 * alpha mask are handles (0 = absent) held by reference (ADR 0019) and
 * retained by the engine while bound (F4b design D6). From F5a a map handle
 * may reference a Texture or a RenderTarget (texture coercion). */
typedef struct efx_material {
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float emissive[4];
    float shininess;
    uint64_t ambient_map;
    uint64_t diffuse_map;
    uint64_t specular_map;
    uint64_t emissive_map;
    uint64_t alpha_mask;
} efx_material;

/* documented default material: white diffuse Phong, no maps */
void efx_material_default(efx_material *m);

/* F4b per-fragment map samples for the CPU lighting reference (design D8):
 * each channel RGB sample multiplies that channel's color; a present mask
 * discards the fragment when mask_alpha < 0.5. Passing NULL to
 * efx_lighting_shade reproduces the F4a neutral result. */
typedef struct efx_map_samples {
    float ambient[3];
    float diffuse[3];
    float specular[3];
    float emissive[3];
    float mask_alpha;
    int has_mask;
} efx_map_samples;

/* one quad (design D1/D3; ~96 bytes) */
typedef struct efx_quad_record {
    efx_affine m;          /* local -> frame, composed at record time */
    float frame_w, frame_h;
    float w, h;            /* local (destination) size in frame px */
    float sx, sy, sw, sh;  /* source rect in texels */
    float tw, th;          /* texture size in texels */
    float color[4];
    uint64_t texture;      /* texture handle; 0 = white texture */
    uint8_t blend;
} efx_quad_record;

/* one whole-mesh draw (design D7): every surface plays back in surface
 * order, depth-tested; the camera is value-snapshotted at record time */
typedef struct efx_mesh_record {
    uint64_t mesh;
    float transform[16];   /* column-major model matrix */
    float color[4];        /* tint */
    efx_camera3d camera;
    efx_light_set lights;  /* value snapshot at record time (F4a design D4) */
    uint8_t blend;
    uint8_t skinned;       /* F7: draw the posed buffer instead of bind pose */
} efx_mesh_record;

/* beginRenderTarget control record (F5a design D2): opens a segment; the
 * clear color is value-snapshotted at record time (every begin starts
 * from a cleared target) */
typedef struct efx_begin_target_record {
    uint64_t target;
    float clear[4];
} efx_begin_target_record;

/* one world-space billboard (F11): the quad basis is derived at playback from
 * the recorded camera and `facing`/`normal`, so only the placement is stored */
typedef struct efx_billboard_record {
    float pos[3];
    float w, h;            /* world-unit destination size */
    float sx, sy, sw, sh;  /* source rect in texels */
    float tw, th;          /* sampled texture size in texels */
    float color[4];
    float rotation;        /* degrees in the quad plane */
    float normal[3];       /* `plane` orientation */
    uint64_t texture;
    uint8_t facing;
    uint8_t depth_test;
    uint8_t blend;
    efx_camera3d camera;   /* value snapshot at record time */
} efx_billboard_record;

/* one particle batch (F11): references a live system at playback, which
 * resolves and depth-sorts its own particles */
typedef struct efx_particle_record {
    uint64_t system;
    uint8_t blend;
    efx_camera3d camera;    /* world-space systems */
    efx_camera2d camera2d;  /* screen-space systems */
    float frame_w, frame_h; /* screen-space frame */
} efx_particle_record;

#define EFX_RECORD_QUAD 0
#define EFX_RECORD_MESH 1
#define EFX_RECORD_BEGIN_TARGET 2
#define EFX_RECORD_END_TARGET 3
#define EFX_RECORD_BILLBOARD 4
#define EFX_RECORD_PARTICLES 5

/* one display-list record; sort key = record index (F2: playback order
 * equals record order, design D3). `target` is the rendering surface the
 * record belongs to (0 = default target; F5a segmentation: the renderer's
 * reordering freedom stops at segment boundaries). */
typedef struct efx_record {
    uint8_t type;
    uint32_t sort_key;
    uint64_t target;
    union {
        efx_quad_record quad;
        efx_mesh_record mesh;
        efx_begin_target_record begin_target;
        efx_billboard_record billboard;
        efx_particle_record particles;
    } u;
} efx_record;

/* one batched quad playback run: consecutive quad records sharing
 * texture + blend (mesh records break runs; design D10) */
typedef struct efx_draw_run {
    int start;      /* index of first record */
    int count;      /* number of records in the run */
    uint64_t texture;
    uint8_t blend;
} efx_draw_run;

/* four influences per vertex (glTF JOINTS_0 / WEIGHTS_0), F6c */
#define EFX_JOINTS_PER_VERTEX 4
#define EFX_WEIGHTS_PER_VERTEX 4

/* ---------------------------------------------------- F6c rig payload */

/* animation channel target path (glTF node TRS; morph weights are skipped) */
#define EFX_ANIM_PATH_TRANSLATION 0
#define EFX_ANIM_PATH_ROTATION 1
#define EFX_ANIM_PATH_SCALE 2

/* sampler interpolation; CUBICSPLINE imports as LINEAR with tangents dropped */
#define EFX_ANIM_INTERP_LINEAR 0
#define EFX_ANIM_INTERP_STEP 1

/* one animation channel with its sampled keyframes inlined (opaque payload;
 * CPU-only, never script-visible). `values` holds times_len * components
 * floats; translation/scale are 3 components, rotation 4 (xyzw). */
typedef struct efx_anim_channel {
    int target_node;    /* glTF node index this channel animates */
    int path;           /* EFX_ANIM_PATH_* */
    int interpolation;  /* EFX_ANIM_INTERP_* */
    int components;     /* 3 (TRS position/scale) or 4 (rotation) */
    int times_len;      /* keyframe count */
    int values_len;     /* times_len * components */
    float *times;       /* seconds */
    float *values;
} efx_anim_channel;

/* one glTF animation clip: a name (stable index-based name when unnamed) and
 * its channels (F6c design D5). Playback resolution is F7's concern. */
typedef struct efx_animation_clip {
    char *name;
    int channel_count;
    efx_anim_channel *channels;
} efx_animation_clip;

/* opaque rig payload: the skin's joint hierarchy + inverse bind matrices and
 * the asset's animation clips, carried MeshData -> Mesh (design D2/D3). */
typedef struct efx_rig {
    int joint_count;
    int *joint_nodes;    /* glTF node index per joint */
    int *joint_parents;  /* parent joint index, -1 for a root joint */
    float *inverse_bind; /* joint_count * 16, column-major (identity-filled) */
    int clip_count;
    efx_animation_clip *clips;
} efx_rig;

/* one resolved pose sample (F7): a clip index plus a time in seconds and an
 * optional blend weight. The binding resolves clip names/indices to the index
 * before calling the render module. */
typedef struct efx_pose_sample {
    int clip;
    float time;
    float weight;
} efx_pose_sample;

/* CPU mesh data: 1..EFX_MESH_MAX_SURFACES surfaces, each with its own
 * attribute arrays + optional indices (Godot surface / glTF primitive).
 * Storage is deep-copied and engine-owned (design D8). */
typedef struct efx_surface_src {
    int positions_len;   /* floats, %3 == 0, > 0 */
    int normals_len;     /* floats, %3 == 0, 0 = absent */
    int uvs_len;         /* floats, %2 == 0, 0 = absent */
    int colors_len;      /* floats, %4 == 0, 0 = absent */
    int joints_len;      /* uint32 count, 0 or vertex_count * 4 (F6c) */
    int weights_len;     /* float count, 0 or vertex_count * 4 (F6c) */
    int indices_len;     /* uint32 count, %3 == 0, 0 = non-indexed */
    const float *positions, *normals, *uvs, *colors, *weights;
    const uint32_t *joints;
    const uint32_t *indices;
} efx_surface_src;

typedef struct efx_surface {
    int vertex_count, index_count;
    float *positions, *normals, *uvs, *colors; /* NULL when absent */
    uint32_t *joints;                          /* NULL when static (F6c) */
    float *weights;                            /* NULL when static (F6c) */
    uint32_t *indices;                          /* NULL when non-indexed */
    int has_material;                           /* F4a: explicit binding */
    efx_material material;
} efx_surface;

typedef struct efx_meshdata {
    efx_surface *surfaces;
    int surface_count;
    efx_rig *rig;   /* NULL when the asset carries no skin/clips (F6c) */
} efx_meshdata;

/* validates + deep-copies; NULL + one of the EFX_MESHERR_* codes */
efx_meshdata *efx_meshdata_create(const efx_surface_src *src, int count,
                                  int *err);
void efx_meshdata_destroy(efx_meshdata *md); /* idempotent, NULL safe */

/* per-surface material binding on CPU MeshData (copied at createMesh);
 * has=0 clears the binding (engine default) */
void efx_meshdata_set_material(efx_meshdata *md, int index,
                               const efx_material *mat, int has);

/* rig payload (F6c): takes ownership of `rig` and releases any previous one;
 * NULL clears. free releases one. */
void efx_meshdata_set_rig(efx_meshdata *md, efx_rig *rig);
void efx_rig_free(efx_rig *rig);

/* one GPU surface as handed to the sink: vertices interleaved
 * pos(3f) normal(3f) uv(2f) color(4f) = 12 floats/vertex (design D1;
 * absent attributes are filled with deterministic defaults) */
typedef struct efx_mesh_gpu_surface {
    int vertex_count, index_count;
    const float *interleaved;
    const uint32_t *indices; /* NULL when non-indexed */
    int skinned;             /* F7: allocate a second posed vertex buffer */
} efx_mesh_gpu_surface;

/* GPU sink, implemented on the platform (sokol) side. create_mesh may
 * return NULL on failure; native handles are owned by the sink side.
 * create_render_target/destroy_render_target (F5a) manage offscreen
 * color+depth attachment pairs; the native value is also the sampling
 * source when a target is used as a texture. */
typedef struct efx_render_sink {
    void *ud;
    void *(*create_texture)(void *ud, int w, int h, const uint8_t *rgba,
                            int wrap, int filter, int mipmaps);
    void (*destroy_texture)(void *ud, void *native);
    void *(*create_mesh)(void *ud, const efx_mesh_gpu_surface *surfaces,
                         int count);
    void (*destroy_mesh)(void *ud, void *native);
    void (*shutdown)(void *ud);
    void *(*create_render_target)(void *ud, int w, int h);
    void (*destroy_render_target)(void *ud, void *native);
} efx_render_sink;

/* lifecycle; installing the sink flushes any uploads queued before a GPU
   surface existed (top-level createTexture/createMesh in main.js) */
void efx_render_install_sink(const efx_render_sink *sink);
void efx_render_shutdown(void);
void efx_render_set_viewport(int w, int h);
void efx_render_viewport(int *out_w, int *out_h);

/* state setters (value-snapshot: recorded draws never observe later changes) */
void efx_render_reset_state(void); /* blend alpha, clear black, default cameras */
void efx_render_set_camera(const efx_camera2d *cam);
void efx_render_set_camera3d(const efx_camera3d *cam);
void efx_render_set_clear_color(const float rgba[4]);
int efx_render_set_blend(int mode);
void efx_render_clear_color(float out_rgba[4]);
void efx_render_camera3d(float out_pos[3], float out_target[3], float *out_fov,
                         float *out_near, float *out_far);

/* fixed light bank (F4a design D4); NULL disables the slot / the single
 * directional light. Slot outside 0..EFX_MAX_POINT_LIGHTS-1 is ignored. */
void efx_render_set_point_light(int slot, const efx_point_light *light);
void efx_render_set_directional_light(const efx_dir_light *light);
void efx_render_lights(efx_light_set *out);

/* texture sampler options (F6b). Wrap: repeat (default) / clamp / mirror.
 * Filter names reuse EFX_FILTER_NEAREST / EFX_FILTER_LINEAR. Out-of-range
 * values fall back to the defaults rather than failing. */
#define EFX_TEX_WRAP_REPEAT 0
#define EFX_TEX_WRAP_CLAMP 1
#define EFX_TEX_WRAP_MIRROR 2

/* textures; handles are opaque, 0 = invalid. wrap/filter/mipmaps are immutable
 * creation state (sokol binds a sampler to the image). */
uint64_t efx_render_texture_create(int w, int h, const uint8_t *rgba, int wrap,
                                  int filter, int mipmaps);
int efx_render_texture_destroy(uint64_t h); /* deferred to frame end */
int efx_render_texture_alive(uint64_t h);
void efx_render_texture_size(uint64_t handle, int *out_w, int *out_h);
/* resolved sampler of a texture (F6b); out params optional, -1 on bad handle */
void efx_render_texture_sampler(uint64_t handle, int *out_wrap, int *out_filter,
                                int *out_mipmaps);
void *efx_render_texture_native(uint64_t h); /* valid until end of frame */
uint64_t efx_render_white_texture(void);
/* F4b material-map retention count (design D6); -1 on a bad handle. Exposed
 * for the headless unit tests that assert bindings retain/release textures. */
int efx_render_texture_ref_count(uint64_t h);

/* meshes; handles are opaque, 0 = invalid; destroy is deferred to frame
 * end (records may reference the mesh until playback finishes) */
uint64_t efx_render_mesh_create(const efx_meshdata *md);
int efx_render_mesh_destroy(uint64_t h);
int efx_render_mesh_alive(uint64_t h);
int efx_render_mesh_surface_count(uint64_t h);
void *efx_render_mesh_native(uint64_t h);

/* F12: exports a live mesh's triangles for a physics static-mesh collider.
 * geometry_count returns the total vertices (out_verts) and triangle indices
 * (out_indices, multiple of 3) across all surfaces; geometry copies positions
 * (3 floats/vertex) and triangle indices into caller buffers sized from the
 * counts. Returns 1 on success, 0 for a dead/empty handle. */
int efx_render_mesh_geometry_count(uint64_t h, int *out_verts,
                                   int *out_indices);
int efx_render_mesh_geometry(uint64_t h, float *positions, uint32_t *indices);
/* test/introspection: the rig carried by a live Mesh (NULL when static;
 * ownership stays with the mesh). Not script-visible (F6c design D6). */
const efx_rig *efx_render_mesh_rig(uint64_t h);

/* F7 posing: 1 when the live Mesh carries a rig (and thus owns posed CPU
 * buffers); 0 for static/unknown handles. */
int efx_render_mesh_skinned(uint64_t h);
/* resolve a clip name to its index; -1 when absent (bad mesh or name). */
int efx_render_mesh_find_clip(uint64_t h, const char *name);
/* apply a weighted pose to a live skinned Mesh in place (bind data is never
 * touched). Returns EFX_RENDER_OK / ERR_HANDLE (bad or rig-less mesh) /
 * ERR_INDEX (clip index out of range). */
int efx_render_mesh_pose(uint64_t h, const efx_pose_sample *samples, int count);
/* posed interleaved CPU vertices for a surface (12 floats/vertex) and the
 * monotonic pose revision used to skip redundant GPU uploads; NULL/0 when the
 * mesh is static or the surface is out of range. */
const float *efx_render_mesh_posed(uint64_t h, int surface);
uint32_t efx_render_mesh_pose_revision(uint64_t h);

/* render targets (F5a); handles are opaque, 0 = invalid; destroy is
 * deferred to frame end like textures (records and bound maps may hold
 * the target until playback/binding release) */
uint64_t efx_render_target_create(int w, int h);
int efx_render_target_destroy(uint64_t h); /* deferred to frame end */
int efx_render_target_alive(uint64_t h);
void efx_render_target_size(uint64_t h, int *out_w, int *out_h);
void *efx_render_target_native(uint64_t h); /* valid until end of frame */
int efx_render_target_ref_count(uint64_t h);

/* render redirection (F5a): records BEGIN/END control records and moves
 * the active target. Returns EFX_RENDER_OK, or ERR_HANDLE (not a live
 * target), ERR_NESTED (a begin is active), ERR_STATE (end without begin),
 * ERR_BUDGET, ERR_NOMEM. */
uint64_t efx_render_active_target(void);
int efx_render_begin_target(uint64_t h);
int efx_render_end_target(void);

/* unified sampling lookup (F5a texture coercion): a live Texture or a
 * live RenderTarget. *_sample_* resolve either registry; 0 handles and
 * unknown handles report not-alive. */
int efx_render_sample_alive(uint64_t h);
void efx_render_sample_size(uint64_t h, int *out_w, int *out_h);
void *efx_render_sample_native(uint64_t h);
/* active rendering surface size (default target or active render target);
 * backs the default 2D camera frame and the 3D projection aspect (F5a).
 * With an active post chain or a render scale != 1 the default surface is
 * the implicit scene target, so this reports the scaled scene size (F5b). */
void efx_render_surface_size(int *out_w, int *out_h);

/* per-surface material binding on a live Mesh (F4a); has=0 restores the
 * default material. Returns EFX_RENDER_OK / EFX_RENDER_ERR_HANDLE /
 * EFX_RENDER_ERR_INDEX. */
int efx_render_mesh_set_material(uint64_t h, int surface,
                                 const efx_material *mat, int has);
/* fills *out with the surface's material (or the default) and returns 1
 * when a material is explicitly bound, 0 for the default; -1 on bad mesh */
int efx_render_mesh_surface_material(uint64_t h, int surface,
                                     efx_material *out);

/* ------------------------------------------------------- F5b post effects */

/* fixed limit: post-effect chain entries per frame (vision.md fixed limits) */
#define EFX_POST_MAX_ENTRIES 8

/* registered effects (F5b v1) */
#define EFX_POST_COLOR_FILTER 0
#define EFX_POST_BLUR 1
#define EFX_POST_BLOOM 2

/* render-scale blit filter */
#define EFX_FILTER_NEAREST 0
#define EFX_FILTER_LINEAR 1

/* native validation results; the binding maps these to JS exception types
 * (unknown effect/filter -> TypeError, count/range -> RangeError) */
#define EFX_POST_OK 0
#define EFX_POST_ERR_UNKNOWN 1
#define EFX_POST_ERR_COUNT 2
#define EFX_POST_ERR_RANGE 3
#define EFX_POST_ERR_FILTER 4

/* one post-effect chain entry: plain values, JS-managed on the script side
 * (no native handle, no destroy). The option union carries the registered
 * effect's pinned fields; `mix` (0..1, default 1) lerps input to output. */
typedef struct efx_post_entry {
    int effect;
    float mix;
    union {
        struct {
            float brightness, contrast, saturation;
            float tint[4];
        } color_filter;
        struct {
            float radius; /* scene pixels, > 0 and <= 64 */
        } blur;
        struct {
            float threshold; /* luminance cut 0..1 */
            float strength;  /* additive contribution 0..1 */
        } bloom;
    } u;
} efx_post_entry;

/* validate + store the chain (value snapshot; count 0 clears). Validates
 * every field against the registered effect's bounds: unknown effect ->
 * ERR_UNKNOWN, count outside 0..EFX_POST_MAX_ENTRIES -> ERR_COUNT, an
 * out-of-range option -> ERR_RANGE. On failure the previous chain remains. */
int efx_render_set_post_effects(const efx_post_entry *entries, int count);
/* current chain (out may be NULL; count optional) */
void efx_render_post_effects(efx_post_entry *out, int *count);
/* 1 when the frame needs the implicit scene target (chain set or scale != 1) */
int efx_render_post_active(void);

/* render scale: scene resolution / surface size, finite in (0, 2]; filter
 * EFX_FILTER_NEAREST / EFX_FILTER_LINEAR. Returns EFX_POST_OK /
 * EFX_POST_ERR_RANGE / EFX_POST_ERR_FILTER; on failure state is unchanged. */
int efx_render_set_render_scale(float scale, int filter);
void efx_render_render_scale(float *out_scale, int *out_filter);

/* engine-owned implicit scene target and ping-pong temporaries (F5b). Never
 * script-visible: no handles cross the binding, no class entry. Sizes are
 * reallocated lazily; the returned handle is 0 on failure. Full-size temp
 * slots 0/1 are the entry ping-pong outputs, 2/3 the blur tap internals. */
uint64_t efx_render_post_scene_target(int w, int h);
uint64_t efx_render_post_temp_target(int slot, int w, int h);
/* half-size temporaries for the blur/bloom downsample chain (slots 0/1) */
uint64_t efx_render_post_half_target(int slot, int w, int h);
/* test/introspection: the allocated scene target handle (0 = fast path) */
uint64_t efx_render_post_scene_handle(void);

/* recording */
int efx_render_quad(float x, float y, float w, float h, uint64_t texture,
                    const float color[4], float rotation_deg, float scale,
                    const float src_rect[4], int has_src,
                    float origin_x, float origin_y);
int efx_render_mesh(uint64_t mesh, const float transform[16],
                    const float color[4], int skinned);
const efx_record *efx_render_records(int *count);
const efx_draw_run *efx_render_runs(int *count); /* batched quad plan */

/* frame boundaries */
void efx_render_begin_frame(void); /* rewind record arena */
void efx_render_end_frame(void);   /* flush deferred destroys */

/* affine/math helpers (exposed for unit tests) */
efx_affine efx_affine_mul(efx_affine f, efx_affine g); /* f(g(p)) */
efx_affine efx_camera_matrix(const efx_camera2d *cam, float fw, float fh);
efx_affine efx_quad_matrix(float x, float y,
                           float origin_x, float origin_y,
                           float rotation_deg, float scale);

/* CPU reference implementation of the lighting equation (F4a design D8,
 * extended by F4b design D8). `normal` may be non-unit (normalized
 * internally); `albedo` is the per-fragment vertex color × tint; `maps` is
 * the optional F4b map-sample set (NULL = neutral). Returns 1 when the
 * fragment is discarded by the alpha mask, else 0; out[4] receives the
 * per-channel clamped lit color with out[3] = albedo[3]. */
int efx_lighting_shade(const efx_material *mat, const efx_light_set *lights,
                       const float world_pos[3], const float normal[3],
                       const float camera_pos[3], const float albedo[4],
                       const efx_map_samples *maps, float out[4]);

/* ------------------------------------------------- F11 billboards + particles */

/* billboard / particle quad facing modes */
#define EFX_FACING_VIEW 0
#define EFX_FACING_Y 1
#define EFX_FACING_PLANE 2

/* particle coordinate space */
#define EFX_SPACE_WORLD 0
#define EFX_SPACE_SCREEN 1

/* particle emission shape (point, box, sphere volume, sphere surface, disc) */
#define EFX_SHAPE_POINT 0
#define EFX_SHAPE_BOX 1
#define EFX_SHAPE_SPHERE 2
#define EFX_SHAPE_SPHERE_SURFACE 3
#define EFX_SHAPE_DISC 4

/* particle insertion order for new particles */
#define EFX_INSERT_TOP 0
#define EFX_INSERT_BOTTOM 1
#define EFX_INSERT_RANDOM 2

/* documented per-system live-particle capacity (hard cap) */
#define EFX_PARTICLES_MAX 65536

/* particle creation/reconfiguration config: plain value state snapshotted by
 * the engine. `sizes`/`colors` are lifetime-interpolated samples (<= 8);
 * `quads` are atlas rects in texels ({x,y,w,h}) selected over the lifetime.
 * `direction` need not be unit length (the engine normalizes it); `spread`
 * is the random cone half-angle in degrees. Vectors are 3 components even for
 * screen space (z ignored). */
typedef struct efx_particle_config {
    uint64_t texture;
    int max;
    int space;           /* EFX_SPACE_* */
    int facing;          /* EFX_FACING_* (world only) */
    int blend;           /* EFX_BLEND_* */
    float normal[3];     /* world `plane` orientation */

    float life_min, life_max;    /* particle lifetime seconds */
    float emission_rate;         /* particles/second */
    float emitter_lifetime;      /* seconds; -1 = infinite */
    float position[3];
    float direction[3];
    float spread;
    float speed_min, speed_max;

    float gravity[3];
    float lin_acc_min[3], lin_acc_max[3];
    float radial_acc_min, radial_acc_max;
    float tangential_acc_min, tangential_acc_max;
    float damping_min, damping_max;

    int size_count;              /* 1..8 */
    float sizes[8];
    float size_variation;        /* 0..1 */
    int color_count;             /* 1..8 */
    float colors[8][4];
    float rotation_min, rotation_max; /* degrees */
    float spin_start, spin_end;       /* degrees/second */
    float spin_variation;             /* 0..1 */
    int relative_rotation;
    int shape;                   /* EFX_SHAPE_* */
    float shape_size[3];         /* box half-extents / radii */
    int quad_count;              /* 0 or 1..64 */
    float quads[64][4];
    int insert_mode;             /* EFX_INSERT_* */
    float speed_scale;           /* > 0, simulated-time factor */
} efx_particle_config;

/* one resolved particle as handed to playback (computed at draw time) */
typedef struct efx_particle_view {
    float pos[3];
    float size;
    float angle;    /* degrees, quad plane roll */
    float color[4];
    float uv[4];    /* normalized atlas rect (0,0,1,1 when no quads) */
} efx_particle_view;

uint64_t efx_render_particles_create(const efx_particle_config *cfg, int *err);
int efx_render_particles_destroy(uint64_t h);
int efx_render_particles_count(uint64_t h);
int efx_render_particles_emit(uint64_t h, int n);
void efx_render_particles_start(uint64_t h);
void efx_render_particles_stop(uint64_t h);
void efx_render_particles_pause(uint64_t h);
void efx_render_particles_reset(uint64_t h);
/* full reconfigure (the binding merges a partial opts over the current config
 * via efx_render_particles_config first). Validates; unchanged on failure. */
int efx_render_particles_set(uint64_t h, const efx_particle_config *cfg);
void efx_render_particles_config(uint64_t h, efx_particle_config *out);
float efx_render_particles_speed_scale(uint64_t h);
void efx_render_particles_set_speed_scale(uint64_t h, float s);
/* advance every live system by dt (engine frame step) */
void efx_render_particles_step(float dt);
/* resolved views of the live particles (valid until the next step); NULL/0
 * for an unknown system. */
const efx_particle_view *efx_render_particles_views(uint64_t h, int *count);
int efx_render_particles_space(uint64_t h);
int efx_render_particles_facing(uint64_t h);
uint64_t efx_render_particles_texture(uint64_t h);
void efx_render_particles_normal(uint64_t h, float out[3]);

/* record one world-space billboard (F11) */
int efx_render_billboard(uint64_t texture, const float pos[3], float w, float h,
                         const float color[4], float rotation, int facing,
                         const float normal[3], int depth_test,
                         const float src_rect[4], int has_src);
/* record one particle batch for a live system (F11) */
int efx_render_particles_draw(uint64_t h);

/* billboard/oriented-quad basis (exposed for unit tests): fills unit vectors
 * right/up from the recorded camera and, for `plane`, the normal. */
void efx_render_billboard_basis(const efx_camera3d *cam, int facing,
                                const float normal[3], float right[3],
                                float up[3]);

#endif
