#include "api/api_internal.h"


/* ------------------------------------------------------------ F4a bindings */

/* ---- natives for the shared prelude validators (ADR 0049) ---- */

/* (slot, enabled, px, py, pz, r, g, b, a, range) */
JSValue efx_js_set_point_light_wire(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 10) {
        return efx_api_type_error(ctx, "point light wire native requires 10 arguments");
    }
    int32_t slot = 0, enabled = 0;
    if (JS_ToInt32(ctx, &slot, argv[0]) < 0 ||
        JS_ToInt32(ctx, &enabled, argv[1]) < 0) {
        return JS_EXCEPTION;
    }
    double v[8];
    for (int i = 0; i < 8; i++) {
        if (JS_ToFloat64(ctx, &v[i], argv[2 + i]) < 0) {
            return JS_EXCEPTION;
        }
    }
    if (!enabled) {
        efx_render_set_point_light(slot, NULL);
        return JS_UNDEFINED;
    }
    efx_point_light l;
    memset(&l, 0, sizeof(l));
    l.pos[0] = (float)v[0];
    l.pos[1] = (float)v[1];
    l.pos[2] = (float)v[2];
    l.color[0] = (float)v[3];
    l.color[1] = (float)v[4];
    l.color[2] = (float)v[5];
    l.color[3] = (float)v[6];
    l.range = (float)v[7];
    l.enabled = 1;
    efx_render_set_point_light(slot, &l);
    return JS_UNDEFINED;
}


/* (enabled, dx, dy, dz, r, g, b, a) */
JSValue efx_js_set_directional_light_wire(JSContext *ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 8) {
        return efx_api_type_error(ctx, "directional light wire native requires 8 arguments");
    }
    int32_t enabled = 0;
    if (JS_ToInt32(ctx, &enabled, argv[0]) < 0) {
        return JS_EXCEPTION;
    }
    double v[7];
    for (int i = 0; i < 7; i++) {
        if (JS_ToFloat64(ctx, &v[i], argv[1 + i]) < 0) {
            return JS_EXCEPTION;
        }
    }
    if (!enabled) {
        efx_render_set_directional_light(NULL);
        return JS_UNDEFINED;
    }
    efx_dir_light l;
    memset(&l, 0, sizeof(l));
    l.dir[0] = (float)v[0];
    l.dir[1] = (float)v[1];
    l.dir[2] = (float)v[2];
    l.color[0] = (float)v[3];
    l.color[1] = (float)v[4];
    l.color[2] = (float)v[5];
    l.color[3] = (float)v[6];
    l.enabled = 1;
    efx_render_set_directional_light(&l);
    return JS_UNDEFINED;
}


/* Mesh.setSurfaceMaterial(surfaceIndex, mat) — the receiver is the subject,
 * so it is resolved first; the receiver errors match the web class liveness
 * guard. */
JSValue efx_js_setMeshSurfaceMaterial(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    efxjs_mesh *mesh = efx_api_get_live_mesh(ctx, this_val);
    if (!mesh) {
        return JS_EXCEPTION;
    }
    if (argc < 2) {
        return efx_api_type_error(ctx,
                          "setSurfaceMaterial requires (surfaceIndex, mat)");
    }
    double index_d = 0;
    if (!JS_IsNumber(argv[0]) || JS_ToFloat64(ctx, &index_d, argv[0]) < 0 ||
        !isfinite(index_d) || index_d != floor(index_d)) {
        return efx_api_range_error(ctx, "surfaceIndex must be an integer");
    }
    int index = (int)index_d;
    int count = efx_render_mesh_surface_count(mesh->handle);
    if (index < 0 || index >= count) {
        return efx_api_range_error(ctx, "surfaceIndex out of range");
    }
    efx_material mat;
    int has = 0;
    if (JS_IsNull(argv[1]) || JS_IsUndefined(argv[1])) {
        has = 0;
    } else {
        if (efx_api_read_material(ctx, argv[1], &mat) != 0) {
            return JS_EXCEPTION;
        }
        has = 1;
    }
    int rc = efx_render_mesh_set_material(mesh->handle, (int)index,
                                          has ? &mat : NULL, has);
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return efx_api_type_error(ctx, "expected a live Mesh");
    }
    if (rc == EFX_RENDER_ERR_INDEX) {
        return efx_api_range_error(ctx, "surfaceIndex out of range");
    }
    if (rc != EFX_RENDER_OK) {
        return efx_api_generic_error(ctx, "setSurfaceMaterial failed");
    }
    return JS_UNDEFINED;
}
