#ifndef EFX_API_INTERNAL_H
#define EFX_API_INTERNAL_H

#include "api/api.h"
#include "runtime/runtime.h"
#include "runtime/runtime_internal.h"
#include "audio/audio.h"
#include "input/input.h"
#include "input/gamepad.h"
#include "physics/physics.h"
#include "physics/broadphase.h"
#include "render/render.h"
#include "render/text.h"
#include "resource/gltf.h"
#include "resource/image.h"
#include "resource/resource.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* wrapper structs */

/* shared numeric element loop for the array readers. `policy` names the
 * per-reader rule so the differences (coerce vs strict, integer range, error
 * class) live in one place instead of five copies. Returns 0, or -1 with the
 * error already thrown on the context. */
typedef enum {
    EFX_ELEM_COERCE, /* JS_ToFloat64; non-finite -> RangeError(msg_finite) */
    EFX_ELEM_NUMBER, /* non-number -> TypeError(msg_numbers) */
    EFX_ELEM_U32,    /* non-number -> TypeError; non-u32 -> RangeError(msg_int) */
} efx_elem_policy;

/* ------------------------------------------------- resource classes */

typedef struct {
    uint64_t handle;
    int alive;
    int permanent; /* engine-owned (white texture) */
} efxjs_texture;

typedef struct {
    uint8_t *pixels;
    int w, h;
    int alive;
} efxjs_imagedata;

typedef struct {
    efx_meshdata *md;
    int alive;
} efxjs_meshdata;

typedef struct efxjs_mesh {
    uint64_t handle;
    int alive;
} efxjs_mesh;

typedef struct {
    uint64_t handle;
    int alive;
} efxjs_rendertarget;

typedef struct {
    efx_text_fontdata *fd;
    int alive;
} efxjs_fontdata;

typedef struct {
    efx_text_font *font;
    int alive;
} efxjs_font;

typedef struct {
    uint64_t handle;
    int alive;
} efxjs_particlesystem;

/* ------------------------------------------------ F14 audio classes */

typedef struct {
    efx_audio_data *data;
    int alive;
} efxjs_audiodata;

typedef struct {
    efx_audio_stream *stream;
    int alive;
} efxjs_audiostream;

typedef struct {
    int voice;        /* core voice id, -1 once stopped */
    long long serial; /* core voice serial at start (steal detection) */
    int alive;
    int loop;
    float volume, pan, pitch;
} efxjs_audio;

/* ------------------------------------------------ F12 physics classes */

/* `self` is an owned reference while `pinned` (the collider is in the world,
 * so the world keeps its wrapper alive), otherwise borrowed (valid while this
 * struct is linked) */
typedef struct efxjs_body {
    efx_physics_world *w;
    efx_phys_body handle;
    int alive;
    int pinned;
    JSValue self;
    struct efxjs_body *next;
    struct efx_host_state *host;
} efxjs_body;

typedef struct efxjs_character {
    efx_physics_world *w;
    efx_phys_character handle;
    int alive;
    int pinned;
    JSValue self;
    struct efxjs_character *next;
    struct efx_host_state *host;
} efxjs_character;

/* the parsed shape carries the source live Mesh for a mesh collider so the
 * caller can build the triangle data where it is needed */
typedef struct parsed_shape {
    efx_shape shape;
    efxjs_mesh *mesh_src;
} parsed_shape;


/* one row per script-facing class: class-id allocation, class registration,
 * prototype wiring, destroy() and finalization are all driven by this table */
typedef struct {
    JSClassID *id;
    const char *name;
    const JSCFunctionListEntry *funcs;
    int nfuncs;
    /* script destroy(); NULL when the class defines its own (Body/Character) */
    JSValue (*destroy)(JSContext *ctx, void *p);
    /* GC finalizer step before the wrapper is freed; may be NULL */
    void (*release)(void *p);
} efx_class_spec;

/* buffers extracted from JS for one createMeshData call; every allocation
 * is registered here the moment it exists so a single release path frees
 * each exactly once on success and on every error exit */
typedef struct {
    float *f[EFX_MESH_MAX_SURFACES * 5];
    uint32_t *i[EFX_MESH_MAX_SURFACES];
    uint32_t *j[EFX_MESH_MAX_SURFACES];
    int nf, ni, nj;
} md_owned;

/* one parsed sprite for the atomic drawSprites loop */
typedef struct {
    float x, y, w, h;
    float color[4];
    float rotation, scale;
    float src[4];
    int has_src;
    float origin[2];
    int has_origin;
} sprite_params;

/* class ids (defined in api.c) */
extern JSClassID texture_class_id;
extern JSClassID imagedata_class_id;
extern JSClassID meshdata_class_id;
extern JSClassID mesh_class_id;
extern JSClassID rendertarget_class_id;
extern JSClassID fontdata_class_id;
extern JSClassID font_class_id;
extern JSClassID particlesystem_class_id;
extern JSClassID audiodata_class_id;
extern JSClassID audiostream_class_id;
extern JSClassID audio_class_id;
extern JSClassID body_class_id;
extern JSClassID character_class_id;

/* prototype function lists defined outside api.c (referenced by CLASS_SPECS) */
extern const JSCFunctionListEntry body_proto_funcs[7];
extern const JSCFunctionListEntry character_proto_funcs[5];
extern const JSCFunctionListEntry particlesystem_proto_funcs[8];
extern const JSCFunctionListEntry audio_proto_funcs[9];

/* shared helpers (defined in api.c) */
extern void efx_api_body_unpin(JSContext *ctx, efxjs_body *b);
extern void efx_api_character_unpin(JSContext *ctx, efxjs_character *c);
extern int efx_api_check_known_fields(JSContext *ctx, JSValueConst obj, const char **known, int nknown, const char *where);
extern JSValue efx_api_generic_error(JSContext *ctx, const char *msg);
extern int efx_api_get_float_array(JSContext *ctx, JSValueConst v, float *out, int n);
extern efxjs_body *efx_api_get_live_body(JSContext *ctx, JSValueConst v);
extern efxjs_character *efx_api_get_live_character(JSContext *ctx, JSValueConst v);
extern efxjs_imagedata *efx_api_get_live_imagedata(JSContext *ctx, JSValueConst v);
extern efxjs_mesh *efx_api_get_live_mesh(JSContext *ctx, JSValueConst v);
extern efxjs_meshdata *efx_api_get_live_meshdata(JSContext *ctx, JSValueConst v);
extern efxjs_particlesystem *efx_api_get_live_ps(JSContext *ctx, JSValueConst v);
extern efxjs_rendertarget *efx_api_get_live_render_target(JSContext *ctx, JSValueConst v);
extern int efx_api_get_live_sample(JSContext *ctx, JSValueConst v, uint64_t *out_handle);
extern struct efx_host_state *efx_api_host_state(JSContext *ctx);
extern int efx_api_opt_bool(JSContext *ctx, JSValueConst obj, const char *key, int *out);
extern int efx_api_opt_number(JSContext *ctx, JSValueConst obj, const char *key, double *out, const char *msg);
extern int efx_api_opt_u32(JSContext *ctx, JSValueConst obj, const char *key, uint32_t *out, const char *msg_num, const char *msg_range);
extern int efx_api_opt_vec3(JSContext *ctx, JSValueConst obj, const char *key, float out[3]);
extern void efx_api_physics_release_wrappers(JSContext *ctx);
extern JSValue efx_api_plain_error(JSContext *ctx, const char *msg);
extern JSValue efx_api_range_error(JSContext *ctx, const char *msg);
extern int efx_api_read_elements(JSContext *ctx, JSValueConst v, int32_t len, efx_elem_policy policy, const char *msg_numbers, const char *msg_finite, const char *msg_int, void (*sink)(void *, int32_t, double), void *ud);
extern int efx_api_read_material(JSContext *ctx, JSValueConst v, efx_material *out);
extern int efx_api_read_number_array(JSContext *ctx, JSValueConst v, float **out, int *out_len, const char *what);
extern int efx_api_read_source_rect(JSContext *ctx, uint64_t tex, JSValueConst srcv, float src[4], int *has_src);
extern int efx_api_read_vec3(JSContext *ctx, JSValueConst v, float out[3], const char *what);
extern JSValue efx_api_register_hook(JSContext *ctx, JSValueConst fn, int which);
extern void efx_api_sink_u32(void *ud, int32_t i, double d);
extern JSValue efx_api_target_call_error(JSContext *ctx, int rc);
extern JSValue efx_api_type_error(JSContext *ctx, const char *msg);

#endif /* EFX_API_INTERNAL_H */
