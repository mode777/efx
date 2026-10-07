#include "api/api_internal.h"


/* indices: non-negative integers in uint32 range; non-integer → RangeError
 * (the F2 pixel-bytes precedent). `what` names the field for messages. */
/* ---- native for the shared prelude validator (ADR 0049): the bag was
 * validated and marshalled into concatenated attribute streams with a
 * per-surface length table and the per-surface material wire ---- */

static void wire_mat_from_block(efx_material *m, const float *f,
                                const double *maps) {
    for (int i = 0; i < 4; i++) {
        m->ambient[i] = f[i];
        m->diffuse[i] = f[4 + i];
        m->specular[i] = f[8 + i];
        m->emissive[i] = f[12 + i];
    }
    m->shininess = f[16];
    m->blend = (int)f[17];
    m->unlit = (int)f[18];
    m->ambient_map = (uint64_t)maps[0];
    m->diffuse_map = (uint64_t)maps[1];
    m->specular_map = (uint64_t)maps[2];
    m->emissive_map = (uint64_t)maps[3];
    m->alpha_mask = (uint64_t)maps[4];
}

static float *wire_f32(JSContext *ctx, JSValueConst v, size_t *out_len) {
    size_t blen = 0;
    uint8_t *bytes = NULL;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, v, NULL, NULL, NULL);
    if (JS_IsException(ab)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        return NULL;
    }
    bytes = JS_GetArrayBuffer(ctx, &blen, ab);
    JS_FreeValue(ctx, ab);
    if (!bytes || blen % sizeof(float) != 0) {
        return NULL;
    }
    *out_len = blen / sizeof(float);
    return (float *)bytes;
}

static int32_t *wire_i32(JSContext *ctx, JSValueConst v, size_t *out_len) {
    size_t blen = 0;
    uint8_t *bytes = NULL;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, v, NULL, NULL, NULL);
    if (JS_IsException(ab)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        return NULL;
    }
    bytes = JS_GetArrayBuffer(ctx, &blen, ab);
    JS_FreeValue(ctx, ab);
    if (!bytes || blen % sizeof(int32_t) != 0) {
        return NULL;
    }
    *out_len = blen / sizeof(int32_t);
    return (int32_t *)bytes;
}

static uint32_t *wire_u32(JSContext *ctx, JSValueConst v, size_t *out_len) {
    size_t blen = 0;
    uint8_t *bytes = NULL;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, v, NULL, NULL, NULL);
    if (JS_IsException(ab)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        return NULL;
    }
    bytes = JS_GetArrayBuffer(ctx, &blen, ab);
    JS_FreeValue(ctx, ab);
    if (!bytes || blen % sizeof(uint32_t) != 0) {
        return NULL;
    }
    *out_len = blen / sizeof(uint32_t);
    return (uint32_t *)bytes;
}

/* (count, lens Int32Array(count*7), pos, nrm, uv, col, joints Uint32Array,
 * weights, indices Uint32Array, blocks Float32Array|null, maps Float64Array|null,
 * matHas Int32Array) */
JSValue efx_js_create_meshdata_wire(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 12) {
        return efx_api_type_error(ctx, "meshdata wire native requires 12 arguments");
    }
    int32_t count = 0;
    if (JS_ToInt32(ctx, &count, argv[0]) < 0 || count < 1 ||
        count > EFX_MESH_MAX_SURFACES) {
        return efx_api_range_error(ctx, "surfaces must hold 1..16 entries");
    }
    size_t lens_len = 0;
    int32_t *lens = wire_i32(ctx, argv[1], &lens_len);
    if (!lens || lens_len < (size_t)count * 7) {
        return efx_api_type_error(ctx, "meshdata wire lens table");
    }
    size_t np = 0, nn = 0, nu = 0, nc = 0, nj = 0, nw = 0, ni = 0;
    float *pos = wire_f32(ctx, argv[2], &np);
    float *nrm = wire_f32(ctx, argv[3], &nn);
    float *uv = wire_f32(ctx, argv[4], &nu);
    float *col = wire_f32(ctx, argv[5], &nc);
    uint32_t *joints = wire_u32(ctx, argv[6], &nj);
    float *weights = wire_f32(ctx, argv[7], &nw);
    uint32_t *idx = wire_u32(ctx, argv[8], &ni);
    if (!pos || !nrm || !uv || !col || !joints || !weights || !idx) {
        return efx_api_type_error(ctx, "meshdata wire attribute streams");
    }
    size_t nhas = 0;
    float *blocks = wire_f32(ctx, argv[9], &(size_t){0});
    JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[10], NULL, NULL, NULL);
    double *maps = NULL;
    if (!JS_IsException(ab)) {
        size_t blen = 0;
        uint8_t *bytes = JS_GetArrayBuffer(ctx, &blen, ab);
        JS_FreeValue(ctx, ab);
        if (bytes && blen % sizeof(double) == 0) {
            maps = (double *)bytes;
        }
    } else {
        JS_FreeValue(ctx, JS_GetException(ctx));
        maps = NULL;
    }
    int32_t *mat_has = wire_i32(ctx, argv[11], &nhas);
    if (!mat_has || nhas < (size_t)count) {
        return efx_api_type_error(ctx, "meshdata wire material flags");
    }

    /* slice the streams per surface (efx_meshdata_create copies) */
    efx_surface_src srcs[EFX_MESH_MAX_SURFACES];
    memset(srcs, 0, sizeof(srcs));
    size_t op = 0, on = 0, ou = 0, oc = 0, oj = 0, ow = 0, oi = 0;
    for (int i = 0; i < count; i++) {
        int32_t *L = lens + (size_t)i * 7;
        if ((size_t)L[0] > np - op || (size_t)L[1] > nn - on ||
            (size_t)L[2] > nu - ou || (size_t)L[3] > nc - oc ||
            (size_t)L[4] > nj - oj || (size_t)L[5] > nw - ow ||
            (size_t)L[6] > ni - oi) {
            return efx_api_range_error(ctx, "invalid mesh data");
        }
        srcs[i].positions = pos + op;
        srcs[i].positions_len = L[0];
        srcs[i].normals = nrm + on;
        srcs[i].normals_len = L[1];
        srcs[i].uvs = uv + ou;
        srcs[i].uvs_len = L[2];
        srcs[i].colors = col + oc;
        srcs[i].colors_len = L[3];
        /* joints/weights live on the surface src alongside (F6c) */
        srcs[i].joints = joints + oj;
        srcs[i].joints_len = L[4];
        srcs[i].weights = weights + ow;
        srcs[i].weights_len = L[5];
        srcs[i].indices = idx + oi;
        srcs[i].indices_len = L[6];
        op += L[0]; on += L[1]; ou += L[2]; oc += L[3];
        oj += L[4]; ow += L[5]; oi += L[6];
    }
    int err = 0;
    efx_meshdata *md = efx_meshdata_create(srcs, count, &err);
    if (!md) {
        if (err == EFX_MESHERR_COUNT || err == EFX_MESHERR_LEN ||
            err == EFX_MESHERR_INDEX) {
            return efx_api_range_error(ctx, "invalid mesh data");
        }
        return efx_api_generic_error(ctx, "out of memory");
    }
    if (blocks && maps) {
        for (int i = 0; i < count; i++) {
            if (!mat_has[i]) {
                continue;
            }
            efx_material m;
            wire_mat_from_block(&m, blocks + (size_t)i * 19,
                                maps + (size_t)i * 5);
            efx_meshdata_set_material(md, i, &m, 1);
        }
    }
    efxjs_meshdata *wrap = calloc(1, sizeof(efxjs_meshdata));
    if (!wrap) {
        efx_meshdata_destroy(md);
        return efx_api_generic_error(ctx, "out of memory");
    }
    wrap->md = md;
    wrap->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, meshdata_class_id);
    JS_SetOpaque(obj, wrap);
    return obj;
}


JSValue efx_js_createMesh(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "createMesh requires a MeshData");
    }
    efxjs_meshdata *md = efx_api_get_live_meshdata(ctx, argv[0]);
    if (!md) {
        return JS_EXCEPTION;
    }
    uint64_t handle = efx_render_mesh_create(md->md);
    if (!handle) {
        return efx_api_generic_error(ctx, "mesh upload failed (no GPU context?)");
    }
    efxjs_mesh *m = calloc(1, sizeof(efxjs_mesh));
    if (!m) {
        efx_render_mesh_destroy(handle);
        return efx_api_generic_error(ctx, "out of memory");
    }
    m->handle = handle;
    m->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, mesh_class_id);
    JS_SetOpaque(obj, m);
    return obj;
}


JSValue efx_js_drawMesh(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "drawMesh requires a mesh");
    }
    efxjs_mesh *mesh = efx_api_get_live_mesh(ctx, argv[0]);
    if (!mesh) {
        return JS_EXCEPTION;
    }

    float transform[16];
    int has_transform = 0;
    float color[4] = {1, 1, 1, 1};
    int skinned = 0;
    int depth_write = 1;

    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        JSValueConst opts = argv[1];
        if (!JS_IsObject(opts)) {
            return efx_api_type_error(ctx, "drawMesh options must be an object");
        }
        static const char *known[] = {"transform", "color", "skinned",
                                      "depthWrite"};
        if (efx_api_check_known_fields(ctx, opts, known, 4, "drawMesh") != 0) {
            return JS_EXCEPTION;
        }
        JSValue tv = JS_GetPropertyStr(ctx, opts, "transform");
        if (JS_IsUndefined(tv)) {
            JS_FreeValue(ctx, tv);
        } else {
            float *buf = NULL;
            int len = 0;
            int rc = efx_api_read_number_array(ctx, tv, &buf, &len, "transform");
            JS_FreeValue(ctx, tv);
            if (rc != 0) {
                return JS_EXCEPTION;
            }
            if (len != 16) {
                free(buf);
                return efx_api_range_error(ctx, "transform must hold 16 numbers");
            }
            memcpy(transform, buf, sizeof(transform));
            free(buf);
            has_transform = 1;
        }

        JSValue cv = JS_GetPropertyStr(ctx, opts, "color");
        if (JS_IsUndefined(cv)) {
            JS_FreeValue(ctx, cv);
        } else {
            float *buf = NULL;
            int len = 0;
            int rc = efx_api_read_number_array(ctx, cv, &buf, &len, "color");
            JS_FreeValue(ctx, cv);
            if (rc != 0) {
                return JS_EXCEPTION;
            }
            if (len != 4) {
                free(buf);
                return efx_api_range_error(ctx, "color must hold 4 numbers");
            }
            memcpy(color, buf, sizeof(color));
            free(buf);
        }

        JSValue sv = JS_GetPropertyStr(ctx, opts, "skinned");
        if (!JS_IsUndefined(sv)) {
            if (!JS_IsBool(sv)) {
                JS_FreeValue(ctx, sv);
                return efx_api_type_error(ctx, "skinned must be a boolean");
            }
            skinned = JS_ToBool(ctx, sv) ? 1 : 0;
        }
        JS_FreeValue(ctx, sv);

        JSValue dv = JS_GetPropertyStr(ctx, opts, "depthWrite");
        if (!JS_IsUndefined(dv)) {
            if (!JS_IsBool(dv)) {
                JS_FreeValue(ctx, dv);
                return efx_api_type_error(ctx, "depthWrite must be a boolean");
            }
            depth_write = JS_ToBool(ctx, dv) ? 1 : 0;
        }
        JS_FreeValue(ctx, dv);
    }

    int rc = efx_render_mesh(mesh->handle, has_transform ? transform : NULL,
                             color, skinned, depth_write);
    if (rc == EFX_RENDER_ERR_BUDGET) {
        return efx_api_range_error(ctx, "display list budget exceeded");
    }
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return efx_api_type_error(ctx, "expected a live Mesh");
    }
    if (rc == EFX_RENDER_ERR_RIG) {
        return efx_api_type_error(ctx, "mesh has no rig to draw skinned");
    }
    if (rc == EFX_RENDER_ERR_FEEDBACK) {
        return efx_api_type_error(ctx,
                          "cannot sample the render target being drawn into");
    }
    if (rc != EFX_RENDER_OK) {
        return efx_api_generic_error(ctx, "drawMesh failed");
    }
    return JS_UNDEFINED;
}

/* F7: parse one pose sample ({ clip, time, weight? }); clip names resolve
 * through the live Mesh's rig. Returns 0 ok (exception pending otherwise). */
static int read_pose_sample(JSContext *ctx, JSValueConst v, efxjs_mesh *mesh,
                            efx_pose_sample *out) {
    if (!JS_IsObject(v)) {
        efx_api_type_error(ctx, "pose samples must be objects");
        return -1;
    }
    static const char *known[] = {"clip", "time", "weight"};
    if (efx_api_check_known_fields(ctx, v, known, 3, "pose sample") != 0) {
        return -1;
    }
    JSValue cv = JS_GetPropertyStr(ctx, v, "clip");
    if (JS_IsUndefined(cv)) {
        JS_FreeValue(ctx, cv);
        efx_api_type_error(ctx, "pose sample requires clip");
        return -1;
    }
    if (JS_IsString(cv)) {
        const char *name = JS_ToCString(ctx, cv);
        int idx = efx_render_mesh_find_clip(mesh->handle, name);
        JS_FreeCString(ctx, name);
        JS_FreeValue(ctx, cv);
        if (idx < 0) {
            efx_api_generic_error(ctx, "unknown clip name");
            return -1;
        }
        out->clip = idx;
    } else if (JS_IsNumber(cv)) {
        double d = 0;
        JS_ToFloat64(ctx, &d, cv);
        JS_FreeValue(ctx, cv);
        if (!isfinite(d) || d != floor(d) || d < 0) {
            efx_api_range_error(ctx, "clip index out of range");
            return -1;
        }
        out->clip = (int)d;
    } else {
        JS_FreeValue(ctx, cv);
        efx_api_type_error(ctx, "clip must be a name or index");
        return -1;
    }

    JSValue tv = JS_GetPropertyStr(ctx, v, "time");
    if (!JS_IsNumber(tv)) {
        JS_FreeValue(ctx, tv);
        efx_api_type_error(ctx, "pose sample requires a numeric time");
        return -1;
    }
    double t = 0;
    JS_ToFloat64(ctx, &t, tv);
    JS_FreeValue(ctx, tv);
    if (!isfinite(t)) {
        efx_api_range_error(ctx, "time must be finite");
        return -1;
    }
    out->time = (float)t;

    out->weight = 1.0f;
    JSValue wv = JS_GetPropertyStr(ctx, v, "weight");
    if (!JS_IsUndefined(wv)) {
        if (!JS_IsNumber(wv)) {
            JS_FreeValue(ctx, wv);
            efx_api_type_error(ctx, "weight must be a number");
            return -1;
        }
        double w = 0;
        JS_ToFloat64(ctx, &w, wv);
        JS_FreeValue(ctx, wv);
        if (!isfinite(w) || w < 0) {
            efx_api_range_error(ctx, "weight must be finite and >= 0");
            return -1;
        }
        out->weight = (float)w;
    } else {
        JS_FreeValue(ctx, wv);
    }
    return 0;
}


/* Mesh.pose(pose) — the receiver is the subject, so it is resolved first; the
 * receiver errors match the web class liveness guard. */
JSValue efx_js_poseMesh(JSContext *ctx, JSValueConst this_val, int argc,
                        JSValueConst *argv) {
    efxjs_mesh *mesh = efx_api_get_live_mesh(ctx, this_val);
    if (!mesh) {
        return JS_EXCEPTION;
    }
    if (argc < 1) {
        return efx_api_type_error(ctx, "pose requires a pose");
    }
    if (!efx_render_mesh_skinned(mesh->handle)) {
        return efx_api_type_error(ctx, "pose requires a Mesh with a rig");
    }
    JSValueConst pose = argv[0];
    efx_pose_sample *samples = NULL;
    int count = 0;
    int rc = 0;
    if (JS_IsArray(pose)) {
        JSValue lenv = JS_GetPropertyStr(ctx, pose, "length");
        int32_t len = -1;
        JS_ToInt32(ctx, &len, lenv);
        JS_FreeValue(ctx, lenv);
        if (len < 0) {
            return efx_api_range_error(ctx, "pose array length is invalid");
        }
        if (len > 0) {
            samples = malloc((size_t)len * sizeof(*samples));
            if (!samples) {
                return efx_api_generic_error(ctx, "out of memory");
            }
        }
        for (int32_t i = 0; i < len; i++) {
            JSValue sv = JS_GetPropertyUint32(ctx, pose, (uint32_t)i);
            int sr = read_pose_sample(ctx, sv, mesh, &samples[i]);
            JS_FreeValue(ctx, sv);
            if (sr != 0) {
                free(samples);
                return JS_EXCEPTION;
            }
        }
        count = (int)len;
    } else if (JS_IsObject(pose)) {
        samples = malloc(sizeof(*samples));
        if (!samples) {
            return efx_api_generic_error(ctx, "out of memory");
        }
        if (read_pose_sample(ctx, pose, mesh, &samples[0]) != 0) {
            free(samples);
            return JS_EXCEPTION;
        }
        count = 1;
    } else {
        return efx_api_type_error(ctx, "pose must be a sample or an array of samples");
    }

    rc = efx_render_mesh_pose(mesh->handle, samples, count);
    free(samples);
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return efx_api_type_error(ctx, "pose requires a Mesh with a rig");
    }
    if (rc == EFX_RENDER_ERR_INDEX) {
        return efx_api_range_error(ctx, "clip index out of range");
    }
    if (rc != EFX_RENDER_OK) {
        return efx_api_generic_error(ctx, "pose failed");
    }
    return JS_UNDEFINED;
}


/* (px, py, pz, tx, ty, tz, fov, near, far) */
JSValue efx_js_set_camera3d_wire(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 9) {
        return efx_api_type_error(ctx, "camera3d wire native requires 9 arguments");
    }
    double d[9];
    for (int i = 0; i < 9; i++) {
        if (JS_ToFloat64(ctx, &d[i], argv[i]) < 0) {
            return JS_EXCEPTION;
        }
    }
    efx_camera3d cam;
    memset(&cam, 0, sizeof(cam));
    cam.pos[0] = (float)d[0];
    cam.pos[1] = (float)d[1];
    cam.pos[2] = (float)d[2];
    cam.target[0] = (float)d[3];
    cam.target[1] = (float)d[4];
    cam.target[2] = (float)d[5];
    cam.fov = (float)d[6];
    cam.near_z = (float)d[7];
    cam.far_z = (float)d[8];
    efx_render_set_camera3d(&cam);
    return JS_UNDEFINED;
}


