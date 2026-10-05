#ifndef EFX_API_H
#define EFX_API_H

#include "quickjs.h"

JSValue efx_js_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_quit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_args(JSContext *ctx, JSValueConst this_val);

/* F1 — lifecycle hooks (ADR 0016) */
JSValue efx_js_registerUpdateHook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_registerRenderHook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F2 — 2D drawing */
JSValue efx_js_setClearColor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_set_camera2d_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_check_image_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_create_image_data_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_create_texture_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_drawQuad(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_setBlendMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_whiteTexture(JSContext *ctx, JSValueConst this_val);

/* F3 — 3D core */
JSValue efx_js_set_camera3d_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_create_meshdata_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_createMesh(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_drawMesh(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F7 — skinning + animation (Mesh.pose method, ADR 0055) */
JSValue efx_js_poseMesh(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F4a — lighting + Phong materials */
JSValue efx_js_set_point_light_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_set_directional_light_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* Mesh.setSurfaceMaterial method (ADR 0055) */
JSValue efx_js_setMeshSurfaceMaterial(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F5a — render targets */
JSValue efx_js_create_render_target_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_beginRenderTarget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_endRenderTarget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F5b — post-processing chain + render scale */
JSValue efx_js_set_post_effects_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_setRenderScale(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F6a — resource loading */
JSValue efx_js_loadText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_loadData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_load_image_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F6b — glTF mesh import */
JSValue efx_js_load_meshdata_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F8a — font + text */
JSValue efx_js_loadFontData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_check_font_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_create_font_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_drawText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* Font.measure method (ADR 0055) */
JSValue efx_js_measureText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F11 — billboards + CPU particles */
JSValue efx_js_drawBillboard(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_drawSprites(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_create_particle_system_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_ps_set_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_ps_proto(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_drawParticles(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* prelude natives (ADR 0049): binding-provided entry points passed to the
 * prelude's private object; never registered on `efx` */
JSValue efx_js_live_sample(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* per-context setup: registers resource classes, prototypes and state */
int efx_api_init(JSContext *ctx);

/* F9: attach the efx.keyboard / efx.mouse / efx.window sub-namespaces to
 * the single efx object (shared by the desktop binding) */
int efx_api_register_input(JSContext *ctx, JSValueConst efx);

/* F12: attach the efx.physics sub-namespace to the single efx object */
int efx_api_register_physics(JSContext *ctx, JSValueConst efx);

/* F12 prelude natives (ADR 0049): flat-form physics constructors/queries */
JSValue efx_js_check_mesh(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_physics_create_body_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_physics_create_static_mesh_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_physics_create_character_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_physics_step_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_physics_raycast_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_physics_overlap_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_physics_shape_cast_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F12: live Body/Character wrappers are held by the world; release them
 * before the context is freed */
void efx_api_physics_release(JSContext *ctx);

/* F14: attach the efx.audio sub-namespace to the single efx object */
int efx_api_register_audio(JSContext *ctx, JSValueConst efx);

/* F14 prelude natives (ADR 0049): loaders return the wrapper or a negative
 * code (-1 unreadable, -2 undecodable); play takes the validated scalars */
JSValue efx_js_audio_load_data_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_audio_load_stream_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_audio_check_source(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_audio_play_wire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif
