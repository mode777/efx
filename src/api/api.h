#ifndef EFX_API_H
#define EFX_API_H

#include "quickjs.h"

JSValue efx_js_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_quit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_args(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F1 — lifecycle hooks (ADR 0016) */
JSValue efx_js_registerUpdateHook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_registerRenderHook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F2 — 2D drawing */
JSValue efx_js_setClearColor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_setCamera2D(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_createImageData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_createTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_drawQuad(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_setBlendMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_whiteTexture(JSContext *ctx, JSValueConst this_val);

/* F3 — 3D core */
JSValue efx_js_setCamera3D(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_createMeshData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_createMesh(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_drawMesh(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F7 — skinning + animation */
JSValue efx_js_poseMesh(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F4a — lighting + Phong materials */
JSValue efx_js_setLight(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_setDirectionalLight(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_setMeshSurfaceMaterial(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F5a — render targets */
JSValue efx_js_createRenderTarget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_beginRenderTarget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_endRenderTarget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F5b — post-processing chain + render scale */
JSValue efx_js_setPostEffects(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_setRenderScale(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F6a — resource loading */
JSValue efx_js_loadText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_loadImage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F6b — glTF mesh import */
JSValue efx_js_loadMeshData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* per-context setup: registers resource classes, prototypes and state */
int efx_api_init(JSContext *ctx);

/* F9: attach the efx.keyboard / efx.mouse / efx.window sub-namespaces to
 * the single efx object (shared by the desktop binding) */
int efx_api_register_input(JSContext *ctx, JSValueConst efx);

void efx_log(const char *msg);

#endif
