#include "api/api_internal.h"


/* ------------------------------------------------------------ F4a bindings */

JSValue efx_js_setLight(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return efx_api_type_error(ctx, "setLight requires (slot, opts)");
    }
    double slot_d = 0;
    if (!JS_IsNumber(argv[0]) || JS_ToFloat64(ctx, &slot_d, argv[0]) < 0 ||
        !isfinite(slot_d) || slot_d != floor(slot_d) || slot_d < 0 ||
        slot_d > (double)(EFX_MAX_POINT_LIGHTS - 1)) {
        return efx_api_range_error(ctx, "light slot must be an integer 0..3");
    }
    int slot = (int)slot_d;
    if (JS_IsNull(argv[1]) || JS_IsUndefined(argv[1])) {
        efx_render_set_point_light(slot, NULL);
        return JS_UNDEFINED;
    }
    if (!JS_IsObject(argv[1])) {
        return efx_api_type_error(ctx, "setLight options must be an object or null");
    }
    static const char *known[] = {"pos", "color", "range"};
    if (efx_api_check_known_fields(ctx, argv[1], known, 3, "setLight") != 0) {
        return JS_EXCEPTION;
    }
    efx_point_light l;
    memset(&l, 0, sizeof(l));
    JSValue pv = JS_GetPropertyStr(ctx, argv[1], "pos");
    if (JS_IsUndefined(pv)) {
        JS_FreeValue(ctx, pv);
        return efx_api_type_error(ctx, "setLight requires pos");
    }
    if (efx_api_read_vec3(ctx, pv, l.pos, "pos") != 0) {
        JS_FreeValue(ctx, pv);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, pv);
    JSValue cv = JS_GetPropertyStr(ctx, argv[1], "color");
    if (JS_IsUndefined(cv)) {
        JS_FreeValue(ctx, cv);
        return efx_api_type_error(ctx, "setLight requires color");
    }
    if (efx_api_get_float_array(ctx, cv, l.color, 4) != 0) {
        JS_FreeValue(ctx, cv);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, cv);
    JSValue rv = JS_GetPropertyStr(ctx, argv[1], "range");
    if (JS_IsUndefined(rv)) {
        JS_FreeValue(ctx, rv);
        l.range = 0.0f;
    } else {
        double d = 0;
        int bad = !JS_IsNumber(rv) || JS_ToFloat64(ctx, &d, rv) < 0;
        JS_FreeValue(ctx, rv);
        if (bad) {
            return efx_api_type_error(ctx, "range must be a number");
        }
        if (!isfinite(d) || d < 0) {
            return efx_api_range_error(ctx, "range must be a finite number >= 0");
        }
        l.range = (float)d;
    }
    l.enabled = 1;
    efx_render_set_point_light((int)slot, &l);
    return JS_UNDEFINED;
}


JSValue efx_js_setDirectionalLight(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "setDirectionalLight requires an options object or null");
    }
    if (JS_IsNull(argv[0]) || JS_IsUndefined(argv[0])) {
        efx_render_set_directional_light(NULL);
        return JS_UNDEFINED;
    }
    if (!JS_IsObject(argv[0])) {
        return efx_api_type_error(ctx, "setDirectionalLight options must be an object or null");
    }
    static const char *known[] = {"dir", "color"};
    if (efx_api_check_known_fields(ctx, argv[0], known, 2, "setDirectionalLight") != 0) {
        return JS_EXCEPTION;
    }
    efx_dir_light l;
    memset(&l, 0, sizeof(l));
    JSValue dv = JS_GetPropertyStr(ctx, argv[0], "dir");
    if (JS_IsUndefined(dv)) {
        JS_FreeValue(ctx, dv);
        return efx_api_type_error(ctx, "setDirectionalLight requires dir");
    }
    if (efx_api_read_vec3(ctx, dv, l.dir, "dir") != 0) {
        JS_FreeValue(ctx, dv);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, dv);
    if (l.dir[0] == 0.0f && l.dir[1] == 0.0f && l.dir[2] == 0.0f) {
        return efx_api_type_error(ctx, "dir must be non-zero");
    }
    JSValue cv = JS_GetPropertyStr(ctx, argv[0], "color");
    if (JS_IsUndefined(cv)) {
        JS_FreeValue(ctx, cv);
        return efx_api_type_error(ctx, "setDirectionalLight requires color");
    }
    if (efx_api_get_float_array(ctx, cv, l.color, 4) != 0) {
        JS_FreeValue(ctx, cv);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, cv);
    l.enabled = 1;
    efx_render_set_directional_light(&l);
    return JS_UNDEFINED;
}


JSValue efx_js_setMeshSurfaceMaterial(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) {
        return efx_api_type_error(ctx,
                          "setMeshSurfaceMaterial requires (mesh, surfaceIndex, mat)");
    }
    efxjs_mesh *mesh = efx_api_get_live_mesh(ctx, argv[0]);
    if (!mesh) {
        return JS_EXCEPTION;
    }
    double index_d = 0;
    if (!JS_IsNumber(argv[1]) || JS_ToFloat64(ctx, &index_d, argv[1]) < 0 ||
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
    if (JS_IsNull(argv[2]) || JS_IsUndefined(argv[2])) {
        has = 0;
    } else {
        if (efx_api_read_material(ctx, argv[2], &mat) != 0) {
            return JS_EXCEPTION;
        }
        has = 1;
    }
    int rc = efx_render_mesh_set_material(mesh->handle, (int)index,
                                          has ? &mat : NULL, has);    if (rc == EFX_RENDER_ERR_HANDLE) {
        return efx_api_type_error(ctx, "expected a live Mesh");
    }
    if (rc == EFX_RENDER_ERR_INDEX) {
        return efx_api_range_error(ctx, "surfaceIndex out of range");
    }
    if (rc != EFX_RENDER_OK) {
        return efx_api_generic_error(ctx, "setMeshSurfaceMaterial failed");
    }
    return JS_UNDEFINED;
}
