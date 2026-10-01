#include "api/api_internal.h"


/* read a value as an [x,y] or [x,y,z] float vector; 0 absent, 1 set, -1 err */
static int vec_from_value(JSContext *ctx, JSValueConst v, float out[3],
                          int allow2) {
    float tmp[3] = {0, 0, 0};
    int n = 3;
    if (!JS_IsArray(v)) {
        efx_api_type_error(ctx, "expected an array");
        return -1;
    }
    JSValue lv = JS_GetPropertyStr(ctx, v, "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lv);
    JS_FreeValue(ctx, lv);
    if (allow2 && len == 2) {
        n = 2;
    } else if (len != 3) {
        efx_api_type_error(ctx, "expected a [x,y] or [x,y,z] array");
        return -1;
    }
    if (efx_api_get_float_array(ctx, v, tmp, n) != 0) {
        return -1;
    }
    for (int i = 0; i < n; i++) out[i] = tmp[i];
    return 1;
}


/* optional vector field: 0 absent, 1 set, -1 error */
static int pcfg_vec(JSContext *ctx, JSValueConst o, const char *k, float out[3],
                    int allow2) {
    JSValue v = JS_GetPropertyStr(ctx, o, k);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    int r = vec_from_value(ctx, v, out, allow2);
    JS_FreeValue(ctx, v);
    return r;
}


/* scalar field: 0 absent, 1 set, -1 error */
static int pcfg_num(JSContext *ctx, JSValueConst o, const char *k, float *out) {
    double d;
    int r = efx_api_opt_number(ctx, o, k, &d, "option must be a finite number");
    if (r == 1) {
        *out = (float)d;
    }
    return r;
}


/* number or [min,max]: 0 absent, 1 set, -1 error */
static int pcfg_range(JSContext *ctx, JSValueConst o, const char *k, float *lo,
                      float *hi) {
    JSValue v = JS_GetPropertyStr(ctx, o, k);
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (JS_IsArray(v)) {
        float t[2];
        if (efx_api_get_float_array(ctx, v, t, 2) != 0) {
            JS_FreeValue(ctx, v);
            return -1;
        }
        *lo = t[0];
        *hi = t[1];
    } else {
        double d;
        if (JS_ToFloat64(ctx, &d, v) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, v);
            efx_api_type_error(ctx, "expected a number or [min,max]");
            return -1;
        }
        *lo = *hi = (float)d;
    }
    JS_FreeValue(ctx, v);
    return 1;
}


static int pcfg_sizes(JSContext *ctx, JSValueConst o,
                      efx_particle_config *c) {
    JSValue v = JS_GetPropertyStr(ctx, o, "sizes");
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (JS_IsArray(v)) {
        JSValue lv = JS_GetPropertyStr(ctx, v, "length");
        int32_t n = -1;
        JS_ToInt32(ctx, &n, lv);
        JS_FreeValue(ctx, lv);
        if (n < 1 || n > 8) {
            JS_FreeValue(ctx, v);
            efx_api_range_error(ctx, "sizes must hold 1..8 entries");
            return -1;
        }
        for (int i = 0; i < n; i++) {
            JSValue e = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
            double d;
            if (JS_ToFloat64(ctx, &d, e) < 0 || !isfinite(d) || d <= 0) {
                JS_FreeValue(ctx, e);
                JS_FreeValue(ctx, v);
                efx_api_range_error(ctx, "sizes must be finite and > 0");
                return -1;
            }
            JS_FreeValue(ctx, e);
            c->sizes[i] = (float)d;
        }
        c->size_count = n;
    } else {
        double d;
        if (JS_ToFloat64(ctx, &d, v) < 0 || !isfinite(d) || d <= 0) {
            JS_FreeValue(ctx, v);
            efx_api_range_error(ctx, "size must be finite and > 0");
            return -1;
        }
        c->sizes[0] = (float)d;
        c->size_count = 1;
    }
    JS_FreeValue(ctx, v);
    return 1;
}


static int pcfg_colors(JSContext *ctx, JSValueConst o,
                       efx_particle_config *c) {
    JSValue v = JS_GetPropertyStr(ctx, o, "colors");
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (!JS_IsArray(v)) {
        JS_FreeValue(ctx, v);
        efx_api_type_error(ctx, "colors must be a color or an array of colors");
        return -1;
    }
    JSValue first = JS_GetPropertyUint32(ctx, v, 0);
    int is_list = JS_IsArray(first);
    JS_FreeValue(ctx, first);
    if (is_list) {
        JSValue lv = JS_GetPropertyStr(ctx, v, "length");
        int32_t n = -1;
        JS_ToInt32(ctx, &n, lv);
        JS_FreeValue(ctx, lv);
        if (n < 1 || n > 8) {
            JS_FreeValue(ctx, v);
            efx_api_range_error(ctx, "colors must hold 1..8 entries");
            return -1;
        }
        for (int i = 0; i < n; i++) {
            JSValue e = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
            if (efx_api_get_float_array(ctx, e, c->colors[i], 4) != 0) {
                JS_FreeValue(ctx, e);
                JS_FreeValue(ctx, v);
                return -1;
            }
            JS_FreeValue(ctx, e);
        }
        c->color_count = n;
    } else {
        if (efx_api_get_float_array(ctx, v, c->colors[0], 4) != 0) {
            JS_FreeValue(ctx, v);
            return -1;
        }
        c->color_count = 1;
    }
    JS_FreeValue(ctx, v);
    return 1;
}


static int pcfg_quads(JSContext *ctx, JSValueConst o,
                      efx_particle_config *c) {
    JSValue v = JS_GetPropertyStr(ctx, o, "quads");
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (!JS_IsArray(v)) {
        JS_FreeValue(ctx, v);
        efx_api_type_error(ctx, "quads must be an array");
        return -1;
    }
    JSValue lv = JS_GetPropertyStr(ctx, v, "length");
    int32_t n = -1;
    JS_ToInt32(ctx, &n, lv);
    JS_FreeValue(ctx, lv);
    if (n < 0 || n > 64) {
        JS_FreeValue(ctx, v);
        efx_api_range_error(ctx, "quads must hold at most 64 entries");
        return -1;
    }
    for (int i = 0; i < n; i++) {
        JSValue e = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
        float rect[4];
        if (JS_IsArray(e)) {
            if (efx_api_get_float_array(ctx, e, rect, 4) != 0) {
                JS_FreeValue(ctx, e);
                JS_FreeValue(ctx, v);
                return -1;
            }
        } else if (JS_IsObject(e)) {
            static const char *rk[] = {"x", "y", "w", "h"};
            for (int k = 0; k < 4; k++) {
                JSValue f = JS_GetPropertyStr(ctx, e, rk[k]);
                double d;
                if (JS_ToFloat64(ctx, &d, f) < 0 || !isfinite(d)) {
                    JS_FreeValue(ctx, f);
                    JS_FreeValue(ctx, e);
                    JS_FreeValue(ctx, v);
                    efx_api_type_error(ctx, "quad rect fields must be finite numbers");
                    return -1;
                }
                JS_FreeValue(ctx, f);
                rect[k] = (float)d;
            }
        } else {
            JS_FreeValue(ctx, e);
            JS_FreeValue(ctx, v);
            efx_api_type_error(ctx, "each quad must be an object or [x,y,w,h]");
            return -1;
        }
        JS_FreeValue(ctx, e);
        for (int k = 0; k < 4; k++) c->quads[i][k] = rect[k];
    }
    c->quad_count = n;
    JS_FreeValue(ctx, v);
    return 1;
}


static int pcfg_shape(JSContext *ctx, JSValueConst o,
                      efx_particle_config *c) {
    JSValue v = JS_GetPropertyStr(ctx, o, "emissionShape");
    if (JS_IsUndefined(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (!JS_IsObject(v)) {
        JS_FreeValue(ctx, v);
        efx_api_type_error(ctx, "emissionShape must be an object");
        return -1;
    }
    static const char *known[] = {"shape", "size"};
    if (efx_api_check_known_fields(ctx, v, known, 2, "emissionShape") != 0) {
        JS_FreeValue(ctx, v);
        return -1;
    }
    JSValue sv = JS_GetPropertyStr(ctx, v, "shape");
    if (!JS_IsUndefined(sv)) {
        const char *s = JS_ToCString(ctx, sv);
        int ok = 0;
        if (s) {
            if (!strcmp(s, "point")) { c->shape = EFX_SHAPE_POINT; ok = 1; }
            else if (!strcmp(s, "box")) { c->shape = EFX_PHYS_SHAPE_BOX; ok = 1; }
            else if (!strcmp(s, "sphere")) { c->shape = EFX_PHYS_SHAPE_SPHERE; ok = 1; }
            else if (!strcmp(s, "sphereSurface")) {
                c->shape = EFX_SHAPE_SPHERE_SURFACE; ok = 1;
            } else if (!strcmp(s, "disc")) { c->shape = EFX_SHAPE_DISC; ok = 1; }
            JS_FreeCString(ctx, s);
        }
        if (!ok) {
            JS_FreeValue(ctx, sv);
            JS_FreeValue(ctx, v);
            efx_api_type_error(ctx, "unknown emission shape");
            return -1;
        }
    }
    JS_FreeValue(ctx, sv);
    int r = pcfg_vec(ctx, v, "size", c->shape_size, 0);
    JS_FreeValue(ctx, v);
    return r < 0 ? -1 : 1;
}


/* parse the particle options over `c` (which the caller pre-fills). Unknown
 * fields throw; absent fields keep their current value. Returns 0/-1. */
static int read_particle_config(JSContext *ctx, JSValueConst opts,
                                efx_particle_config *c) {
    static const char *known[] = {
        "texture", "max", "space", "facing", "normal", "blend", "lifetime",
        "emissionRate", "emitterLifetime", "position", "direction", "spread",
        "speed", "gravity", "linearAcceleration", "radialAcceleration",
        "tangentialAcceleration", "linearDamping", "sizes", "sizeVariation",
        "colors", "rotation", "spin", "spinVariation", "relativeRotation",
        "emissionShape", "quads", "insertMode", "speedScale"};
    if (efx_api_check_known_fields(ctx, opts, known,
                           (int)(sizeof(known) / sizeof(known[0])),
                           "createParticleSystem") != 0) {
        return -1;
    }

    JSValue tv = JS_GetPropertyStr(ctx, opts, "texture");
    if (!JS_IsUndefined(tv)) {
        if (efx_api_get_live_sample(ctx, tv, &c->texture) != 0) {
            JS_FreeValue(ctx, tv);
            return -1;
        }
    }
    JS_FreeValue(ctx, tv);

    JSValue mv = JS_GetPropertyStr(ctx, opts, "max");
    if (!JS_IsUndefined(mv)) {
        int32_t n = 0;
        if (JS_ToInt32(ctx, &n, mv) < 0) {
            JS_FreeValue(ctx, mv);
            efx_api_type_error(ctx, "max must be an integer");
            return -1;
        }
        c->max = n;
    }
    JS_FreeValue(ctx, mv);

    JSValue sp = JS_GetPropertyStr(ctx, opts, "space");
    if (!JS_IsUndefined(sp)) {
        const char *s = JS_ToCString(ctx, sp);
        if (s && !strcmp(s, "screen")) c->space = EFX_SPACE_SCREEN;
        else if (s && !strcmp(s, "world")) c->space = EFX_SPACE_WORLD;
        else {
            if (s) JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, sp);
            efx_api_type_error(ctx, "space must be 'world' or 'screen'");
            return -1;
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, sp);

    JSValue fv = JS_GetPropertyStr(ctx, opts, "facing");
    if (!JS_IsUndefined(fv)) {
        const char *s = JS_ToCString(ctx, fv);
        if (s && !strcmp(s, "view")) c->facing = EFX_FACING_VIEW;
        else if (s && !strcmp(s, "y")) c->facing = EFX_FACING_Y;
        else if (s && !strcmp(s, "plane")) c->facing = EFX_FACING_PLANE;
        else {
            if (s) JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, fv);
            efx_api_type_error(ctx, "facing must be 'view', 'y', or 'plane'");
            return -1;
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, fv);

    if (c->space == EFX_SPACE_SCREEN && c->facing != EFX_FACING_VIEW) {
        efx_api_type_error(ctx, "facing must be 'view' for screen space");
        return -1;
    }

    if (pcfg_vec(ctx, opts, "normal", c->normal, 0) < 0) return -1;

    JSValue bv = JS_GetPropertyStr(ctx, opts, "blend");
    if (!JS_IsUndefined(bv)) {
        const char *s = JS_ToCString(ctx, bv);
        if (s && !strcmp(s, "alpha")) c->blend = EFX_BLEND_ALPHA;
        else if (s && !strcmp(s, "additive")) c->blend = EFX_BLEND_ADDITIVE;
        else if (s && !strcmp(s, "subtractive")) c->blend = EFX_BLEND_SUBTRACTIVE;
        else {
            if (s) JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, bv);
            efx_api_type_error(ctx, "blend must be 'alpha', 'additive', or 'subtractive'");
            return -1;
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, bv);

    if (pcfg_range(ctx, opts, "lifetime", &c->life_min, &c->life_max) < 0)
        return -1;
    float f;
    int r;
    if ((r = pcfg_num(ctx, opts, "emissionRate", &f)) < 0) return -1;
    if (r > 0) c->emission_rate = f;
    if ((r = pcfg_num(ctx, opts, "emitterLifetime", &f)) < 0) return -1;
    if (r > 0) c->emitter_lifetime = f;
    if (pcfg_vec(ctx, opts, "position", c->position, 1) < 0) return -1;
    if (pcfg_vec(ctx, opts, "direction", c->direction, 1) < 0) return -1;
    if ((r = pcfg_num(ctx, opts, "spread", &f)) < 0) return -1;
    if (r > 0) c->spread = f;
    if (pcfg_range(ctx, opts, "speed", &c->speed_min, &c->speed_max) < 0)
        return -1;
    if (pcfg_vec(ctx, opts, "gravity", c->gravity, 1) < 0) return -1;
    if (pcfg_range(ctx, opts, "radialAcceleration", &c->radial_acc_min,
                   &c->radial_acc_max) < 0)
        return -1;
    if (pcfg_range(ctx, opts, "tangentialAcceleration", &c->tangential_acc_min,
                   &c->tangential_acc_max) < 0)
        return -1;
    if (pcfg_range(ctx, opts, "linearDamping", &c->damping_min,
                   &c->damping_max) < 0)
        return -1;
    if (pcfg_sizes(ctx, opts, c) < 0) return -1;
    if ((r = pcfg_num(ctx, opts, "sizeVariation", &f)) < 0) return -1;
    if (r > 0) c->size_variation = f;
    if (pcfg_colors(ctx, opts, c) < 0) return -1;
    if (pcfg_range(ctx, opts, "rotation", &c->rotation_min, &c->rotation_max) < 0)
        return -1;
    if (pcfg_range(ctx, opts, "spin", &c->spin_start, &c->spin_end) < 0)
        return -1;
    if ((r = pcfg_num(ctx, opts, "spinVariation", &f)) < 0) return -1;
    if (r > 0) c->spin_variation = f;
    JSValue rrv = JS_GetPropertyStr(ctx, opts, "relativeRotation");
    if (!JS_IsUndefined(rrv)) {
        if (!JS_IsBool(rrv)) {
            JS_FreeValue(ctx, rrv);
            efx_api_type_error(ctx, "relativeRotation must be a boolean");
            return -1;
        }
        c->relative_rotation = JS_ToBool(ctx, rrv);
    }
    JS_FreeValue(ctx, rrv);
    if (pcfg_shape(ctx, opts, c) < 0) return -1;
    if (pcfg_quads(ctx, opts, c) < 0) return -1;

    JSValue im = JS_GetPropertyStr(ctx, opts, "insertMode");
    if (!JS_IsUndefined(im)) {
        const char *s = JS_ToCString(ctx, im);
        if (s && !strcmp(s, "top")) c->insert_mode = EFX_INSERT_TOP;
        else if (s && !strcmp(s, "bottom")) c->insert_mode = EFX_INSERT_BOTTOM;
        else if (s && !strcmp(s, "random")) c->insert_mode = EFX_INSERT_RANDOM;
        else {
            if (s) JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, im);
            efx_api_type_error(ctx, "insertMode must be 'top', 'bottom', or 'random'");
            return -1;
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, im);

    if ((r = pcfg_num(ctx, opts, "speedScale", &f)) < 0) return -1;
    if (r > 0) c->speed_scale = f;

    /* gravity/linear acceleration are the same concept for a particle */
    r = pcfg_vec(ctx, opts, "linearAcceleration", c->lin_acc_min, 0);
    if (r < 0) return -1;
    if (r > 0) {
        for (int i = 0; i < 3; i++) c->lin_acc_max[i] = c->lin_acc_min[i];
    }
    return 0;
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


static JSValue efx_js_ps_set(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    efxjs_particlesystem *p = efx_api_get_live_ps(ctx, this_val);
    if (!p) return JS_EXCEPTION;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return efx_api_type_error(ctx, "set requires an options object");
    }
    efx_particle_config cfg;
    efx_render_particles_config(p->handle, &cfg);
    if (read_particle_config(ctx, argv[0], &cfg) != 0) {
        return JS_EXCEPTION;
    }
    int rc = efx_render_particles_set(p->handle, &cfg);
    if (rc == EFX_RENDER_ERR_HANDLE) return efx_api_type_error(ctx, "expected a live ParticleSystem");
    if (rc == EFX_RENDER_ERR_SIZE) return efx_api_range_error(ctx, "invalid particle configuration");
    if (rc != EFX_RENDER_OK) return efx_api_generic_error(ctx, "set failed");
    return JS_UNDEFINED;
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


const JSCFunctionListEntry particlesystem_proto_funcs[] = {
    JS_CFUNC_DEF("emit", 1, efx_js_ps_emit),
    JS_CFUNC_DEF("start", 0, efx_js_ps_start),
    JS_CFUNC_DEF("stop", 0, efx_js_ps_stop),
    JS_CFUNC_DEF("pause", 0, efx_js_ps_pause),
    JS_CFUNC_DEF("reset", 0, efx_js_ps_reset),
    JS_CFUNC_DEF("set", 1, efx_js_ps_set),
    JS_CGETSET_DEF("count", efx_js_ps_getCount, NULL),
    JS_CGETSET_DEF("speedScale", efx_js_ps_getSpeedScale,
                   efx_js_ps_setSpeedScale),
};


/* --------------------------------------------------- F11 particle/billboard bindings */

JSValue efx_js_createParticleSystem(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return efx_api_type_error(ctx, "createParticleSystem requires an options object");
    }
    /* required fields throw TypeError when absent (web-binding parity);
       range problems are raised by the engine validation below */
    JSValue rq = JS_GetPropertyStr(ctx, argv[0], "max");
    int has_max = !JS_IsUndefined(rq);
    JS_FreeValue(ctx, rq);
    if (!has_max) {
        return efx_api_type_error(ctx, "createParticleSystem requires max");
    }
    rq = JS_GetPropertyStr(ctx, argv[0], "lifetime");
    int has_life = !JS_IsUndefined(rq);
    JS_FreeValue(ctx, rq);
    if (!has_life) {
        return efx_api_type_error(ctx, "createParticleSystem requires lifetime");
    }
    efx_particle_config c;
    memset(&c, 0, sizeof(c));
    c.space = EFX_SPACE_WORLD;
    c.facing = EFX_FACING_VIEW;
    c.blend = EFX_BLEND_ALPHA;
    c.normal[1] = 1.0f;
    c.life_min = c.life_max = 1.0f;
    c.emitter_lifetime = -1.0f;
    c.direction[1] = 1.0f;
    c.size_count = 1;
    c.sizes[0] = 1.0f;
    c.color_count = 1;
    c.colors[0][0] = c.colors[0][1] = c.colors[0][2] = c.colors[0][3] = 1.0f;
    c.shape = EFX_SHAPE_POINT;
    c.insert_mode = EFX_INSERT_TOP;
    c.speed_scale = 1.0f;
    if (read_particle_config(ctx, argv[0], &c) != 0) {
        return JS_EXCEPTION;
    }
    if (!c.texture) {
        return efx_api_type_error(ctx, "createParticleSystem requires a texture");
    }
    if (c.max <= 0) {
        return efx_api_range_error(ctx, "createParticleSystem requires a positive max");
    }
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


JSValue efx_js_drawBillboard(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) {
        return efx_api_type_error(ctx, "drawBillboard requires (pos, opts)");
    }
    float pos[3];
    if (!JS_IsArray(argv[0])) {
        return efx_api_type_error(ctx, "drawBillboard pos must be [x,y,z]");
    }
    {
        JSValue lv = JS_GetPropertyStr(ctx, argv[0], "length");
        int32_t ln = -1;
        JS_ToInt32(ctx, &ln, lv);
        JS_FreeValue(ctx, lv);
        if (ln != 3) {
            return efx_api_type_error(ctx, "drawBillboard pos must be [x,y,z]");
        }
    }
    if (efx_api_get_float_array(ctx, argv[0], pos, 3) != 0) {
        return JS_EXCEPTION;
    }
    if (!JS_IsObject(argv[1])) {
        return efx_api_type_error(ctx, "drawBillboard options must be an object");
    }
    JSValueConst opts = argv[1];
    static const char *known[] = {"texture", "size",     "color",     "sourceRect",
                                  "rotation", "facing",  "depthTest", "normal"};
    if (efx_api_check_known_fields(ctx, opts, known, 8, "drawBillboard") != 0) {
        return JS_EXCEPTION;
    }
    JSValue tv = JS_GetPropertyStr(ctx, opts, "texture");
    if (JS_IsUndefined(tv)) {
        JS_FreeValue(ctx, tv);
        return efx_api_type_error(ctx, "drawBillboard requires a texture");
    }
    uint64_t tex = 0;
    if (efx_api_get_live_sample(ctx, tv, &tex) != 0) {
        JS_FreeValue(ctx, tv);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, tv);

    float w = 1.0f, h = 1.0f;
    float color[4] = {1, 1, 1, 1};
    float rotation = 0.0f;
    float normal[3] = {0, 1, 0};
    int facing = EFX_FACING_VIEW;
    int depth_test = 1;
    float src[4] = {0, 0, 0, 0};
    int has_src = 0;

    JSValue zv = JS_GetPropertyStr(ctx, opts, "size");
    if (!JS_IsUndefined(zv)) {
        if (JS_IsArray(zv)) {
            float sz[2];
            if (efx_api_get_float_array(ctx, zv, sz, 2) != 0) {
                JS_FreeValue(ctx, zv);
                return JS_EXCEPTION;
            }
            w = sz[0];
            h = sz[1];
        } else {
            double d;
            if (JS_ToFloat64(ctx, &d, zv) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, zv);
                return efx_api_type_error(ctx, "size must be a number or [w,h]");
            }
            w = h = (float)d;
        }
    }
    JS_FreeValue(ctx, zv);
    if (!(w > 0) || !(h > 0)) {
        return efx_api_range_error(ctx, "size entries must be > 0");
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

    JSValue fv = JS_GetPropertyStr(ctx, opts, "facing");
    if (!JS_IsUndefined(fv)) {
        const char *s = JS_ToCString(ctx, fv);
        if (s && !strcmp(s, "view")) facing = EFX_FACING_VIEW;
        else if (s && !strcmp(s, "y")) facing = EFX_FACING_Y;
        else if (s && !strcmp(s, "plane")) facing = EFX_FACING_PLANE;
        else {
            if (s) JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, fv);
            return efx_api_type_error(ctx, "facing must be 'view', 'y', or 'plane'");
        }
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, fv);

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
        if (!JS_IsObject(sv)) {
            JS_FreeValue(ctx, sv);
            return efx_api_type_error(ctx, "sourceRect must be an object");
        }
        static const char *skeys[] = {"x", "y", "w", "h"};
        for (int i = 0; i < 4; i++) {
            JSValue f = JS_GetPropertyStr(ctx, sv, skeys[i]);
            double d;
            if (JS_ToFloat64(ctx, &d, f) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, f);
                JS_FreeValue(ctx, sv);
                return efx_api_type_error(ctx, "sourceRect fields must be finite numbers");
            }
            JS_FreeValue(ctx, f);
            src[i] = (float)d;
        }
        JS_FreeValue(ctx, sv);
        if (src[2] <= 0 || src[3] <= 0) {
            return efx_api_range_error(ctx, "sourceRect extent must be > 0");
        }
        int tw = 0, th = 0;
        efx_render_sample_size(tex, &tw, &th);
        if (src[0] < 0 || src[1] < 0 || src[0] + src[2] > (float)tw ||
            src[1] + src[3] > (float)th) {
            return efx_api_range_error(ctx, "sourceRect outside texture bounds");
        }
        has_src = 1;
    }

    int rc = efx_render_billboard(tex, pos, w, h, color, rotation, facing, normal,
                                  depth_test, src, has_src);
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

    float size[2] = {0, 0};
    int has_size = 0;
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
        has_size = 1;
    }
    JS_FreeValue(ctx, zv);

    JSValue ov = JS_GetPropertyStr(ctx, e, "origin");
    if (!JS_IsUndefined(ov)) {
        if (efx_api_get_float_array(ctx, ov, s->origin, 2) != 0) {
            JS_FreeValue(ctx, ov);
            return -1;
        }
        s->has_origin = 1;
    }
    JS_FreeValue(ctx, ov);

    JSValue srcv = JS_GetPropertyStr(ctx, e, "sourceRect");
    if (!JS_IsUndefined(srcv)) {
        if (!JS_IsObject(srcv)) {
            JS_FreeValue(ctx, srcv);
            efx_api_type_error(ctx, "sourceRect must be an object");
            return -1;
        }
        static const char *skeys[] = {"x", "y", "w", "h"};
        for (int i = 0; i < 4; i++) {
            JSValue f = JS_GetPropertyStr(ctx, srcv, skeys[i]);
            double d;
            if (JS_ToFloat64(ctx, &d, f) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, f);
                JS_FreeValue(ctx, srcv);
                efx_api_type_error(ctx, "sourceRect fields must be finite numbers");
                return -1;
            }
            JS_FreeValue(ctx, f);
            s->src[i] = (float)d;
        }
        JS_FreeValue(ctx, srcv);
        if (s->src[2] <= 0 || s->src[3] <= 0) {
            efx_api_range_error(ctx, "sourceRect extent must be > 0");
            return -1;
        }
        int tw = 0, th = 0;
        efx_render_sample_size(tex, &tw, &th);
        if (s->src[0] < 0 || s->src[1] < 0 ||
            s->src[0] + s->src[2] > (float)tw ||
            s->src[1] + s->src[3] > (float)th) {
            efx_api_range_error(ctx, "sourceRect outside texture bounds");
            return -1;
        }
        s->has_src = 1;
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
                                 oy);
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
