#include "api/api_internal.h"

/* desktop twin of the web wire reader (src/web/bridge_particles.c); the
 * layout comment there applies here too */
#define EFX_PART_WIRE_LEN 352

static void wire_particle_config(const float *w, uint64_t texture,
                                 efx_particle_config *c) {
    memset(c, 0, sizeof(*c));
    c->texture = texture;
    c->max = (int)w[0];
    c->space = (int)w[1];
    c->facing = (int)w[2];
    c->blend = (int)w[3];
    c->life_min = w[4];
    c->life_max = w[5];
    c->emission_rate = w[6];
    c->emitter_lifetime = w[7];
    c->speed_scale = w[8];
    c->spread = w[9];
    c->size_count = (int)w[10];
    c->size_variation = w[11];
    c->color_count = (int)w[12];
    c->relative_rotation = (int)w[13];
    c->shape = (int)w[14];
    c->quad_count = (int)w[15];
    c->rotation_min = w[16];
    c->rotation_max = w[17];
    c->spin_start = w[18];
    c->spin_end = w[19];
    c->spin_variation = w[20];
    for (int i = 0; i < 3; i++) c->position[i] = w[21 + i];
    for (int i = 0; i < 3; i++) c->direction[i] = w[24 + i];
    c->speed_min = w[27];
    c->speed_max = w[28];
    for (int i = 0; i < 3; i++) c->gravity[i] = w[29 + i];
    for (int i = 0; i < 3; i++) c->lin_acc_min[i] = w[32 + i];
    for (int i = 0; i < 3; i++) c->lin_acc_max[i] = w[35 + i];
    c->radial_acc_min = w[38];
    c->radial_acc_max = w[39];
    c->tangential_acc_min = w[40];
    c->tangential_acc_max = w[41];
    c->damping_min = w[42];
    c->damping_max = w[43];
    for (int i = 0; i < 8; i++) c->sizes[i] = w[44 + i];
    for (int i = 0; i < 8; i++) {
        for (int k = 0; k < 4; k++) c->colors[i][k] = w[52 + i * 4 + k];
    }
    for (int i = 0; i < 3; i++) c->shape_size[i] = w[84 + i];
    for (int i = 0; i < 64; i++) {
        for (int k = 0; k < 4; k++) c->quads[i][k] = w[87 + i * 4 + k];
    }
    for (int i = 0; i < 3; i++) c->normal[i] = w[343 + i];
    c->insert_mode = (int)w[346];
}



static JSValue efx_js_ps_emit(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv) {
    efxjs_particlesystem *p = efx_api_get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    if (argc < 1) return efx_api_type_error(ctx, "emit requires a count");
    int32_t n = 0;
    if (JS_ToInt32(ctx, &n, argv[0]) < 0 || n < 0) {
        return efx_api_range_error(ctx, "emit count must be a non-negative integer");
    }
    int rc = efx_render_particles_emit(p->handle, n);
    if (rc != EFX_RENDER_OK) return efx_api_generic_error(ctx, "emit failed");
    return JS_UNDEFINED;
}


static JSValue efx_js_ps_start(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    (void)argc; (void)argv;
    efxjs_particlesystem *p = efx_api_get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    efx_render_particles_start(p->handle);
    return JS_UNDEFINED;
}


static JSValue efx_js_ps_stop(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv) {
    (void)argc; (void)argv;
    efxjs_particlesystem *p = efx_api_get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    efx_render_particles_stop(p->handle);
    return JS_UNDEFINED;
}


static JSValue efx_js_ps_pause(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    (void)argc; (void)argv;
    efxjs_particlesystem *p = efx_api_get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    efx_render_particles_pause(p->handle);
    return JS_UNDEFINED;
}


static JSValue efx_js_ps_reset(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    (void)argc; (void)argv;
    efxjs_particlesystem *p = efx_api_get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    efx_render_particles_reset(p->handle);
    return JS_UNDEFINED;
}


/* native set from the prelude's normalized wire (ADR 0049): the merged bag
 * was validated and marshalled by the shared prelude validator */
JSValue efx_js_ps_set_wire(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) {
        return efx_api_type_error(ctx, "particle wire set native requires (system, wire, texture)");
    }
    efxjs_particlesystem *p = efx_api_get_live_ps(ctx, argv[0]);
    if (!p) {
        return JS_EXCEPTION;
    }
    size_t blen = 0;
    uint8_t *bytes = NULL;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[1], NULL, NULL, NULL);
    if (JS_IsException(ab)) {
        return ab;
    }
    bytes = JS_GetArrayBuffer(ctx, &blen, ab);
    JS_FreeValue(ctx, ab);
    if (!bytes || blen < EFX_PART_WIRE_LEN * sizeof(float)) {
        return efx_api_type_error(ctx, "particle wire must be a Float32Array(352)");
    }
    double tex = 0;
    if (JS_ToFloat64(ctx, &tex, argv[2]) < 0) {
        return JS_EXCEPTION;
    }
    efx_particle_config c;
    wire_particle_config((const float *)bytes, (uint64_t)tex, &c);
    int rc = efx_render_particles_set(p->handle, &c);
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return efx_api_type_error(ctx, "expected a live ParticleSystem");
    }
    if (rc == EFX_RENDER_ERR_SIZE) {
        return efx_api_range_error(ctx, "invalid particle configuration");
    }
    if (rc != EFX_RENDER_OK) {
        return efx_api_generic_error(ctx, "set failed");
    }
    return JS_UNDEFINED;
}


/* the ParticleSystem prototype, so the prelude can install its shared
 * `set` wrapper over it (ADR 0049) */
JSValue efx_js_ps_proto(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_GetClassProto(ctx, particlesystem_class_id);
}


static JSValue efx_js_ps_getCount(JSContext *ctx, JSValueConst this_val) {
    efxjs_particlesystem *p = efx_api_get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    return JS_NewInt32(ctx, efx_render_particles_count(p->handle));
}


static JSValue efx_js_ps_getSpeedScale(JSContext *ctx, JSValueConst this_val) {
    efxjs_particlesystem *p = efx_api_get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    return JS_NewFloat64(ctx, (double)efx_render_particles_speed_scale(p->handle));
}


static JSValue efx_js_ps_setSpeedScale(JSContext *ctx, JSValueConst this_val,
                                       JSValueConst val) {
    efxjs_particlesystem *p = efx_api_get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    double d;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d) || d <= 0) {
        return efx_api_range_error(ctx, "speedScale must be a finite number > 0");
    }
    efx_render_particles_set_speed_scale(p->handle, (float)d);
    return JS_UNDEFINED;
}


_Static_assert(sizeof(particlesystem_proto_funcs) / sizeof((particlesystem_proto_funcs)[0]) == 7,
                "particlesystem_proto_funcs must match the api_internal.h declaration");
const JSCFunctionListEntry particlesystem_proto_funcs[] = {
    JS_CFUNC_DEF("emit", 1, efx_js_ps_emit),
    JS_CFUNC_DEF("start", 0, efx_js_ps_start),
    JS_CFUNC_DEF("stop", 0, efx_js_ps_stop),
    JS_CFUNC_DEF("pause", 0, efx_js_ps_pause),
    JS_CFUNC_DEF("reset", 0, efx_js_ps_reset),
    JS_CGETSET_DEF("count", efx_js_ps_getCount, NULL),
    JS_CGETSET_DEF("speedScale", efx_js_ps_getSpeedScale,
                   efx_js_ps_setSpeedScale),
};



/* --------------------------------------------------- F11 particle/billboard bindings */

/* native create from the prelude's normalized wire (ADR 0049): the option
 * bag was validated and marshalled by the shared prelude validator */
JSValue efx_js_create_particle_system_wire(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return efx_api_type_error(ctx, "particle wire native requires (wire, texture)");
    }
    size_t blen = 0;
    uint8_t *bytes = NULL;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[0], NULL, NULL, NULL);
    if (JS_IsException(ab)) {
        return ab;
    }
    bytes = JS_GetArrayBuffer(ctx, &blen, ab);
    JS_FreeValue(ctx, ab);
    if (!bytes || blen < EFX_PART_WIRE_LEN * sizeof(float)) {
        return efx_api_type_error(ctx, "particle wire must be a Float32Array(352)");
    }
    double tex = 0;
    if (JS_ToFloat64(ctx, &tex, argv[1]) < 0) {
        return JS_EXCEPTION;
    }
    efx_particle_config c;
    wire_particle_config((const float *)bytes, (uint64_t)tex, &c);
    int err = 0;
    uint64_t h = efx_render_particles_create(&c, &err);
    if (!h) {
        if (err == EFX_RENDER_ERR_SIZE) {
            return efx_api_range_error(ctx, "invalid particle configuration");
        }
        return efx_api_generic_error(ctx, "createParticleSystem failed");
    }
    efxjs_particlesystem *p = calloc(1, sizeof(*p));
    if (!p) {
        efx_render_particles_destroy(h);
        return efx_api_generic_error(ctx, "out of memory");
    }
    p->handle = h;
    p->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, particlesystem_class_id);
    JS_SetOpaque(obj, p);
    return obj;
}


JSValue efx_js_drawParticles(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "drawParticles requires a ParticleSystem");
    }
    efxjs_particlesystem *p = efx_api_get_live_ps(ctx, argv[0]);
    if (!p) {
        return JS_EXCEPTION;
    }
    int rc = efx_render_particles_draw(p->handle);
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return efx_api_type_error(ctx, "expected a live ParticleSystem");
    }
    if (rc == EFX_RENDER_ERR_BUDGET) {
        return efx_api_range_error(ctx, "display list budget exceeded");
    }
    if (rc == EFX_RENDER_ERR_FEEDBACK) {
        return efx_api_type_error(ctx, "cannot sample the render target being drawn into");
    }
    if (rc != EFX_RENDER_OK) {
        return efx_api_generic_error(ctx, "drawParticles failed");
    }
    return JS_UNDEFINED;
}


static int read_billboard_pos(JSContext *ctx, JSValueConst v, float pos[3]) {
    if (!JS_IsArray(v)) {
        efx_api_type_error(ctx, "drawBillboard pos must be [x,y,z]");
        return -1;
    }
    JSValue lv = JS_GetPropertyStr(ctx, v, "length");
    int32_t ln = -1;
    JS_ToInt32(ctx, &ln, lv);
    JS_FreeValue(ctx, lv);
    if (ln != 3) {
        efx_api_type_error(ctx, "drawBillboard pos must be [x,y,z]");
        return -1;
    }
    if (efx_api_get_float_array(ctx, v, pos, 3) != 0) {
        return -1;
    }
    return 0;
}

static int read_billboard_size(JSContext *ctx, JSValueConst opts, float *out_w,
                               float *out_h) {
    float w = 1.0f, h = 1.0f;
    JSValue zv = JS_GetPropertyStr(ctx, opts, "size");
    if (!JS_IsUndefined(zv)) {
        if (JS_IsArray(zv)) {
            float sz[2];
            if (efx_api_get_float_array(ctx, zv, sz, 2) != 0) {
                JS_FreeValue(ctx, zv);
                return -1;
            }
            w = sz[0];
            h = sz[1];
        } else {
            double d;
            if (JS_ToFloat64(ctx, &d, zv) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, zv);
                efx_api_type_error(ctx, "size must be a number or [w,h]");
                return -1;
            }
            w = h = (float)d;
        }
    }
    JS_FreeValue(ctx, zv);
    if (!(w > 0) || !(h > 0)) {
        efx_api_range_error(ctx, "size entries must be > 0");
        return -1;
    }
    *out_w = w;
    *out_h = h;
    return 0;
}

static int read_billboard_facing(JSContext *ctx, JSValueConst opts, int *out) {
    int facing = EFX_FACING_VIEW;
    JSValue fv = JS_GetPropertyStr(ctx, opts, "facing");
    if (!JS_IsUndefined(fv)) {
        const char *s = JS_ToCString(ctx, fv);
        if (s && !strcmp(s, "view")) facing = EFX_FACING_VIEW;
        else if (s && !strcmp(s, "y")) facing = EFX_FACING_Y;
        else if (s && !strcmp(s, "plane")) facing = EFX_FACING_PLANE;
        else {
            if (s) JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, fv);
            efx_api_type_error(ctx, "facing must be 'view', 'y', or 'plane'");
            return -1;
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, fv);
    *out = facing;
    return 0;
}

JSValue efx_js_drawBillboard(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return efx_api_type_error(ctx, "drawBillboard requires (texture, pos, opts?)");
    }
    JSValueConst tv = argv[0];
    if (JS_IsUndefined(tv)) {
        return efx_api_type_error(ctx, "drawBillboard requires a texture");
    }
    uint64_t tex = 0;
    if (efx_api_get_live_sample(ctx, tv, &tex) != 0) {
        return JS_EXCEPTION;
    }
    float pos[3];
    if (read_billboard_pos(ctx, argv[1], pos) != 0) {
        return JS_EXCEPTION;
    }
    JSValueConst opts = JS_UNDEFINED;
    if (argc >= 3 && !JS_IsUndefined(argv[2])) {
        if (!JS_IsObject(argv[2])) {
            return efx_api_type_error(ctx, "drawBillboard options must be an object");
        }
        opts = argv[2];
        static const char *known[] = {"size",     "color",     "sourceRect",
                                      "rotation", "facing",  "depthTest", "normal",
                                      "blend"};
        if (efx_api_check_known_fields(ctx, opts, known, 8, "drawBillboard") != 0) {
            return JS_EXCEPTION;
        }
    }

    float w = 1.0f, h = 1.0f;
    float color[4] = {1, 1, 1, 1};
    float rotation = 0.0f;
    float normal[3] = {0, 1, 0};
    int facing = EFX_FACING_VIEW;
    int depth_test = 1;
    int blend = EFX_BLEND_INHERIT;
    float src[4] = {0, 0, 0, 0};
    int has_src = 0;

    if (read_billboard_size(ctx, opts, &w, &h) != 0) {
        return JS_EXCEPTION;
    }

    JSValue cv = JS_GetPropertyStr(ctx, opts, "color");
    if (!JS_IsUndefined(cv)) {
        if (efx_api_get_float_array(ctx, cv, color, 4) != 0) {
            JS_FreeValue(ctx, cv);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, cv);

    JSValue rv = JS_GetPropertyStr(ctx, opts, "rotation");
    if (!JS_IsUndefined(rv)) {
        double d;
        if (JS_ToFloat64(ctx, &d, rv) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, rv);
            return efx_api_type_error(ctx, "rotation must be a finite number");
        }
        rotation = (float)d;
    }
    JS_FreeValue(ctx, rv);

    if (read_billboard_facing(ctx, opts, &facing) != 0) {
        return JS_EXCEPTION;
    }

    JSValue nv = JS_GetPropertyStr(ctx, opts, "normal");
    if (!JS_IsUndefined(nv)) {
        if (efx_api_get_float_array(ctx, nv, normal, 3) != 0) {
            JS_FreeValue(ctx, nv);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, nv);

    JSValue dv = JS_GetPropertyStr(ctx, opts, "depthTest");
    if (!JS_IsUndefined(dv)) {
        if (!JS_IsBool(dv)) {
            JS_FreeValue(ctx, dv);
            return efx_api_type_error(ctx, "depthTest must be a boolean");
        }
        depth_test = JS_ToBool(ctx, dv);
    }
    JS_FreeValue(ctx, dv);

    JSValue sv = JS_GetPropertyStr(ctx, opts, "sourceRect");
    if (!JS_IsUndefined(sv)) {
        if (efx_api_read_source_rect(ctx, tex, sv, src, &has_src) != 0) {
            JS_FreeValue(ctx, sv);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, sv);

    JSValue bv = JS_GetPropertyStr(ctx, opts, "blend");
    if (!JS_IsUndefined(bv)) {
        if (efx_api_read_blend(ctx, bv, &blend) != 0) {
            JS_FreeValue(ctx, bv);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, bv);

    int rc = efx_render_billboard(tex, pos, w, h, color, rotation, facing, normal,
                                  depth_test, src, has_src, blend);
    if (rc == EFX_RENDER_ERR_HANDLE) {
        return efx_api_type_error(ctx, "expected a live Texture or RenderTarget");
    }
    if (rc == EFX_RENDER_ERR_SIZE) {
        return efx_api_range_error(ctx, "invalid billboard size or facing");
    }
    if (rc == EFX_RENDER_ERR_BUDGET) {
        return efx_api_range_error(ctx, "display list budget exceeded");
    }
    if (rc != EFX_RENDER_OK) {
        return efx_api_generic_error(ctx, "drawBillboard failed");
    }
    return JS_UNDEFINED;
}


static int parse_sprite_pos(JSContext *ctx, JSValueConst e, sprite_params *s) {
    JSValue xv = JS_GetPropertyStr(ctx, e, "x");
    JSValue yv = JS_GetPropertyStr(ctx, e, "y");
    double x = 0, y = 0;
    int bad = JS_ToFloat64(ctx, &x, xv) < 0 || JS_ToFloat64(ctx, &y, yv) < 0 ||
              !isfinite(x) || !isfinite(y);
    JS_FreeValue(ctx, xv);
    JS_FreeValue(ctx, yv);
    if (bad) {
        efx_api_type_error(ctx, "sprite x and y must be finite numbers");
        return -1;
    }
    s->x = (float)x;
    s->y = (float)y;
    return 0;
}

static int parse_sprite_transform(JSContext *ctx, JSValueConst e,
                                  sprite_params *s) {
    JSValue cv = JS_GetPropertyStr(ctx, e, "color");
    if (!JS_IsUndefined(cv)) {
        if (efx_api_get_float_array(ctx, cv, s->color, 4) != 0) {
            JS_FreeValue(ctx, cv);
            return -1;
        }
    }
    JS_FreeValue(ctx, cv);

    JSValue rv = JS_GetPropertyStr(ctx, e, "rotation");
    if (!JS_IsUndefined(rv)) {
        double d;
        if (JS_ToFloat64(ctx, &d, rv) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, rv);
            efx_api_type_error(ctx, "rotation must be a finite number");
            return -1;
        }
        s->rotation = (float)d;
    }
    JS_FreeValue(ctx, rv);

    JSValue scv = JS_GetPropertyStr(ctx, e, "scale");
    if (!JS_IsUndefined(scv)) {
        double d;
        if (JS_ToFloat64(ctx, &d, scv) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, scv);
            efx_api_type_error(ctx, "scale must be a finite number");
            return -1;
        }
        if (d <= 0) {
            JS_FreeValue(ctx, scv);
            efx_api_range_error(ctx, "scale must be > 0");
            return -1;
        }
        s->scale = (float)d;
    }
    JS_FreeValue(ctx, scv);
    return 0;
}

static int parse_sprite_size(JSContext *ctx, JSValueConst e, float size[2],
                             int *has_size) {
    size[0] = size[1] = 0;
    *has_size = 0;
    JSValue zv = JS_GetPropertyStr(ctx, e, "size");
    if (!JS_IsUndefined(zv)) {
        if (efx_api_get_float_array(ctx, zv, size, 2) != 0) {
            JS_FreeValue(ctx, zv);
            return -1;
        }
        if (size[0] <= 0 || size[1] <= 0) {
            JS_FreeValue(ctx, zv);
            efx_api_range_error(ctx, "size entries must be > 0");
            return -1;
        }
        *has_size = 1;
    }
    JS_FreeValue(ctx, zv);
    return 0;
}

static int parse_sprite_origin(JSContext *ctx, JSValueConst e,
                               sprite_params *s) {
    JSValue ov = JS_GetPropertyStr(ctx, e, "origin");
    if (!JS_IsUndefined(ov)) {
        if (efx_api_get_float_array(ctx, ov, s->origin, 2) != 0) {
            JS_FreeValue(ctx, ov);
            return -1;
        }
        s->has_origin = 1;
    }
    JS_FreeValue(ctx, ov);
    return 0;
}

/* validate + parse one sprite entry exactly as drawQuad parses its options */
static int parse_sprite(JSContext *ctx, uint64_t tex, JSValueConst e,
                        sprite_params *s) {
    if (!JS_IsObject(e)) {
        efx_api_type_error(ctx, "each sprite must be an object");
        return -1;
    }
    static const char *known[] = {"x",     "y",      "size", "color",
                                  "rotation", "scale", "sourceRect",
                                  "origin"};
    if (efx_api_check_known_fields(ctx, e, known, 8, "drawSprites") != 0) {
        return -1;
    }
    s->color[0] = s->color[1] = s->color[2] = s->color[3] = 1.0f;
    s->rotation = 0.0f;
    s->scale = 1.0f;
    s->has_src = 0;
    s->has_origin = 0;

    if (parse_sprite_pos(ctx, e, s) != 0) {
        return -1;
    }
    if (parse_sprite_transform(ctx, e, s) != 0) {
        return -1;
    }

    float size[2];
    int has_size = 0;
    if (parse_sprite_size(ctx, e, size, &has_size) != 0) {
        return -1;
    }
    if (parse_sprite_origin(ctx, e, s) != 0) {
        return -1;
    }

    JSValue srcv = JS_GetPropertyStr(ctx, e, "sourceRect");
    if (!JS_IsUndefined(srcv)) {
        if (efx_api_read_source_rect(ctx, tex, srcv, s->src,
                                     &s->has_src) != 0) {
            return -1;
        }
    }

    if (has_size) {
        s->w = size[0];
        s->h = size[1];
    } else if (s->has_src) {
        s->w = s->src[2];
        s->h = s->src[3];
    } else {
        int tw = 0, th = 0;
        efx_render_sample_size(tex, &tw, &th);
        s->w = (float)tw;
        s->h = (float)th;
    }
    return 0;
}


JSValue efx_js_drawSprites(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return efx_api_type_error(ctx, "drawSprites requires (texture, sprites)");
    }
    uint64_t tex = 0;
    if (efx_api_get_live_sample(ctx, argv[0], &tex) != 0) {
        return JS_EXCEPTION;
    }
    if (!JS_IsArray(argv[1])) {
        return efx_api_type_error(ctx, "sprites must be an array");
    }
    int blend = EFX_BLEND_INHERIT;
    if (argc >= 3 && !JS_IsUndefined(argv[2])) {
        if (!JS_IsObject(argv[2]) || JS_IsArray(argv[2])) {
            return efx_api_type_error(ctx, "drawSprites options must be an object");
        }
        static const char *known[] = {"blend"};
        if (efx_api_check_known_fields(ctx, argv[2], known, 1, "drawSprites") != 0) {
            return JS_EXCEPTION;
        }
        JSValue bv = JS_GetPropertyStr(ctx, argv[2], "blend");
        if (!JS_IsUndefined(bv)) {
            if (efx_api_read_blend(ctx, bv, &blend) != 0) {
                JS_FreeValue(ctx, bv);
                return JS_EXCEPTION;
            }
        }
        JS_FreeValue(ctx, bv);
    }
    JSValue lv = JS_GetPropertyStr(ctx, argv[1], "length");
    int32_t n = 0;
    JS_ToInt32(ctx, &n, lv);
    JS_FreeValue(ctx, lv);
    if (n <= 0) {
        return JS_UNDEFINED;
    }
    sprite_params *items = calloc((size_t)n, sizeof(*items));
    if (!items) {
        return efx_api_generic_error(ctx, "out of memory");
    }
    /* validate every entry before recording any (atomic) */
    for (int i = 0; i < n; i++) {
        JSValue e = JS_GetPropertyUint32(ctx, argv[1], (uint32_t)i);
        int rc = parse_sprite(ctx, tex, e, &items[i]);
        JS_FreeValue(ctx, e);
        if (rc != 0) {
            free(items);
            return JS_EXCEPTION;
        }
    }
    for (int i = 0; i < n; i++) {
        sprite_params *s = &items[i];
        float ox = s->has_origin ? s->origin[0] : s->w * 0.5f;
        float oy = s->has_origin ? s->origin[1] : s->h * 0.5f;
        int rc = efx_render_quad(s->x, s->y, s->w, s->h, tex, s->color,
                                 s->rotation, s->scale, s->src, s->has_src, ox,
                                 oy, blend);
        if (rc == EFX_RENDER_ERR_BUDGET) {
            free(items);
            return efx_api_range_error(ctx, "display list budget exceeded");
        }
        if (rc != EFX_RENDER_OK) {
            free(items);
            return efx_api_generic_error(ctx, "drawSprites failed");
        }
    }
    free(items);
    return JS_UNDEFINED;
}

