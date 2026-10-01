#include "api/api_internal.h"


/* indices: non-negative integers in uint32 range; non-integer → RangeError
 * (the F2 pixel-bytes precedent). `what` names the field for messages. */
static int read_index_array(JSContext *ctx, JSValueConst v, uint32_t **out,
                            int *out_len, const char *what) {
    int is_ta = JS_GetTypedArrayType(v);
    if (!JS_IsArray(v) && is_ta < 0) {
        efx_api_type_error(ctx, what);
        return -1;
    }
    JSValue lenv = JS_GetPropertyStr(ctx, v, "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    if (len < 0) {
        efx_api_range_error(ctx, what);
        return -2;
    }
    uint32_t *buf = len ? malloc((size_t)len * sizeof(uint32_t)) : NULL;
    if (len && !buf) {
        efx_api_generic_error(ctx, "out of memory");
        return -3;
    }
    if (efx_api_read_elements(ctx, v, len, EFX_ELEM_U32,
                      "array elements must be numbers", NULL,
                      "array elements must be integers in [0, 2^32-1]",
                      efx_api_sink_u32, buf) != 0) {
        free(buf);
        return -2;
    }
    *out = buf;
    *out_len = (int)len;
    return 0;
}


static const char *MD_KEYS[] = {"positions", "normals", "uvs",
                                "colors", "joints", "weights", "indices"};

static const char *MD_KEYS_MAT[] = {"positions", "normals", "uvs",
                                    "colors", "joints", "weights",
                                    "indices", "materials"};


static void md_owned_free(md_owned *o) {
    for (int k = 0; k < o->nf; k++) {
        free(o->f[k]);
    }
    for (int k = 0; k < o->ni; k++) {
        free(o->i[k]);
    }
    for (int k = 0; k < o->nj; k++) {
        free(o->j[k]);
    }
    o->nf = 0;
    o->ni = 0;
    o->nj = 0;
}


/* extract one surface object into an efx_surface_src; buffers are owned
 * by *own (the caller releases them, on success and on failure alike) */
static int read_surface(JSContext *ctx, JSValueConst obj, efx_surface_src *s,
                        md_owned *own, int allow_materials) {
    memset(s, 0, sizeof(*s));
    if (!JS_IsObject(obj)) {
        efx_api_type_error(ctx, "surfaces must be objects");
        return -1;
    }
    if (allow_materials
            ? efx_api_check_known_fields(ctx, obj, MD_KEYS_MAT, 8, "surface") != 0
            : efx_api_check_known_fields(ctx, obj, MD_KEYS, 7, "surface") != 0) {
        return -1;
    }
    static const char *keys[] = {"positions", "normals", "uvs", "colors"};
    float *bufs[4] = {NULL, NULL, NULL, NULL};
    int lens[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
        JSValue v = JS_GetPropertyStr(ctx, obj, keys[i]);
        if (JS_IsUndefined(v)) {
            JS_FreeValue(ctx, v);
            continue;
        }
        int rc = efx_api_read_number_array(ctx, v, &bufs[i], &lens[i], keys[i]);
        JS_FreeValue(ctx, v);
        if (rc != 0) {
            return -1;
        }
        own->f[own->nf++] = bufs[i];
    }
    s->positions = bufs[0];
    s->positions_len = lens[0];
    s->normals = bufs[1];
    s->normals_len = lens[1];
    s->uvs = bufs[2];
    s->uvs_len = lens[2];
    s->colors = bufs[3];
    s->colors_len = lens[3];
    /* F6c skinned attributes: joints are integer indices, weights finite
     * floats; pairing/count validation happens in efx_meshdata_create */
    JSValue jv = JS_GetPropertyStr(ctx, obj, "joints");
    if (!JS_IsUndefined(jv)) {
        uint32_t *jb = NULL;
        int jl = 0;
        int rc = read_index_array(ctx, jv, &jb, &jl, "joints");
        JS_FreeValue(ctx, jv);
        if (rc != 0) {
            return -1;
        }
        s->joints = jb;
        s->joints_len = jl;
        own->j[own->nj++] = jb;
    } else {
        JS_FreeValue(ctx, jv);
    }
    JSValue wv = JS_GetPropertyStr(ctx, obj, "weights");
    if (!JS_IsUndefined(wv)) {
        float *wb = NULL;
        int wl = 0;
        int rc = efx_api_read_number_array(ctx, wv, &wb, &wl, "weights");
        JS_FreeValue(ctx, wv);
        if (rc != 0) {
            return -1;
        }
        s->weights = wb;
        s->weights_len = wl;
        own->f[own->nf++] = wb;
    } else {
        JS_FreeValue(ctx, wv);
    }
    JSValue iv = JS_GetPropertyStr(ctx, obj, "indices");
    if (!JS_IsUndefined(iv)) {
        uint32_t *ibuf = NULL;
        int ilen = 0;
        int rc = read_index_array(ctx, iv, &ibuf, &ilen, "indices");
        JS_FreeValue(ctx, iv);
        if (rc != 0) {
            return -1;
        }
        s->indices = ibuf;
        s->indices_len = ilen;
        own->i[own->ni++] = ibuf;
    }
    if (!s->positions) {
        efx_api_type_error(ctx, "surface requires positions");
        return -1;
    }
    return 0;
}


/* read a `surfaces` array into src/own; consumes the `surfaces` and
 * `positions` JSValues (positions is unused in this form) */
static int read_mesh_surfaces(JSContext *ctx, JSValue surfaces,
                              JSValue positions, efx_surface_src *src,
                              md_owned *own, int *out_count) {
    if (!JS_IsArray(surfaces)) {
        JS_FreeValue(ctx, surfaces);
        JS_FreeValue(ctx, positions);
        efx_api_type_error(ctx, "surfaces must be an array");
        return -1;
    }
    JSValue lenv = JS_GetPropertyStr(ctx, surfaces, "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    JS_FreeValue(ctx, positions);
    if (len < 1 || len > EFX_MESH_MAX_SURFACES) {
        JS_FreeValue(ctx, surfaces);
        efx_api_range_error(ctx, "surfaces must hold 1..16 entries");
        return -1;
    }
    for (int32_t i = 0; i < len; i++) {
        JSValue sv = JS_GetPropertyUint32(ctx, surfaces, (uint32_t)i);
        int rc = read_surface(ctx, sv, &src[i], own, 0);
        JS_FreeValue(ctx, sv);
        if (rc != 0) {
            JS_FreeValue(ctx, surfaces);
            return -1;
        }
        (*out_count)++;
    }
    JS_FreeValue(ctx, surfaces);
    return 0;
}

/* F4a: optional parallel materials array (one entry per surface) */
static int read_materials(JSContext *ctx, JSValueConst opts,
                          efx_material *mats, uint8_t *mat_has, int count) {
    JSValue materials = JS_GetPropertyStr(ctx, opts, "materials");
    if (JS_IsUndefined(materials)) {
        JS_FreeValue(ctx, materials);
        return 0;
    }
    if (!JS_IsArray(materials)) {
        JS_FreeValue(ctx, materials);
        efx_api_type_error(ctx, "materials must be an array");
        return -1;
    }
    JSValue mlenv = JS_GetPropertyStr(ctx, materials, "length");
    int32_t mlen = -1;
    JS_ToInt32(ctx, &mlen, mlenv);
    JS_FreeValue(ctx, mlenv);
    if (mlen != count) {
        JS_FreeValue(ctx, materials);
        efx_api_range_error(ctx, "materials must have one entry per surface");
        return -1;
    }
    for (int32_t i = 0; i < count; i++) {
        JSValue mv = JS_GetPropertyUint32(ctx, materials, (uint32_t)i);
        if (JS_IsNull(mv) || JS_IsUndefined(mv)) {
            JS_FreeValue(ctx, mv);
            continue;
        }
        if (efx_api_read_material(ctx, mv, &mats[i]) != 0) {
            JS_FreeValue(ctx, mv);
            JS_FreeValue(ctx, materials);
            return -1;
        }
        mat_has[i] = 1;
        JS_FreeValue(ctx, mv);
    }
    JS_FreeValue(ctx, materials);
    return 0;
}

/* create the MeshData + JS wrapper from the read sources/materials; releases
 * the temporary source buffers via `own` */
static JSValue build_meshdata(JSContext *ctx, efx_surface_src *src, int count,
                              efx_material *mats, uint8_t *mat_has,
                              md_owned *own) {
    int err = 0;
    efx_meshdata *md = efx_meshdata_create(src, count, &err);
    md_owned_free(own);
    if (!md) {
        if (err == EFX_MESHERR_COUNT || err == EFX_MESHERR_LEN ||
            err == EFX_MESHERR_INDEX) {
            return efx_api_range_error(ctx, "invalid mesh data");
        }
        return efx_api_generic_error(ctx, "out of memory");
    }
    for (int i = 0; i < count; i++) {
        efx_meshdata_set_material(md, i, mat_has[i] ? &mats[i] : NULL,
                                  mat_has[i]);
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

JSValue efx_js_createMeshData(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return efx_api_type_error(ctx, "createMeshData requires an options object");
    }
    JSValueConst opts = argv[0];
    static const char *bag_keys[] = {"surfaces", "positions", "normals",
                                     "uvs", "colors", "joints", "weights",
                                     "indices", "materials"};
    if (efx_api_check_known_fields(ctx, opts, bag_keys, 9, "createMeshData") != 0) {
        return JS_EXCEPTION;
    }

    JSValue surfaces = JS_GetPropertyStr(ctx, opts, "surfaces");
    JSValue positions = JS_GetPropertyStr(ctx, opts, "positions");
    int has_surfaces = !JS_IsUndefined(surfaces);
    int has_positions = !JS_IsUndefined(positions);
    if (has_surfaces && has_positions) {
        JS_FreeValue(ctx, surfaces);
        JS_FreeValue(ctx, positions);
        return efx_api_type_error(ctx,
                          "pass either surfaces or single-surface fields");
    }
    if (!has_surfaces && !has_positions) {
        JS_FreeValue(ctx, surfaces);
        JS_FreeValue(ctx, positions);
        return efx_api_type_error(ctx, "createMeshData requires surfaces");
    }

    efx_surface_src src[EFX_MESH_MAX_SURFACES];
    md_owned own;
    memset(&own, 0, sizeof(own));
    int count = 0;

    if (has_surfaces) {
        if (read_mesh_surfaces(ctx, surfaces, positions, src, &own, &count) != 0) {
            md_owned_free(&own);
            return JS_EXCEPTION;
        }
    } else {
        JS_FreeValue(ctx, surfaces);
        /* the bag itself is the single surface */
        int rc = read_surface(ctx, opts, &src[0], &own, 1);
        JS_FreeValue(ctx, positions);
        if (rc != 0) {
            md_owned_free(&own);
            return JS_EXCEPTION;
        }
        count = 1;
    }

    efx_material mats[EFX_MESH_MAX_SURFACES];
    uint8_t mat_has[EFX_MESH_MAX_SURFACES];
    memset(mat_has, 0, sizeof(mat_has));
    if (read_materials(ctx, opts, mats, mat_has, count) != 0) {
        md_owned_free(&own);
        return JS_EXCEPTION;
    }

    return build_meshdata(ctx, src, count, mats, mat_has, &own);
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

    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        JSValueConst opts = argv[1];
        if (!JS_IsObject(opts)) {
            return efx_api_type_error(ctx, "drawMesh options must be an object");
        }
        static const char *known[] = {"transform", "color", "skinned"};
        if (efx_api_check_known_fields(ctx, opts, known, 3, "drawMesh") != 0) {
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
    }

    int rc = efx_render_mesh(mesh->handle, has_transform ? transform : NULL,
                             color, skinned);
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


JSValue efx_js_poseMesh(JSContext *ctx, JSValueConst this_val, int argc,
                        JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return efx_api_type_error(ctx, "poseMesh requires (mesh, pose)");
    }
    efxjs_mesh *mesh = efx_api_get_live_mesh(ctx, argv[0]);
    if (!mesh) {
        return JS_EXCEPTION;
    }
    if (!efx_render_mesh_skinned(mesh->handle)) {
        return efx_api_type_error(ctx, "poseMesh requires a Mesh with a rig");
    }
    JSValueConst pose = argv[1];
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
        return efx_api_type_error(ctx, "poseMesh requires a Mesh with a rig");
    }
    if (rc == EFX_RENDER_ERR_INDEX) {
        return efx_api_range_error(ctx, "clip index out of range");
    }
    if (rc != EFX_RENDER_OK) {
        return efx_api_generic_error(ctx, "poseMesh failed");
    }
    return JS_UNDEFINED;
}


JSValue efx_js_setCamera3D(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return efx_api_type_error(ctx, "setCamera3D requires an options object");
    }
    JSValueConst opts = argv[0];
    static const char *known[] = {"pos", "target", "fov", "near", "far"};
    if (efx_api_check_known_fields(ctx, opts, known, 5, "setCamera3D") != 0) {
        return JS_EXCEPTION;
    }
    efx_camera3d cam;
    memset(&cam, 0, sizeof(cam));
    cam.near_z = 0.1f;
    cam.far_z = 100.0f;

    static const char *vec_keys[] = {"pos", "target"};
    float *vec_outs[] = {cam.pos, cam.target};
    for (int i = 0; i < 2; i++) {
        JSValue v = JS_GetPropertyStr(ctx, opts, vec_keys[i]);
        if (JS_IsUndefined(v)) {
            JS_FreeValue(ctx, v);
            return efx_api_type_error(ctx, "setCamera3D requires pos and target");
        }
        float *buf = NULL;
        int len = 0;
        int rc = efx_api_read_number_array(ctx, v, &buf, &len, vec_keys[i]);
        JS_FreeValue(ctx, v);
        if (rc != 0) {
            return JS_EXCEPTION;
        }
        if (len != 3) {
            free(buf);
            return efx_api_range_error(ctx, "pos and target must hold 3 numbers");
        }
        memcpy(vec_outs[i], buf, sizeof(float) * 3);
        free(buf);
    }
    JSValue fv = JS_GetPropertyStr(ctx, opts, "fov");
    if (JS_IsUndefined(fv)) {
        JS_FreeValue(ctx, fv);
        return efx_api_type_error(ctx, "setCamera3D requires fov");
    }
    double d = 0;
    if (!JS_IsNumber(fv) || JS_ToFloat64(ctx, &d, fv) < 0) {
        JS_FreeValue(ctx, fv);
        return efx_api_type_error(ctx, "fov must be a number");
    }
    JS_FreeValue(ctx, fv);
    if (!isfinite(d)) {
        return efx_api_range_error(ctx, "fov must be finite");
    }
    cam.fov = (float)d;

    static const char *opt_keys[] = {"near", "far"};
    float *opt_outs[] = {&cam.near_z, &cam.far_z};
    for (int i = 0; i < 2; i++) {
        JSValue v = JS_GetPropertyStr(ctx, opts, opt_keys[i]);
        if (!JS_IsUndefined(v)) {
            if (!JS_IsNumber(v) || JS_ToFloat64(ctx, &d, v) < 0) {
                JS_FreeValue(ctx, v);
                return efx_api_type_error(ctx, "near and far must be numbers");
            }
            if (!isfinite(d)) {
                JS_FreeValue(ctx, v);
                return efx_api_range_error(ctx, "near and far must be finite");
            }
            *opt_outs[i] = (float)d;
        }
        JS_FreeValue(ctx, v);
    }
    efx_render_set_camera3d(&cam);
    return JS_UNDEFINED;
}
