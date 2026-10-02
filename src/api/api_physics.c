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


/* ------------------------------------- natives for the prelude (ADR 0049)
 *
 * The option bags were validated and marshalled by the shared prelude
 * validators; these unpack the flat form. Shape type codes match the web
 * bridge: 0 sphere, 1 box, 2 capsule, 3 mesh. */

JSValue efx_js_check_mesh(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    (void)argc;
    if (!efx_api_get_live_mesh(ctx, argv[0])) {
        return JS_EXCEPTION;
    }
    return JS_UNDEFINED;
}

/* resolve a wire shape code into `out`; mesh shapes resolve the wrapper into
 * a temporary efx_phys_mesh (queries) or the live Mesh (static bodies) */
static int wire_shape(JSContext *ctx, int t, double r, double hx, double hy,
                      double hz, double height, JSValueConst meshv,
                      efx_shape *out, efx_phys_mesh **temp,
                      efxjs_mesh **out_mesh) {
    if (temp) {
        *temp = NULL;
    }
    if (out_mesh) {
        *out_mesh = NULL;
    }
    switch (t) {
    case 0:
        *out = efx_shape_sphere((float)r);
        return 0;
    case 1:
        *out = efx_shape_box(efx_v3((float)hx, (float)hy, (float)hz));
        return 0;
    case 2:
        *out = efx_shape_capsule((float)r, (float)height);
        return 0;
    default:
        break;
    }
    efxjs_mesh *m = efx_api_get_live_mesh(ctx, meshv);
    if (!m) {
        return -1;
    }
    if (out_mesh) {
        *out_mesh = m;
        *out = efx_shape_sphere(0);
        return 0;
    }
    efx_phys_mesh *mesh = build_temp_mesh(ctx, m);
    if (!mesh) {
        return -1;
    }
    *temp = mesh;
    *out = efx_shape_sphere(0);
    out->type = EFX_PHYS_SHAPE_MESH;
    out->mesh = mesh;
    return 0;
}

static double wire_num(JSContext *ctx, JSValueConst v) {
    double d = 0;
    if (JS_ToFloat64(ctx, &d, v) < 0) {
        return 0;
    }
    return d;
}

static int wire_i32(JSContext *ctx, JSValueConst v) {
    int32_t i = 0;
    JS_ToInt32(ctx, &i, v);
    return (int)i;
}

static uint32_t wire_u32(JSContext *ctx, JSValueConst v) {
    return (uint32_t)wire_num(ctx, v);
}


/* (dynamic, sensor, t, r, hx, hy, hz, height, px, py, pz, mass, friction,
 * restitution, layer, mask, mesh|null) */
JSValue efx_js_physics_create_body_wire(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 17) {
        return efx_api_type_error(ctx, "body wire native requires 17 arguments");
    }
    int dynamic = wire_i32(ctx, argv[0]);
    int sensor = wire_i32(ctx, argv[1]);
    int t = wire_i32(ctx, argv[2]);
    efx_physics_world *w = physics_world(ctx);
    if (t == 3) {
        if (dynamic) {
            return efx_api_type_error(ctx, "mesh colliders are static only");
        }
        efxjs_mesh *m = NULL;
        efx_phys_mesh *temp = NULL;
        efx_shape s;
        if (wire_shape(ctx, t, 0, 0, 0, 0, 0, argv[16], &s, &temp, &m) != 0) {
            efx_phys_mesh_free(temp);
            return JS_EXCEPTION;
        }
        (void)s;
        efx_static_mesh_desc sd;
        memset(&sd, 0, sizeof(sd));
        sd.position = efx_v3((float)wire_num(ctx, argv[8]),
                             (float)wire_num(ctx, argv[9]),
                             (float)wire_num(ctx, argv[10]));
        sd.sensor = sensor;
        sd.friction = (float)wire_num(ctx, argv[13]);
        sd.restitution = (float)wire_num(ctx, argv[14]);
        sd.layer = wire_u32(ctx, argv[15]);
        sd.mask = wire_u32(ctx, argv[15]);
        efx_phys_body h = create_mesh_body(ctx, w, m, &sd);
        return h ? wrap_collider(ctx, EFX_COLLIDER_BODY, w, h) : JS_EXCEPTION;
    }
    efx_body_desc d;
    memset(&d, 0, sizeof(d));
    d.dynamic = dynamic;
    d.sensor = sensor;
    efx_phys_mesh *temp = NULL;
    efxjs_mesh *unused = NULL;
    if (wire_shape(ctx, t, wire_num(ctx, argv[3]), wire_num(ctx, argv[4]),
                   wire_num(ctx, argv[5]), wire_num(ctx, argv[6]),
                   wire_num(ctx, argv[7]), argv[16], &d.shape, &temp,
                   &unused) != 0) {
        efx_phys_mesh_free(temp);
        return JS_EXCEPTION;
    }
    efx_phys_mesh_free(temp);
    d.position = efx_v3((float)wire_num(ctx, argv[8]),
                        (float)wire_num(ctx, argv[9]),
                        (float)wire_num(ctx, argv[10]));
    d.mass = (float)wire_num(ctx, argv[11]);
    d.friction = (float)wire_num(ctx, argv[12]);
    d.restitution = (float)wire_num(ctx, argv[13]);
    d.layer = wire_u32(ctx, argv[14]);
    d.mask = wire_u32(ctx, argv[15]);
    efx_phys_body h = efx_physics_create_body(w, &d);
    if (!h) {
        return efx_api_generic_error(ctx, "failed to create body");
    }
    return wrap_collider(ctx, EFX_COLLIDER_BODY, w, h);
}


/* (mesh, px, py, pz, sensor, friction, restitution, layer, mask) */
JSValue efx_js_physics_create_static_mesh_wire(JSContext *ctx,
                                               JSValueConst this_val,
                                               int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 9) {
        return efx_api_type_error(ctx, "static mesh wire native requires 9 arguments");
    }
    efxjs_mesh *m = efx_api_get_live_mesh(ctx, argv[0]);
    if (!m) {
        return JS_EXCEPTION;
    }
    efx_static_mesh_desc sd;
    memset(&sd, 0, sizeof(sd));
    sd.position = efx_v3((float)wire_num(ctx, argv[1]),
                         (float)wire_num(ctx, argv[2]),
                         (float)wire_num(ctx, argv[3]));
    sd.sensor = wire_i32(ctx, argv[4]);
    sd.friction = (float)wire_num(ctx, argv[5]);
    sd.restitution = (float)wire_num(ctx, argv[6]);
    sd.layer = wire_u32(ctx, argv[7]);
    sd.mask = wire_u32(ctx, argv[8]);
    efx_phys_body h = create_mesh_body(ctx, physics_world(ctx), m, &sd);
    if (!h) {
        return JS_EXCEPTION;
    }
    return wrap_collider(ctx, EFX_COLLIDER_BODY, physics_world(ctx), h);
}


/* (radius, height, px, py, pz, ux, uy, uz, floorMaxAngle, floorSnapLength,
 * stepHeight, safeMargin, maxSlides, layer, mask) */
JSValue efx_js_physics_create_character_wire(JSContext *ctx,
                                             JSValueConst this_val,
                                             int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 15) {
        return efx_api_type_error(ctx, "character wire native requires 15 arguments");
    }
    efx_character_desc d;
    memset(&d, 0, sizeof(d));
    d.radius = (float)wire_num(ctx, argv[0]);
    d.height = (float)wire_num(ctx, argv[1]);
    d.position = efx_v3((float)wire_num(ctx, argv[2]),
                        (float)wire_num(ctx, argv[3]),
                        (float)wire_num(ctx, argv[4]));
    d.up = efx_v3((float)wire_num(ctx, argv[5]),
                  (float)wire_num(ctx, argv[6]),
                  (float)wire_num(ctx, argv[7]));
    d.floor_max_angle = (float)wire_num(ctx, argv[8]);
    d.floor_snap_length = (float)wire_num(ctx, argv[9]);
    d.step_height = (float)wire_num(ctx, argv[10]);
    d.safe_margin = (float)wire_num(ctx, argv[11]);
    d.max_slides = wire_i32(ctx, argv[12]);
    d.layer = wire_u32(ctx, argv[13]);
    d.mask = wire_u32(ctx, argv[14]);
    efx_physics_world *w = physics_world(ctx);
    efx_phys_character h = efx_physics_create_character(w, &d);
    if (!h) {
        return efx_api_generic_error(ctx, "failed to create character");
    }
    return wrap_collider(ctx, EFX_COLLIDER_CHARACTER, w, h);
}


JSValue efx_js_physics_step_wire(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "dt must be a finite number");
    }
    efx_physics_step(physics_world(ctx), (float)wire_num(ctx, argv[0]));
    return JS_UNDEFINED;
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


/* (ox, oy, oz, dx, dy, dz, maxDistance, mask, sensors, all) */
JSValue efx_js_physics_raycast_wire(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 10) {
        return efx_api_type_error(ctx, "raycast wire native requires 10 arguments");
    }
    efx_vec3 o = efx_v3((float)wire_num(ctx, argv[0]),
                        (float)wire_num(ctx, argv[1]),
                        (float)wire_num(ctx, argv[2]));
    efx_vec3 d = efx_v3((float)wire_num(ctx, argv[3]),
                        (float)wire_num(ctx, argv[4]),
                        (float)wire_num(ctx, argv[5]));
    float maxd = (float)wire_num(ctx, argv[6]);
    uint32_t mask = wire_u32(ctx, argv[7]);
    int sensors = wire_i32(ctx, argv[8]);
    int all = wire_i32(ctx, argv[9]);
    efx_ray_hit hits[256];
    int cap = all ? 256 : 1;
    int n = efx_physics_raycast(physics_world(ctx), o, d, maxd, mask, sensors,
                                all, hits, cap);
    if (!all) {
        if (n == 0) {
            return JS_NULL;
        }
        return ray_hit_to_js(ctx, &hits[0]);
    }
    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < n; i++) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, ray_hit_to_js(ctx, &hits[i]));
    }
    return arr;
}


/* (t, r, hx, hy, hz, height, mesh|null, px, py, pz, mask) */
JSValue efx_js_physics_overlap_wire(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 11) {
        return efx_api_type_error(ctx, "overlap wire native requires 11 arguments");
    }
    efx_shape s;
    efx_phys_mesh *temp = NULL;
    if (wire_shape(ctx, wire_i32(ctx, argv[0]), wire_num(ctx, argv[1]),
                   wire_num(ctx, argv[2]), wire_num(ctx, argv[3]),
                   wire_num(ctx, argv[4]), wire_num(ctx, argv[5]), argv[6],
                   &s, &temp, NULL) != 0) {
        efx_phys_mesh_free(temp);
        return JS_EXCEPTION;
    }
    efx_vec3 position = efx_v3((float)wire_num(ctx, argv[7]),
                               (float)wire_num(ctx, argv[8]),
                               (float)wire_num(ctx, argv[9]));
    uint32_t mask = wire_u32(ctx, argv[10]);
    efx_physics_world *w = physics_world(ctx);
    int count = efx_physics_overlap(w, &s, position, mask, 1, NULL, 0);
    JSValue arr = JS_NewArray(ctx);
    if (count > 0) {
        efx_overlap_hit *hits = calloc((size_t)count, sizeof(*hits));
        if (!hits) {
            efx_phys_mesh_free(temp);
            return efx_api_generic_error(ctx, "out of memory");
        }
        int n = efx_physics_overlap(w, &s, position, mask, 1, hits, count);
        for (int i = 0; i < n; i++) {
            JSValue h = hit_wrapper(ctx, hits[i].character, hits[i].body);
            JS_SetPropertyUint32(ctx, arr, (uint32_t)i, h);
        }
        free(hits);
    }
    efx_phys_mesh_free(temp);
    return arr;
}


/* (t, r, hx, hy, hz, height, mesh|null, fx, fy, fz, mx, my, mz, mask,
 * sensors) */
JSValue efx_js_physics_shape_cast_wire(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 15) {
        return efx_api_type_error(ctx, "shapeCast wire native requires 15 arguments");
    }
    efx_shape s;
    efx_phys_mesh *temp = NULL;
    if (wire_shape(ctx, wire_i32(ctx, argv[0]), wire_num(ctx, argv[1]),
                   wire_num(ctx, argv[2]), wire_num(ctx, argv[3]),
                   wire_num(ctx, argv[4]), wire_num(ctx, argv[5]), argv[6],
                   &s, &temp, NULL) != 0) {
        efx_phys_mesh_free(temp);
        return JS_EXCEPTION;
    }
    efx_vec3 from = efx_v3((float)wire_num(ctx, argv[7]),
                           (float)wire_num(ctx, argv[8]),
                           (float)wire_num(ctx, argv[9]));
    efx_vec3 motion = efx_v3((float)wire_num(ctx, argv[10]),
                             (float)wire_num(ctx, argv[11]),
                             (float)wire_num(ctx, argv[12]));
    uint32_t mask = wire_u32(ctx, argv[13]);
    int sensors = wire_i32(ctx, argv[14]);
    efx_shape_hit hit;
    int rc = efx_physics_shape_cast(physics_world(ctx), &s, from, motion, mask,
                                    sensors, &hit);
    efx_phys_mesh_free(temp);
    if (!rc) {
        return JS_NULL;
    }
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "point", vec3_to_js(ctx, hit.point));
    JS_SetPropertyStr(ctx, o, "normal", vec3_to_js(ctx, hit.normal));
    JS_SetPropertyStr(ctx, o, "fraction", JS_NewFloat64(ctx, hit.fraction));
    JSValue body = hit_wrapper(ctx, hit.character, hit.body);
    JS_SetPropertyStr(ctx, o, "body", body);
    return o;
}


int efx_api_register_physics(JSContext *ctx, JSValueConst efx) {
    /* createBody/createCharacter/createStaticMesh/step/raycast/overlap/
     * shapeCast are installed by the shared prelude (ADR 0049) */
    static const JSCFunctionListEntry physics_funcs[] = {
        JS_CFUNC_DEF("clear", 0, physics_clear),
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
