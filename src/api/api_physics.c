#include "api/api_internal.h"


static efx_physics_world *physics_world(JSContext *ctx) {
    struct efx_host_state *h = efx_api_host_state(ctx);
    if (!h->physics_world) {
        h->physics_world = efx_physics_world_new();
    }
    return (efx_physics_world *)h->physics_world;
}


static JSValue vec3_to_js(JSContext *ctx, efx_vec3 v) {
    JSValue a = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, a, 0, JS_NewFloat64(ctx, (double)v.x));
    JS_SetPropertyUint32(ctx, a, 1, JS_NewFloat64(ctx, (double)v.y));
    JS_SetPropertyUint32(ctx, a, 2, JS_NewFloat64(ctx, (double)v.z));
    return a;
}


/* per-kind native operations and messages; Body and Character handles are
 * both uint32_t, so one wrapper serves both classes */
typedef struct {
    JSClassID *class_id;
    const char *type_msg;
    const char *dead_msg;
    int (*destroy)(efx_physics_world *, uint32_t);
    int (*alive)(const efx_physics_world *, uint32_t);
    int (*position)(efx_physics_world *, uint32_t, efx_vec3 *);
    int (*velocity)(efx_physics_world *, uint32_t, efx_vec3 *);
    int (*set_velocity)(efx_physics_world *, uint32_t, efx_vec3);
} collider_ops;

static const collider_ops OPS[EFX_COLLIDER_KINDS] = {
    {&body_class_id, "expected a Body", "using a destroyed Body",
     efx_physics_destroy_body, efx_physics_body_alive,
     efx_physics_body_position, efx_physics_body_velocity,
     efx_physics_body_set_velocity},
    {&character_class_id, "expected a Character", "using a destroyed Character",
     efx_physics_destroy_character, efx_physics_character_alive,
     efx_physics_character_position, efx_physics_character_velocity,
     efx_physics_character_set_velocity},
};


static efxjs_collider **collider_list(struct efx_host_state *h, int kind) {
    return (efxjs_collider **)&h->physics_colliders[kind];
}


/* borrowed wrapper lookup by native id (a live wrapper is guaranteed to exist
 * while its C struct is linked) */
static JSValue find_wrapper(JSContext *ctx, int kind, uint32_t id) {
    if (id == 0) return JS_NULL;
    for (efxjs_collider *c = *collider_list(efx_api_host_state(ctx), kind); c;
         c = c->next) {
        if (c->alive && c->handle == id) {
            return JS_DupValue(ctx, c->self);
        }
    }
    return JS_NULL;
}


/* the wrapper a query hit refers to: its character if any, else its body */
static JSValue hit_wrapper(JSContext *ctx, efx_phys_character ch,
                           efx_phys_body b) {
    return ch ? find_wrapper(ctx, EFX_COLLIDER_CHARACTER, ch)
              : find_wrapper(ctx, EFX_COLLIDER_BODY, b);
}


/* physics option wording; numbers coerce (policy 0) */
static const char NUM_MSG[] = "numeric option fields must be finite numbers";
static const char MASK_MSG[] = "layer/mask must be a 32-bit unsigned integer";


static int parse_shape(JSContext *ctx, JSValueConst v, parsed_shape *out) {
    if (!JS_IsObject(v)) {
        efx_api_type_error(ctx, "shape must be an options object");
        return -1;
    }
    memset(out, 0, sizeof(*out));
    JSValue tv = JS_GetPropertyStr(ctx, v, "type");
    const char *type = JS_ToCString(ctx, tv);
    JS_FreeValue(ctx, tv);
    if (!type) return -1;

    int rc = -1;
    if (strcmp(type, "sphere") == 0) {
        static const char *known[] = {"type", "radius"};
        double r;
        if (efx_api_check_known_fields(ctx, v, known, 2, "shape") != 0) {
            rc = -1;
        } else if (efx_api_opt_number(ctx, v, "radius", &r, 0, NUM_MSG) != 1) {
            efx_api_type_error(ctx, "sphere shapes require a radius");
            rc = -1;
        } else if (!(r > 0)) {
            efx_api_range_error(ctx, "radius must be positive");
            rc = -1;
        } else {
            out->shape = efx_shape_sphere((float)r);
            rc = 0;
        }
    } else if (strcmp(type, "box") == 0) {
        static const char *known[] = {"type", "size"};
        JSValue sv = JS_GetPropertyStr(ctx, v, "size");
        float f[3];
        if (efx_api_check_known_fields(ctx, v, known, 2, "shape") != 0) {
            rc = -1;
        } else if (efx_api_get_float_array(ctx, sv, f, 3) != 0) {
            efx_api_type_error(ctx, "box shapes require a [x,y,z] size");
            rc = -1;
        } else if (!(f[0] > 0 && f[1] > 0 && f[2] > 0)) {
            efx_api_range_error(ctx, "box size components must be positive");
            rc = -1;
        } else {
            out->shape = efx_shape_box(efx_v3(f[0], f[1], f[2]));
            rc = 0;
        }
        JS_FreeValue(ctx, sv);
    } else if (strcmp(type, "capsule") == 0) {
        static const char *known[] = {"type", "radius", "height"};
        double r = 0, h = 0;
        int has_r = efx_api_opt_number(ctx, v, "radius", &r, 0, NUM_MSG);
        int has_h = efx_api_opt_number(ctx, v, "height", &h, 0, NUM_MSG);
        if (efx_api_check_known_fields(ctx, v, known, 3, "shape") != 0) {
            rc = -1;
        } else if (has_r != 1 || has_h != 1) {
            efx_api_type_error(ctx, "capsule shapes require radius and height");
            rc = -1;
        } else if (!(r > 0)) {
            efx_api_range_error(ctx, "radius must be positive");
            rc = -1;
        } else if (!(h >= 2 * r)) {
            efx_api_range_error(ctx, "capsule height must be at least 2 * radius");
            rc = -1;
        } else {
            out->shape = efx_shape_capsule((float)r, (float)h);
            rc = 0;
        }
    } else if (strcmp(type, "mesh") == 0) {
        static const char *known[] = {"type", "mesh"};
        JSValue mv = JS_GetPropertyStr(ctx, v, "mesh");
        if (efx_api_check_known_fields(ctx, v, known, 2, "shape") != 0) {
            rc = -1;
        } else {
            efxjs_mesh *m = efx_api_get_live_mesh(ctx, mv);
            if (!m) {
                rc = -1;
            } else {
                out->shape = efx_shape_sphere(0);
                out->shape.type = EFX_PHYS_SHAPE_MESH;
                out->mesh_src = m;
                rc = 0;
            }
        }
        JS_FreeValue(ctx, mv);
    } else {
        efx_api_type_error(ctx, "unknown shape type");
        rc = -1;
    }
    JS_FreeCString(ctx, type);
    return rc;
}


/* build a temporary efx_phys_mesh from a live Mesh (queries only; the caller
 * frees it) */
static efx_phys_mesh *build_temp_mesh(JSContext *ctx, efxjs_mesh *m) {
    int verts = 0, indices = 0;
    if (!efx_render_mesh_geometry_count(m->handle, &verts, &indices) ||
        verts <= 0 || indices < 3) {
        return NULL;
    }
    if (indices % 3 != 0) return NULL;
    float *pos = malloc((size_t)verts * 3 * sizeof(float));
    uint32_t *idx = malloc((size_t)indices * sizeof(uint32_t));
    if (!pos || !idx) {
        free(pos);
        free(idx);
        efx_api_generic_error(ctx, "out of memory");
        return NULL;
    }
    efx_render_mesh_geometry(m->handle, pos, idx);
    efx_phys_mesh *mesh =
        efx_phys_mesh_create(pos, verts, idx, indices / 3);
    free(pos);
    free(idx);
    if (!mesh) {
        efx_api_generic_error(ctx, "failed to build collision mesh");
    }
    return mesh;
}


static JSValue wrap_collider(JSContext *ctx, int kind, efx_physics_world *w,
                             uint32_t handle) {
    const collider_ops *o = &OPS[kind];
    efxjs_collider *c = calloc(1, sizeof(*c));
    if (!c) {
        o->destroy(w, handle);
        return efx_api_generic_error(ctx, "out of memory");
    }
    c->kind = kind;
    c->w = w;
    c->handle = handle;
    c->alive = 1;
    c->host = efx_api_host_state(ctx);
    JSValue obj = JS_NewObjectClass(ctx, *o->class_id);
    if (JS_IsException(obj)) {
        o->destroy(w, handle);
        free(c);
        return obj;
    }
    JS_SetOpaque(obj, c);
    c->self = JS_DupValue(ctx, obj);
    c->pinned = 1;
    efxjs_collider **head = collider_list(c->host, kind);
    c->next = *head;
    *head = c;
    return obj;
}


/* drop the world's reference; this may finalize and free `c` */
static void collider_unpin(JSContext *ctx, efxjs_collider *c) {
    if (!c->pinned) return;
    c->pinned = 0;
    JS_FreeValue(ctx, c->self);
}


/* finalizer step (CLASS_SPECS): unlink, then release a still-live collider */
void efx_api_collider_release(void *p) {
    efxjs_collider *c = (efxjs_collider *)p;
    if (c->host) {
        efxjs_collider **pp = collider_list(c->host, c->kind);
        while (*pp) {
            if (*pp == c) {
                *pp = c->next;
                break;
            }
            pp = &(*pp)->next;
        }
    }
    if (c->alive && c->w) {
        OPS[c->kind].destroy(c->w, c->handle);
    }
}


/* marks every wrapper destroyed and releases the world's references (after
 * efx_physics_clear, and at runtime teardown before the context is freed) */
void efx_api_physics_release(JSContext *ctx) {
    struct efx_host_state *h = efx_api_host_state(ctx);
    for (int kind = 0; kind < EFX_COLLIDER_KINDS; kind++) {
        efxjs_collider *c = *collider_list(h, kind);
        while (c) {
            efxjs_collider *next = c->next;
            c->alive = 0;
            collider_unpin(ctx, c);
            c = next;
        }
    }
}


/* live wrapper of `kind`, or NULL with a TypeError thrown */
static efxjs_collider *live_collider(JSContext *ctx, JSValueConst v, int kind) {
    const collider_ops *o = &OPS[kind];
    efxjs_collider *c = JS_GetOpaque2(ctx, v, *o->class_id);
    if (!c) {
        efx_api_type_error(ctx, o->type_msg);
        return NULL;
    }
    if (!c->alive || !c->w || !o->alive(c->w, c->handle)) {
        efx_api_type_error(ctx, o->dead_msg);
        return NULL;
    }
    return c;
}


/* ---- shared Body/Character methods / properties (magic = kind) ---- */

static JSValue collider_destroy(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int kind) {
    (void)argc;
    (void)argv;
    const collider_ops *o = &OPS[kind];
    efxjs_collider *c = JS_GetOpaque2(ctx, this_val, *o->class_id);
    if (!c) return efx_api_type_error(ctx, o->type_msg);
    if (c->alive) {
        c->alive = 0;
        if (c->w) o->destroy(c->w, c->handle);
    }
    collider_unpin(ctx, c);
    return JS_UNDEFINED;
}


static JSValue collider_get_position(JSContext *ctx, JSValueConst this_val,
                                     int kind) {
    efxjs_collider *c = live_collider(ctx, this_val, kind);
    if (!c) return JS_EXCEPTION;
    efx_vec3 p;
    OPS[kind].position(c->w, c->handle, &p);
    return vec3_to_js(ctx, p);
}


static JSValue collider_get_velocity(JSContext *ctx, JSValueConst this_val,
                                     int kind) {
    efxjs_collider *c = live_collider(ctx, this_val, kind);
    if (!c) return JS_EXCEPTION;
    efx_vec3 v;
    OPS[kind].velocity(c->w, c->handle, &v);
    return vec3_to_js(ctx, v);
}


static JSValue collider_set_velocity(JSContext *ctx, JSValueConst this_val,
                                     JSValueConst val, int kind) {
    efxjs_collider *c = live_collider(ctx, this_val, kind);
    if (!c) return JS_EXCEPTION;
    float f[3];
    if (efx_api_get_float_array(ctx, val, f, 3) != 0) return JS_EXCEPTION;
    OPS[kind].set_velocity(c->w, c->handle, efx_v3(f[0], f[1], f[2]));
    return JS_UNDEFINED;
}


/* ---- Body methods / properties ---- */

static efxjs_collider *live_body(JSContext *ctx, JSValueConst v) {
    return live_collider(ctx, v, EFX_COLLIDER_BODY);
}


static JSValue body_applyImpulse(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    efxjs_collider *b = live_body(ctx, this_val);
    if (!b) return JS_EXCEPTION;
    if (argc < 1) return efx_api_type_error(ctx, "applyImpulse requires a vector");
    float f[3];
    if (efx_api_get_float_array(ctx, argv[0], f, 3) != 0) return JS_EXCEPTION;
    if (!efx_physics_body_apply_impulse(b->w, b->handle,
                                        efx_v3(f[0], f[1], f[2]))) {
        return efx_api_type_error(ctx, "applyImpulse requires a dynamic body");
    }
    return JS_UNDEFINED;
}


static JSValue body_applyForce(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    efxjs_collider *b = live_body(ctx, this_val);
    if (!b) return JS_EXCEPTION;
    if (argc < 1) return efx_api_type_error(ctx, "applyForce requires a vector");
    float f[3];
    if (efx_api_get_float_array(ctx, argv[0], f, 3) != 0) return JS_EXCEPTION;
    if (!efx_physics_body_apply_force(b->w, b->handle,
                                      efx_v3(f[0], f[1], f[2]))) {
        return efx_api_type_error(ctx, "applyForce requires a dynamic body");
    }
    return JS_UNDEFINED;
}


static JSValue body_get_contacts(JSContext *ctx, JSValueConst this_val) {
    efxjs_collider *b = live_body(ctx, this_val);
    if (!b) return JS_EXCEPTION;
    int n = efx_physics_body_contact_count(b->w, b->handle);
    JSValue arr = JS_NewArray(ctx);
    int out_i = 0;
    for (int i = 0; i < n; i++) {
        efx_contact_info ci;
        if (!efx_physics_body_contact(b->w, b->handle, i, &ci)) continue;
        JSValue o = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, o, "body",
                          find_wrapper(ctx, EFX_COLLIDER_BODY, ci.body));
        JS_SetPropertyStr(ctx, o, "sensor", JS_NewBool(ctx, ci.sensor));
        JS_SetPropertyStr(ctx, o, "normal", vec3_to_js(ctx, ci.normal));
        JS_SetPropertyStr(ctx, o, "point", vec3_to_js(ctx, ci.point));
        JS_SetPropertyStr(ctx, o, "depth", JS_NewFloat64(ctx, ci.depth));
        JS_SetPropertyStr(ctx, o, "impulse", JS_NewFloat64(ctx, ci.impulse));
        JS_SetPropertyUint32(ctx, arr, (uint32_t)out_i++, o);
    }
    return arr;
}


static JSValue body_get_transform(JSContext *ctx, JSValueConst this_val) {
    efxjs_collider *b = live_body(ctx, this_val);
    if (!b) return JS_EXCEPTION;
    efx_vec3 p;
    efx_physics_body_position(b->w, b->handle, &p);
    JSValue a = JS_NewArray(ctx);
    const double m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, p.x, p.y, p.z, 1};
    for (int i = 0; i < 16; i++) {
        JS_SetPropertyUint32(ctx, a, (uint32_t)i, JS_NewFloat64(ctx, m[i]));
    }
    return a;
}


_Static_assert(sizeof(body_proto_funcs) / sizeof((body_proto_funcs)[0]) == 7,
                "body_proto_funcs must match the api_internal.h declaration");
const JSCFunctionListEntry body_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("destroy", 0, collider_destroy, EFX_COLLIDER_BODY),
    JS_CFUNC_DEF("applyImpulse", 1, body_applyImpulse),
    JS_CFUNC_DEF("applyForce", 1, body_applyForce),
    JS_CGETSET_MAGIC_DEF("position", collider_get_position, NULL,
                         EFX_COLLIDER_BODY),
    JS_CGETSET_MAGIC_DEF("velocity", collider_get_velocity,
                         collider_set_velocity, EFX_COLLIDER_BODY),
    JS_CGETSET_DEF("contacts", body_get_contacts, NULL),
    JS_CGETSET_DEF("transform", body_get_transform, NULL),
};



/* ---- Character methods / properties ---- */

static efxjs_collider *live_character(JSContext *ctx, JSValueConst v) {
    return live_collider(ctx, v, EFX_COLLIDER_CHARACTER);
}


static JSValue character_get_onFloor(JSContext *ctx, JSValueConst this_val) {
    efxjs_collider *c = live_character(ctx, this_val);
    if (!c) return JS_EXCEPTION;
    return JS_NewBool(ctx, efx_physics_character_on_floor(c->w, c->handle));
}


static JSValue character_moveAndSlide(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    efxjs_collider *c = live_character(ctx, this_val);
    if (!c) return JS_EXCEPTION;
    if (argc < 1) return efx_api_type_error(ctx, "moveAndSlide requires a motion vector");
    float f[3];
    if (efx_api_get_float_array(ctx, argv[0], f, 3) != 0) return JS_EXCEPTION;
    efx_move_result mr;
    if (!efx_physics_character_move_and_slide(c->w, c->handle,
                                              efx_v3(f[0], f[1], f[2]), &mr)) {
        return efx_api_type_error(ctx, "moveAndSlide failed");
    }
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "position", vec3_to_js(ctx, mr.position));
    JS_SetPropertyStr(ctx, o, "onFloor", JS_NewBool(ctx, mr.on_floor));
    JS_SetPropertyStr(ctx, o, "onWall", JS_NewBool(ctx, mr.on_wall));
    JS_SetPropertyStr(ctx, o, "onCeiling", JS_NewBool(ctx, mr.on_ceiling));
    JS_SetPropertyStr(ctx, o, "floorNormal",
                      vec3_to_js(ctx, mr.floor_normal));
    JSValue cols = JS_NewArray(ctx);
    int n = efx_physics_move_collision_count(c->w, c->handle);
    int out_i = 0;
    for (int i = 0; i < n; i++) {
        efx_move_collision mc;
        if (!efx_physics_move_collision(c->w, c->handle, i, &mc)) continue;
        JSValue co = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, co, "body",
                          find_wrapper(ctx, EFX_COLLIDER_BODY, mc.body));
        JS_SetPropertyStr(ctx, co, "normal", vec3_to_js(ctx, mc.normal));
        JS_SetPropertyStr(ctx, co, "point", vec3_to_js(ctx, mc.point));
        JS_SetPropertyUint32(ctx, cols, (uint32_t)out_i++, co);
    }
    JS_SetPropertyStr(ctx, o, "collisions", cols);
    return o;
}


_Static_assert(sizeof(character_proto_funcs) / sizeof((character_proto_funcs)[0]) == 5,
                "character_proto_funcs must match the api_internal.h declaration");
const JSCFunctionListEntry character_proto_funcs[] = {
    JS_CFUNC_MAGIC_DEF("destroy", 0, collider_destroy, EFX_COLLIDER_CHARACTER),
    JS_CFUNC_DEF("moveAndSlide", 1, character_moveAndSlide),
    JS_CGETSET_MAGIC_DEF("position", collider_get_position, NULL,
                         EFX_COLLIDER_CHARACTER),
    JS_CGETSET_MAGIC_DEF("velocity", collider_get_velocity,
                         collider_set_velocity, EFX_COLLIDER_CHARACTER),
    JS_CGETSET_DEF("onFloor", character_get_onFloor, NULL),
};



/* ================================================= F12 physics namespace */

static JSValue physics_get_gravity(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    return vec3_to_js(ctx, efx_physics_gravity(physics_world(ctx)));
}


static JSValue physics_set_gravity(JSContext *ctx, JSValueConst this_val,
                                   JSValueConst val) {
    (void)this_val;
    float f[3];
    if (efx_api_get_float_array(ctx, val, f, 3) != 0) return JS_EXCEPTION;
    efx_physics_set_gravity(physics_world(ctx), efx_v3(f[0], f[1], f[2]));
    return JS_UNDEFINED;
}


static JSValue physics_get_iterations(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    return JS_NewInt32(ctx, efx_physics_iterations(physics_world(ctx)));
}


static JSValue physics_set_iterations(JSContext *ctx, JSValueConst this_val,
                                      JSValueConst val) {
    (void)this_val;
    double d;
    if (JS_ToFloat64(ctx, &d, val) < 0 || !isfinite(d) || floor(d) != d ||
        d < 1) {
        return efx_api_range_error(ctx, "iterations must be a positive integer");
    }
    efx_physics_set_iterations(physics_world(ctx), (int)d);
    return JS_UNDEFINED;
}


static JSValue physics_step(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return efx_api_type_error(ctx, "step requires dt");
    double dt;
    if (JS_ToFloat64(ctx, &dt, argv[0]) < 0 || !isfinite(dt)) {
        return efx_api_type_error(ctx, "dt must be a finite number");
    }
    efx_physics_step(physics_world(ctx), (float)dt);
    return JS_UNDEFINED;
}


static JSValue physics_clear(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    efx_physics_clear(physics_world(ctx));
    efx_api_physics_release(ctx);
    return JS_UNDEFINED;
}


/* build a static mesh collider from a live Mesh + a static mesh descriptor */
static efx_phys_body create_mesh_body(JSContext *ctx, efx_physics_world *w,
                                      efxjs_mesh *m,
                                      const efx_static_mesh_desc *sd) {
    int verts = 0, indices = 0;
    if (!efx_render_mesh_geometry_count(m->handle, &verts, &indices) ||
        verts <= 0 || indices < 3 || indices % 3 != 0) {
        efx_api_type_error(ctx, "mesh has no triangles");
        return 0;
    }
    float *pos = malloc((size_t)verts * 3 * sizeof(float));
    uint32_t *idx = malloc((size_t)indices * sizeof(uint32_t));
    if (!pos || !idx) {
        free(pos);
        free(idx);
        efx_api_generic_error(ctx, "out of memory");
        return 0;
    }
    efx_render_mesh_geometry(m->handle, pos, idx);
    efx_phys_body h =
        efx_physics_create_static_mesh(w, pos, verts, idx, indices / 3, sd);
    free(pos);
    free(idx);
    if (!h) efx_api_generic_error(ctx, "failed to create mesh collider");
    return h;
}


static int read_common_body_opts(JSContext *ctx, JSValueConst opts, int *sensor,
                                 double *friction, double *restitution,
                                 efx_vec3 *position, uint32_t *layer,
                                 uint32_t *mask) {
    *sensor = 0;
    *friction = 0.5;
    *restitution = 0.0;
    *position = efx_v3(0, 0, 0);
    *layer = 0xFFFFFFFFu;
    *mask = 0xFFFFFFFFu;
    if (efx_api_opt_bool(ctx, opts, "sensor", sensor, 0, NULL) < 0) return -1;
    if (efx_api_opt_number(ctx, opts, "friction", friction, 0, NUM_MSG) < 0)
        return -1;
    if (efx_api_opt_number(ctx, opts, "restitution", restitution, 0,
                           NUM_MSG) < 0)
        return -1;
    if (efx_api_opt_vec3(ctx, opts, "position", position) < 0) return -1;
    if (efx_api_opt_u32(ctx, opts, "layer", layer, NUM_MSG, MASK_MSG) < 0)
        return -1;
    if (efx_api_opt_u32(ctx, opts, "mask", mask, NUM_MSG, MASK_MSG) < 0)
        return -1;
    if (*friction < 0) {
        efx_api_range_error(ctx, "friction must not be negative");
        return -1;
    }
    if (*restitution < 0 || *restitution > 1) {
        efx_api_range_error(ctx, "restitution must be in [0, 1]");
        return -1;
    }
    return 0;
}


static JSValue physics_createBody(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return efx_api_type_error(ctx, "createBody requires an options object");
    }
    JSValueConst opts = argv[0];
    static const char *known[] = {"dynamic", "sensor",      "shape",
                                  "position", "mass",       "friction",
                                  "restitution", "layer",    "mask"};
    if (efx_api_check_known_fields(ctx, opts, known, 9, "createBody") != 0) {
        return JS_EXCEPTION;
    }
    JSValue sv = JS_GetPropertyStr(ctx, opts, "shape");
    parsed_shape ps;
    int prc = parse_shape(ctx, sv, &ps);
    JS_FreeValue(ctx, sv);
    if (prc != 0) return JS_EXCEPTION;

    int dynamic = 0;
    efx_api_opt_bool(ctx, opts, "dynamic", &dynamic, 0, NULL);
    double mass = 1.0;
    if (efx_api_opt_number(ctx, opts, "mass", &mass, 0, NUM_MSG) < 0)
        return JS_EXCEPTION;
    if (dynamic && !(mass > 0)) {
        return efx_api_range_error(ctx, "dynamic bodies require a positive mass");
    }
    int sensor = 0;
    double friction = 0, rest = 0;
    efx_vec3 position;
    uint32_t layer, mask;
    if (read_common_body_opts(ctx, opts, &sensor, &friction, &rest, &position,
                              &layer, &mask) != 0) {
        return JS_EXCEPTION;
    }

    efx_physics_world *w = physics_world(ctx);
    if (ps.mesh_src) {
        if (dynamic) {
            return efx_api_type_error(ctx, "mesh colliders are static only");
        }
        efx_static_mesh_desc sd;
        memset(&sd, 0, sizeof(sd));
        sd.position = position;
        sd.sensor = sensor;
        sd.friction = (float)friction;
        sd.restitution = (float)rest;
        sd.layer = layer;
        sd.mask = mask;
        efx_phys_body h = create_mesh_body(ctx, w, ps.mesh_src, &sd);
        if (!h) return JS_EXCEPTION;
        return wrap_collider(ctx, EFX_COLLIDER_BODY, w, h);
    }
    efx_body_desc d;
    memset(&d, 0, sizeof(d));
    d.dynamic = dynamic;
    d.sensor = sensor;
    d.shape = ps.shape;
    d.position = position;
    d.mass = (float)mass;
    d.friction = (float)friction;
    d.restitution = (float)rest;
    d.layer = layer;
    d.mask = mask;
    efx_phys_body h = efx_physics_create_body(w, &d);
    if (!h) return efx_api_generic_error(ctx, "failed to create body");
    return wrap_collider(ctx, EFX_COLLIDER_BODY, w, h);
}


static JSValue physics_createStaticMesh(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return efx_api_type_error(ctx, "createStaticMesh requires a Mesh");
    efxjs_mesh *m = efx_api_get_live_mesh(ctx, argv[0]);
    if (!m) return JS_EXCEPTION;
    JSValueConst opts = JS_UNDEFINED;
    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        if (!JS_IsObject(argv[1])) {
            return efx_api_type_error(ctx, "createStaticMesh options must be an object");
        }
        opts = argv[1];
        static const char *known[] = {"position", "sensor", "friction",
                                      "restitution", "layer", "mask"};
        if (efx_api_check_known_fields(ctx, opts, known, 6, "createStaticMesh") != 0) {
            return JS_EXCEPTION;
        }
    }
    int sensor = 0;
    double friction = 0.5, rest = 0;
    efx_vec3 position = efx_v3(0, 0, 0);
    uint32_t layer = 0xFFFFFFFFu, mask = 0xFFFFFFFFu;
    if (JS_IsObject(opts) &&
        read_common_body_opts(ctx, opts, &sensor, &friction, &rest, &position,
                              &layer, &mask) != 0) {
        return JS_EXCEPTION;
    }
    efx_static_mesh_desc sd;
    memset(&sd, 0, sizeof(sd));
    sd.position = position;
    sd.sensor = sensor;
    sd.friction = (float)friction;
    sd.restitution = (float)rest;
    sd.layer = layer;
    sd.mask = mask;
    efx_physics_world *w = physics_world(ctx);
    efx_phys_body h = create_mesh_body(ctx, w, m, &sd);
    if (!h) return JS_EXCEPTION;
    return wrap_collider(ctx, EFX_COLLIDER_BODY, w, h);
}


static JSValue physics_createCharacter(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return efx_api_type_error(ctx, "createCharacter requires an options object");
    }
    JSValueConst opts = argv[0];
    static const char *known[] = {
        "radius",    "height",       "position",    "up",
        "floorMaxAngle", "floorSnapLength", "stepHeight", "maxSlides",
        "safeMargin", "layer",       "mask"};
    if (efx_api_check_known_fields(ctx, opts, known, 11, "createCharacter") != 0) {
        return JS_EXCEPTION;
    }
    double radius = 0, height = 0;
    int has_r = efx_api_opt_number(ctx, opts, "radius", &radius, 0, NUM_MSG);
    int has_h = efx_api_opt_number(ctx, opts, "height", &height, 0, NUM_MSG);
    if (has_r < 0 || has_h < 0) return JS_EXCEPTION;
    if (has_r != 1 || has_h != 1) {
        return efx_api_type_error(ctx, "createCharacter requires radius and height");
    }
    if (!(radius > 0)) return efx_api_range_error(ctx, "radius must be positive");
    if (!(height >= 2 * radius)) {
        return efx_api_range_error(ctx, "height must be at least 2 * radius");
    }
    efx_vec3 position = efx_v3(0, 0, 0), up = efx_v3(0, 1, 0);
    if (efx_api_opt_vec3(ctx, opts, "position", &position) < 0)
        return JS_EXCEPTION;
    if (efx_api_opt_vec3(ctx, opts, "up", &up) < 0) return JS_EXCEPTION;
    if (efx_v3_len_sq(up) <= 0) return efx_api_range_error(ctx, "up must be non-zero");

    double floor_max_angle = 45, snap = 0.1, step_height = 0.3, safe = 0.001;
    double max_slides = 6;
    if (efx_api_opt_number(ctx, opts, "floorMaxAngle", &floor_max_angle, 0,
                           NUM_MSG) < 0 ||
        efx_api_opt_number(ctx, opts, "floorSnapLength", &snap, 0, NUM_MSG) < 0 ||
        efx_api_opt_number(ctx, opts, "stepHeight", &step_height, 0,
                           NUM_MSG) < 0 ||
        efx_api_opt_number(ctx, opts, "safeMargin", &safe, 0, NUM_MSG) < 0 ||
        efx_api_opt_number(ctx, opts, "maxSlides", &max_slides, 0,
                           NUM_MSG) < 0)
        return JS_EXCEPTION;
    if (!(max_slides >= 1) || floor(max_slides) != max_slides) {
        return efx_api_range_error(ctx, "maxSlides must be a positive integer");
    }
    if (snap < 0) return efx_api_range_error(ctx, "floorSnapLength must not be negative");
    if (step_height < 0) return efx_api_range_error(ctx, "stepHeight must not be negative");
    if (safe < 0) return efx_api_range_error(ctx, "safeMargin must not be negative");
    uint32_t layer = 0xFFFFFFFFu, mask = 0xFFFFFFFFu;
    if (efx_api_opt_u32(ctx, opts, "layer", &layer, NUM_MSG, MASK_MSG) < 0 ||
        efx_api_opt_u32(ctx, opts, "mask", &mask, NUM_MSG, MASK_MSG) < 0)
        return JS_EXCEPTION;

    efx_character_desc d;
    memset(&d, 0, sizeof(d));
    d.radius = (float)radius;
    d.height = (float)height;
    d.position = position;
    d.up = up;
    d.floor_max_angle = (float)floor_max_angle;
    d.floor_snap_length = (float)snap;
    d.step_height = (float)step_height;
    d.safe_margin = (float)safe;
    d.max_slides = (int)max_slides;
    d.layer = layer;
    d.mask = mask;
    efx_physics_world *w = physics_world(ctx);
    efx_phys_character h = efx_physics_create_character(w, &d);
    if (!h) return efx_api_generic_error(ctx, "failed to create character");
    return wrap_collider(ctx, EFX_COLLIDER_CHARACTER, w, h);
}


static JSValue ray_hit_to_js(JSContext *ctx, const efx_ray_hit *h) {
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "point", vec3_to_js(ctx, h->point));
    JS_SetPropertyStr(ctx, o, "normal", vec3_to_js(ctx, h->normal));
    JS_SetPropertyStr(ctx, o, "distance", JS_NewFloat64(ctx, h->distance));
    JSValue body = hit_wrapper(ctx, h->character, h->body);
    JS_SetPropertyStr(ctx, o, "body", body);
    return o;
}


static JSValue physics_raycast(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return efx_api_type_error(ctx, "raycast requires origin and direction");
    float o[3], d[3];
    if (efx_api_get_float_array(ctx, argv[0], o, 3) != 0) return JS_EXCEPTION;
    if (efx_api_get_float_array(ctx, argv[1], d, 3) != 0) return JS_EXCEPTION;
    JSValueConst opts = JS_UNDEFINED;
    if (argc >= 3 && !JS_IsUndefined(argv[2])) {
        if (!JS_IsObject(argv[2])) {
            return efx_api_type_error(ctx, "raycast options must be an object");
        }
        opts = argv[2];
        static const char *known[] = {"maxDistance", "mask", "all", "sensors"};
        if (efx_api_check_known_fields(ctx, opts, known, 4, "raycast") != 0) {
            return JS_EXCEPTION;
        }
    }
    double maxd = 0;
    int has = JS_IsObject(opts)
                  ? efx_api_opt_number(ctx, opts, "maxDistance", &maxd, 0,
                                       NUM_MSG)
                  : 0;
    if (has < 0) return JS_EXCEPTION;
    if (has != 1 || !(maxd > 0)) {
        return efx_api_type_error(ctx, "raycast requires a positive maxDistance");
    }
    uint32_t mask = 0xFFFFFFFFu;
    int all = 0, sensors = 0;
    if (JS_IsObject(opts)) {
        if (efx_api_opt_u32(ctx, opts, "mask", &mask, NUM_MSG, MASK_MSG) < 0 ||
            efx_api_opt_bool(ctx, opts, "all", &all, 0, NULL) < 0 ||
            efx_api_opt_bool(ctx, opts, "sensors", &sensors, 0, NULL) < 0)
            return JS_EXCEPTION;
    }
    efx_ray_hit hits[256];
    int cap = all ? 256 : 1;
    int n = efx_physics_raycast(physics_world(ctx), efx_v3(o[0], o[1], o[2]),
                                efx_v3(d[0], d[1], d[2]), (float)maxd, mask,
                                sensors, all, hits, cap);
    if (!all) {
        if (n == 0) return JS_NULL;
        return ray_hit_to_js(ctx, &hits[0]);
    }
    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < n; i++) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, ray_hit_to_js(ctx, &hits[i]));
    }
    return arr;
}


static JSValue physics_overlap(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return efx_api_type_error(ctx, "overlap requires a shape");
    parsed_shape ps;
    if (parse_shape(ctx, argv[0], &ps) != 0) return JS_EXCEPTION;
    JSValueConst opts = JS_UNDEFINED;
    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        if (!JS_IsObject(argv[1])) {
            return efx_api_type_error(ctx, "overlap options must be an object");
        }
        opts = argv[1];
        static const char *known[] = {"position", "mask"};
        if (efx_api_check_known_fields(ctx, opts, known, 2, "overlap") != 0) {
            return JS_EXCEPTION;
        }
    }
    efx_vec3 position = efx_v3(0, 0, 0);
    uint32_t mask = 0xFFFFFFFFu;
    if (JS_IsObject(opts)) {
        if (efx_api_opt_vec3(ctx, opts, "position", &position) < 0 ||
            efx_api_opt_u32(ctx, opts, "mask", &mask, NUM_MSG, MASK_MSG) < 0)
            return JS_EXCEPTION;
    }
    efx_phys_mesh *temp = NULL;
    if (ps.mesh_src) {
        temp = build_temp_mesh(ctx, ps.mesh_src);
        if (!temp) return JS_EXCEPTION;
        ps.shape.mesh = temp;
    }
    efx_physics_world *w = physics_world(ctx);
    int count = efx_physics_overlap(w, &ps.shape, position, mask, 1, NULL, 0);
    JSValue arr = JS_NewArray(ctx);
    if (count > 0) {
        efx_overlap_hit *hits = calloc((size_t)count, sizeof(*hits));
        if (!hits) {
            efx_phys_mesh_free(temp);
            return efx_api_generic_error(ctx, "out of memory");
        }
        int n = efx_physics_overlap(w, &ps.shape, position, mask, 1, hits,
                                    count);
        for (int i = 0; i < n; i++) {
            JSValue h = hit_wrapper(ctx, hits[i].character, hits[i].body);
            JS_SetPropertyUint32(ctx, arr, (uint32_t)i, h);
        }
        free(hits);
    }
    efx_phys_mesh_free(temp);
    return arr;
}


static JSValue physics_shapeCast(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) {
        return efx_api_type_error(ctx, "shapeCast requires shape, from and motion");
    }
    parsed_shape ps;
    if (parse_shape(ctx, argv[0], &ps) != 0) return JS_EXCEPTION;
    float from[3], motion[3];
    if (efx_api_get_float_array(ctx, argv[1], from, 3) != 0) return JS_EXCEPTION;
    if (efx_api_get_float_array(ctx, argv[2], motion, 3) != 0) return JS_EXCEPTION;
    JSValueConst opts = JS_UNDEFINED;
    if (argc >= 4 && !JS_IsUndefined(argv[3])) {
        if (!JS_IsObject(argv[3])) {
            return efx_api_type_error(ctx, "shapeCast options must be an object");
        }
        opts = argv[3];
        static const char *known[] = {"mask", "sensors"};
        if (efx_api_check_known_fields(ctx, opts, known, 2, "shapeCast") != 0) {
            return JS_EXCEPTION;
        }
    }
    uint32_t mask = 0xFFFFFFFFu;
    int sensors = 0;
    if (JS_IsObject(opts)) {
        if (efx_api_opt_u32(ctx, opts, "mask", &mask, NUM_MSG, MASK_MSG) < 0 ||
            efx_api_opt_bool(ctx, opts, "sensors", &sensors, 0, NULL) < 0)
            return JS_EXCEPTION;
    }
    efx_phys_mesh *temp = NULL;
    if (ps.mesh_src) {
        temp = build_temp_mesh(ctx, ps.mesh_src);
        if (!temp) return JS_EXCEPTION;
        ps.shape.mesh = temp;
    }
    efx_shape_hit hit;
    int rc = efx_physics_shape_cast(physics_world(ctx), &ps.shape,
                                    efx_v3(from[0], from[1], from[2]),
                                    efx_v3(motion[0], motion[1], motion[2]),
                                    mask, sensors, &hit);
    efx_phys_mesh_free(temp);
    if (!rc) return JS_NULL;
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "point", vec3_to_js(ctx, hit.point));
    JS_SetPropertyStr(ctx, o, "normal", vec3_to_js(ctx, hit.normal));
    JS_SetPropertyStr(ctx, o, "fraction", JS_NewFloat64(ctx, hit.fraction));
    JSValue body = hit_wrapper(ctx, hit.character, hit.body);
    JS_SetPropertyStr(ctx, o, "body", body);
    return o;
}


int efx_api_register_physics(JSContext *ctx, JSValueConst efx) {
    static const JSCFunctionListEntry physics_funcs[] = {
        JS_CFUNC_DEF("step", 1, physics_step),
        JS_CFUNC_DEF("clear", 0, physics_clear),
        JS_CFUNC_DEF("createBody", 1, physics_createBody),
        JS_CFUNC_DEF("createCharacter", 1, physics_createCharacter),
        JS_CFUNC_DEF("createStaticMesh", 2, physics_createStaticMesh),
        JS_CFUNC_DEF("raycast", 2, physics_raycast),
        JS_CFUNC_DEF("overlap", 1, physics_overlap),
        JS_CFUNC_DEF("shapeCast", 3, physics_shapeCast),
        JS_CGETSET_DEF("gravity", physics_get_gravity, physics_set_gravity),
        JS_CGETSET_DEF("iterations", physics_get_iterations,
                       physics_set_iterations),
    };
    JSValue phys = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, phys, physics_funcs,
                               (int)(sizeof(physics_funcs) /
                                     sizeof(physics_funcs[0])));
    /* JS_SetPropertyStr consumes the value reference */
    JS_SetPropertyStr(ctx, efx, "physics", phys);
    return 0;
}
