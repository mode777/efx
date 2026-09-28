#define _POSIX_C_SOURCE 200809L

#include "runtime/runtime.h"
#include "runtime/runtime_internal.h"
#include "api/api.h"
#include "input/efx_input.h"
#include "prelude/prelude.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"

struct efx_runtime {
    struct efx_host_state host;
    JSRuntime *js_rt;
    JSContext *ctx;
    int in_error;
    int hooks_sugar_done; /* global update/render registered once after eval */
    /* F10 shared CommonJS runtime (owned JS values) */
    JSValue module_runtime;
    JSValue module_run_entry;
    JSValue entry_exports;
    JSValue entry_update;
    JSValue entry_render;
    int has_entry;
};

int efx_hooks_append(JSContext *ctx, struct efx_hook_list *list, JSValueConst fn) {
    if (list->count == list->cap) {
        int cap = list->cap ? list->cap * 2 : 4;
        struct efx_hook_entry *grown =
            realloc(list->entries, (size_t)cap * sizeof(*grown));
        if (!grown) {
            return -1;
        }
        list->entries = grown;
        list->cap = cap;
    }
    int idx = list->count++;
    list->entries[idx].fn = JS_DupValue(ctx, fn);
    list->entries[idx].active = 1;
    return idx;
}

int efx_hooks_active(const struct efx_hook_list *list) {
    int n = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->entries[i].active) {
            n++;
        }
    }
    return n;
}

void efx_hooks_free_all(JSContext *ctx, struct efx_hook_list *list) {
    for (int i = 0; i < list->count; i++) {
        JS_FreeValue(ctx, list->entries[i].fn);
    }
    free(list->entries);
    list->entries = NULL;
    list->count = 0;
    list->cap = 0;
}

struct efx_hook_list *efx_host_hook_list(struct efx_host_state *h, int which) {
    switch (which) {
    case EFX_HOOK_LIST_RENDER:
        return &h->render_hooks;
    case EFX_HOOK_LIST_KB_DOWN:
        return &h->input_key_down;
    case EFX_HOOK_LIST_KB_UP:
        return &h->input_key_up;
    case EFX_HOOK_LIST_KB_CHAR:
        return &h->input_char;
    case EFX_HOOK_LIST_MOUSE_DOWN:
        return &h->input_mouse_down;
    case EFX_HOOK_LIST_MOUSE_UP:
        return &h->input_mouse_up;
    case EFX_HOOK_LIST_MOUSE_MOVE:
        return &h->input_mouse_move;
    case EFX_HOOK_LIST_MOUSE_WHEEL:
        return &h->input_mouse_wheel;
    case EFX_HOOK_LIST_UPDATE:
    default:
        return &h->update_hooks;
    }
}

static char *dup_string(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) {
        memcpy(p, s, n);
    }
    return p;
}

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    *out_len = rd;
    return buf;
}

static void dump_exception_value(efx_runtime *rt, JSValue exc) {
    if (JS_IsError(exc)) {
        JSValue msg = JS_GetPropertyStr(rt->ctx, exc, "message");
        const char *msg_str = JS_ToCString(rt->ctx, msg);
        fprintf(stderr, "uncaught exception: %s\n", msg_str ? msg_str : "<no message>");
        if (msg_str) {
            JS_FreeCString(rt->ctx, msg_str);
        }
        JS_FreeValue(rt->ctx, msg);
        JSValue stack = JS_GetPropertyStr(rt->ctx, exc, "stack");
        if (!JS_IsUndefined(stack)) {
            const char *stack_str = JS_ToCString(rt->ctx, stack);
            if (stack_str) {
                fprintf(stderr, "%s\n", stack_str);
                JS_FreeCString(rt->ctx, stack_str);
            }
        }
        JS_FreeValue(rt->ctx, stack);
    } else {
        const char *s = JS_ToCString(rt->ctx, exc);
        fprintf(stderr, "uncaught exception: %s\n", s ? s : "<non-error value thrown>");
        if (s) {
            JS_FreeCString(rt->ctx, s);
        }
    }
}

static int finish_exception(efx_runtime *rt) {
    JSValue exc = JS_GetException(rt->ctx);
    if (JS_VALUE_GET_PTR(exc) == JS_VALUE_GET_PTR(rt->host.quit_sentinel)) {
        JS_FreeValue(rt->ctx, exc);
        return 0;
    }
    dump_exception_value(rt, exc);
    JS_FreeValue(rt->ctx, exc);
    rt->in_error = 1;
    return 1;
}

efx_runtime *efx_runtime_new(char *const *args, int arg_count) {
    efx_runtime *rt = calloc(1, sizeof(*rt));
    if (!rt) {
        fprintf(stderr, "player: out of memory\n");
        return NULL;
    }
    rt->module_runtime = JS_UNDEFINED;
    rt->module_run_entry = JS_UNDEFINED;
    rt->entry_exports = JS_UNDEFINED;
    rt->entry_update = JS_UNDEFINED;
    rt->entry_render = JS_UNDEFINED;
    rt->js_rt = JS_NewRuntime();
    rt->ctx = JS_NewContext(rt->js_rt);
    rt->host.quit_sentinel = JS_NewObject(rt->ctx);
    rt->host.quit_code = 0;
    if (arg_count > 0 && args) {
        rt->host.args = calloc((size_t)arg_count, sizeof(char *));
        for (int i = 0; i < arg_count; i++) {
            rt->host.args[i] = dup_string(args[i]);
        }
        rt->host.arg_count = arg_count;
    }
    JS_SetContextOpaque(rt->ctx, &rt->host);

    JSValue glob = JS_GetGlobalObject(rt->ctx);
    JSValue efx = JS_NewObject(rt->ctx);
    static const JSCFunctionListEntry efx_funcs[] = {
        JS_CFUNC_DEF("log", 1, efx_js_log),
        JS_CFUNC_DEF("quit", 1, efx_js_quit),
        JS_CFUNC_DEF("args", 0, efx_js_args),
        JS_CFUNC_DEF("registerUpdateHook", 1, efx_js_registerUpdateHook),
        JS_CFUNC_DEF("registerRenderHook", 1, efx_js_registerRenderHook),
        JS_CFUNC_DEF("setClearColor", 1, efx_js_setClearColor),
        JS_CFUNC_DEF("setCamera2D", 1, efx_js_setCamera2D),
        JS_CFUNC_DEF("createImageData", 1, efx_js_createImageData),
        JS_CFUNC_DEF("createTexture", 1, efx_js_createTexture),
        JS_CFUNC_DEF("drawQuad", 4, efx_js_drawQuad),
        JS_CFUNC_DEF("setBlendMode", 1, efx_js_setBlendMode),
        JS_CGETSET_DEF("whiteTexture", efx_js_whiteTexture, NULL),
        JS_CFUNC_DEF("setCamera3D", 1, efx_js_setCamera3D),
        JS_CFUNC_DEF("createMeshData", 1, efx_js_createMeshData),
        JS_CFUNC_DEF("createMesh", 1, efx_js_createMesh),
        JS_CFUNC_DEF("drawMesh", 2, efx_js_drawMesh),
        JS_CFUNC_DEF("poseMesh", 2, efx_js_poseMesh),
        JS_CFUNC_DEF("setLight", 2, efx_js_setLight),
        JS_CFUNC_DEF("setDirectionalLight", 1, efx_js_setDirectionalLight),
        JS_CFUNC_DEF("setMeshSurfaceMaterial", 3, efx_js_setMeshSurfaceMaterial),
        JS_CFUNC_DEF("createRenderTarget", 1, efx_js_createRenderTarget),
        JS_CFUNC_DEF("beginRenderTarget", 1, efx_js_beginRenderTarget),
        JS_CFUNC_DEF("endRenderTarget", 0, efx_js_endRenderTarget),
        JS_CFUNC_DEF("setPostEffects", 1, efx_js_setPostEffects),
        JS_CFUNC_DEF("setRenderScale", 2, efx_js_setRenderScale),
        JS_CFUNC_DEF("loadText", 1, efx_js_loadText),
        JS_CFUNC_DEF("loadImage", 1, efx_js_loadImage),
        JS_CFUNC_DEF("loadMeshData", 2, efx_js_loadMeshData),
    };
    JS_SetPropertyFunctionList(rt->ctx, efx, efx_funcs,
                               (int)(sizeof(efx_funcs) / sizeof(efx_funcs[0])));
    if (efx_api_register_input(rt->ctx, efx) < 0) {
        fprintf(stderr, "player: input api init failed\n");
        JS_FreeValue(rt->ctx, efx);
        JS_FreeValue(rt->ctx, glob);
        efx_runtime_destroy(rt);
        return NULL;
    }
    JS_SetPropertyStr(rt->ctx, glob, "efx", efx);
    JS_FreeValue(rt->ctx, glob);
    if (efx_api_init(rt->ctx) < 0) {
        fprintf(stderr, "player: api init failed\n");
        efx_runtime_destroy(rt);
        return NULL;
    }
    /* engine-bundled pure-JS layer (F3 math + primitives, F10 CommonJS
       runtime); evaluated against the efx namespace so both bindings share
       one source. The IIFE returns the module-runtime factory, which we
       instantiate here for the desktop binding. */
    static const char wrapper[] =
        "(function(efx){\n";
    size_t wrap_len = sizeof(wrapper) - 1;
    size_t total = wrap_len + (size_t)EFX_JS_PRELUDE_LEN + 16;
    char *code = malloc(total);
    if (!code) {
        fprintf(stderr, "player: out of memory\n");
        efx_runtime_destroy(rt);
        return NULL;
    }
    memcpy(code, wrapper, wrap_len);
    memcpy(code + wrap_len, EFX_JS_PRELUDE, (size_t)EFX_JS_PRELUDE_LEN);
    memcpy(code + wrap_len + (size_t)EFX_JS_PRELUDE_LEN, "\n})(efx);\n", 11);
    size_t code_len = wrap_len + (size_t)EFX_JS_PRELUDE_LEN + 10;
    JSValue factory =
        JS_Eval(rt->ctx, code, code_len, "<prelude>", JS_EVAL_TYPE_GLOBAL);
    free(code);
    if (JS_IsException(factory)) {
        fprintf(stderr, "player: prelude evaluation failed\n");
        finish_exception(rt);
        efx_runtime_destroy(rt);
        return NULL;
    }
    JSValue glob2 = JS_GetGlobalObject(rt->ctx);
    JSValue efx_obj = JS_GetPropertyStr(rt->ctx, glob2, "efx");
    JS_FreeValue(rt->ctx, glob2);
    rt->module_runtime = JS_Call(rt->ctx, factory, JS_UNDEFINED, 1, &efx_obj);
    JS_FreeValue(rt->ctx, factory);
    JS_FreeValue(rt->ctx, efx_obj);
    if (JS_IsException(rt->module_runtime)) {
        fprintf(stderr, "player: module runtime init failed\n");
        rt->module_runtime = JS_UNDEFINED;
        finish_exception(rt);
        efx_runtime_destroy(rt);
        return NULL;
    }
    rt->module_run_entry =
        JS_GetPropertyStr(rt->ctx, rt->module_runtime, "runEntry");
    if (!JS_IsFunction(rt->ctx, rt->module_run_entry)) {
        fprintf(stderr, "player: module runtime missing runEntry\n");
        efx_runtime_destroy(rt);
        return NULL;
    }
    return rt;
}

void efx_runtime_destroy(efx_runtime *rt) {
    if (!rt) {
        return;
    }
    efx_hooks_free_all(rt->ctx, &rt->host.update_hooks);
    efx_hooks_free_all(rt->ctx, &rt->host.render_hooks);
    efx_hooks_free_all(rt->ctx, &rt->host.input_key_down);
    efx_hooks_free_all(rt->ctx, &rt->host.input_key_up);
    efx_hooks_free_all(rt->ctx, &rt->host.input_char);
    efx_hooks_free_all(rt->ctx, &rt->host.input_mouse_down);
    efx_hooks_free_all(rt->ctx, &rt->host.input_mouse_up);
    efx_hooks_free_all(rt->ctx, &rt->host.input_mouse_move);
    efx_hooks_free_all(rt->ctx, &rt->host.input_mouse_wheel);
    JS_FreeValue(rt->ctx, rt->host.quit_sentinel);
    JS_FreeValue(rt->ctx, rt->module_runtime);
    JS_FreeValue(rt->ctx, rt->module_run_entry);
    JS_FreeValue(rt->ctx, rt->entry_exports);
    JS_FreeValue(rt->ctx, rt->entry_update);
    JS_FreeValue(rt->ctx, rt->entry_render);
    if (rt->host.has_white_texture) {
        JS_FreeValue(rt->ctx, rt->host.white_texture);
    }
    for (int i = 0; i < rt->host.arg_count; i++) {
        free(rt->host.args[i]);
    }
    free(rt->host.args);
    JS_FreeContext(rt->ctx);
    JS_FreeRuntime(rt->js_rt);
    free(rt);
}

void efx_runtime_set_resource(efx_runtime *rt, struct efx_resource *resource) {
    rt->host.resource = resource;
}

int efx_runtime_eval_file(efx_runtime *rt, const char *path) {
    size_t len = 0;
    char *code = read_file(path, &len);
    if (!code) {
        fprintf(stderr, "player: cannot read script file: %s\n", path);
        return -1;
    }
    /* F10: the headless script is evaluated as the entry module; its resolved
       path is the bare file name so `require('./x')` resolves against the
       resource root (the script's own directory by default, or --root) */
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    int rc = efx_runtime_run_entry(rt, base, code);
    free(code);
    return rc;
}

int efx_runtime_run_entry(efx_runtime *rt, const char *path, const char *source) {
    JSValue args[2];
    args[0] = JS_NewString(rt->ctx, path);
    args[1] = source ? JS_NewString(rt->ctx, source) : JS_UNDEFINED;
    JSValue result = JS_Call(rt->ctx, rt->module_run_entry, rt->module_runtime,
                             2, args);
    JS_FreeValue(rt->ctx, args[0]);
    JS_FreeValue(rt->ctx, args[1]);
    if (JS_IsException(result)) {
        return finish_exception(rt);
    }
    JS_FreeValue(rt->ctx, rt->entry_exports);
    JS_FreeValue(rt->ctx, rt->entry_update);
    JS_FreeValue(rt->ctx, rt->entry_render);
    rt->entry_exports = JS_GetPropertyStr(rt->ctx, result, "exports");
    rt->entry_update = JS_GetPropertyStr(rt->ctx, result, "update");
    rt->entry_render = JS_GetPropertyStr(rt->ctx, result, "render");
    JS_FreeValue(rt->ctx, result);
    rt->has_entry = 1;
    return 0;
}

int efx_runtime_eval_string(efx_runtime *rt, const char *name, const char *code) {
    size_t len = strlen(code);
    JSValue result = JS_Eval(rt->ctx, code, len, name, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        return finish_exception(rt);
    }
    JS_FreeValue(rt->ctx, result);
    return 0;
}

int efx_runtime_eval_repl_line(efx_runtime *rt, const char *line) {
    JSValue result =
        JS_Eval(rt->ctx, line, strlen(line), "<repl>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(rt->ctx);
        if (JS_VALUE_GET_PTR(exc) == JS_VALUE_GET_PTR(rt->host.quit_sentinel)) {
            /* efx.quit(): a requested shutdown, not an error */
            JS_FreeValue(rt->ctx, exc);
            return 0;
        }
        dump_exception_value(rt, exc);
        JS_FreeValue(rt->ctx, exc);
        return 1; /* recovered: the run continues */
    }
    if (!JS_IsUndefined(result)) {
        const char *s = JS_ToCString(rt->ctx, result);
        fprintf(stdout, "%s\n", s ? s : "<unprintable value>");
        fflush(stdout);
        if (s) {
            JS_FreeCString(rt->ctx, s);
        }
    }
    JS_FreeValue(rt->ctx, result);
    return 0;
}

void efx_runtime_pick_hooks(efx_runtime *rt, int *has_update, int *has_render) {
    if (!rt->hooks_sugar_done) {
        rt->hooks_sugar_done = 1;
        /* F10: the entry module may export update/render (resolved by the
           module runtime), which take precedence over the global sugar; a
           hook present in both forms is registered once, not twice */
        JSValue u = JS_UNDEFINED;
        JSValue r = JS_UNDEFINED;
        if (rt->has_entry) {
            u = JS_DupValue(rt->ctx, rt->entry_update);
            r = JS_DupValue(rt->ctx, rt->entry_render);
        }
        if (!JS_IsFunction(rt->ctx, u)) {
            JS_FreeValue(rt->ctx, u);
            JSValue glob = JS_GetGlobalObject(rt->ctx);
            u = JS_GetPropertyStr(rt->ctx, glob, "update");
            JS_FreeValue(rt->ctx, glob);
        }
        if (!JS_IsFunction(rt->ctx, r)) {
            JS_FreeValue(rt->ctx, r);
            JSValue glob = JS_GetGlobalObject(rt->ctx);
            r = JS_GetPropertyStr(rt->ctx, glob, "render");
            JS_FreeValue(rt->ctx, glob);
        }
        if (JS_IsFunction(rt->ctx, u)) {
            efx_hooks_append(rt->ctx, &rt->host.update_hooks, u);
        }
        if (JS_IsFunction(rt->ctx, r)) {
            efx_hooks_append(rt->ctx, &rt->host.render_hooks, r);
        }
        JS_FreeValue(rt->ctx, u);
        JS_FreeValue(rt->ctx, r);
    }
    if (has_update) {
        *has_update = efx_hooks_active(&rt->host.update_hooks) > 0;
    }
    if (has_render) {
        *has_render = efx_hooks_active(&rt->host.render_hooks) > 0;
    }
}

int efx_runtime_call_hook(efx_runtime *rt, int update_not_render, double dt) {
    struct efx_hook_list *list =
        update_not_render ? &rt->host.update_hooks : &rt->host.render_hooks;
    for (int i = 0; i < list->count; i++) {
        if (!list->entries[i].active) {
            continue;
        }
        JSValue args[1];
        int nargs = 0;
        if (update_not_render) {
            args[0] = JS_NewFloat64(rt->ctx, dt);
            nargs = 1;
        }
        JSValue result = JS_Call(rt->ctx, list->entries[i].fn, JS_UNDEFINED,
                                 nargs, args);
        if (nargs) {
            JS_FreeValue(rt->ctx, args[0]);
        }
        if (JS_IsException(result)) {
            int rc = finish_exception(rt);
            return rc == 0 ? EFX_HOOK_QUIT : EFX_HOOK_ERROR;
        }
        JS_FreeValue(rt->ctx, result);
        if (rt->host.quit_requested) {
            return EFX_HOOK_QUIT;
        }
    }
    return EFX_HOOK_OK;
}

/* ---------------------------------------------- F9 input dispatch */

static int utf8_encode(uint32_t cp, char out[5]) {
    if (cp < 0x80u) {
        out[0] = (char)cp;
        out[1] = '\0';
        return 1;
    }
    if (cp < 0x800u) {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        out[2] = '\0';
        return 2;
    }
    if (cp < 0x10000u) {
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        out[3] = '\0';
        return 3;
    }
    if (cp <= 0x10FFFFu) {
        out[0] = (char)(0xF0u | (cp >> 18));
        out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = (char)(0x80u | (cp & 0x3Fu));
        out[4] = '\0';
        return 4;
    }
    out[0] = '?';
    out[1] = '\0';
    return 1;
}

static JSValue make_mods_array(JSContext *ctx, unsigned mods) {
    static const unsigned BITS[4] = {EFX_INPUT_MOD_SHIFT, EFX_INPUT_MOD_CTRL,
                                      EFX_INPUT_MOD_ALT, EFX_INPUT_MOD_SUPER};
    JSValue arr = JS_NewArray(ctx);
    uint32_t n = 0;
    for (int i = 0; i < 4; i++) {
        if (mods & BITS[i]) {
            const char *name = efx_input_mod_name(BITS[i]);
            JS_SetPropertyUint32(ctx, arr, n++, JS_NewString(ctx, name));
        }
    }
    return arr;
}

static JSValue make_input_event(JSContext *ctx, const efx_input_event *ev) {
    JSValue obj = JS_NewObject(ctx);
    switch (ev->type) {
    case EFX_INPUT_KEY_DOWN: {
        const char *name = efx_input_key_name(ev->key);
        JS_SetPropertyStr(ctx, obj, "key",
                          JS_NewString(ctx, name ? name : ""));
        JS_SetPropertyStr(ctx, obj, "repeat", JS_NewBool(ctx, ev->repeat));
        JS_SetPropertyStr(ctx, obj, "mods", make_mods_array(ctx, ev->mods));
        break;
    }
    case EFX_INPUT_KEY_UP: {
        const char *name = efx_input_key_name(ev->key);
        JS_SetPropertyStr(ctx, obj, "key",
                          JS_NewString(ctx, name ? name : ""));
        JS_SetPropertyStr(ctx, obj, "mods", make_mods_array(ctx, ev->mods));
        break;
    }
    case EFX_INPUT_CHAR: {
        char utf8[5];
        utf8_encode(ev->codepoint, utf8);
        JS_SetPropertyStr(ctx, obj, "char", JS_NewString(ctx, utf8));
        break;
    }
    case EFX_INPUT_MOUSE_DOWN:
    case EFX_INPUT_MOUSE_UP: {
        const char *name = efx_input_button_name(ev->button);
        JS_SetPropertyStr(ctx, obj, "button",
                          JS_NewString(ctx, name ? name : ""));
        JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, ev->x));
        JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, ev->y));
        JS_SetPropertyStr(ctx, obj, "mods", make_mods_array(ctx, ev->mods));
        break;
    }
    case EFX_INPUT_MOUSE_MOVE:
        JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, ev->x));
        JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, ev->y));
        JS_SetPropertyStr(ctx, obj, "dx", JS_NewFloat64(ctx, ev->dx));
        JS_SetPropertyStr(ctx, obj, "dy", JS_NewFloat64(ctx, ev->dy));
        break;
    case EFX_INPUT_WHEEL:
        JS_SetPropertyStr(ctx, obj, "dx", JS_NewFloat64(ctx, ev->dx));
        JS_SetPropertyStr(ctx, obj, "dy", JS_NewFloat64(ctx, ev->dy));
        break;
    default:
        break;
    }
    return obj;
}

static int input_event_list(const efx_input_event *ev) {
    switch (ev->type) {
    case EFX_INPUT_KEY_DOWN:
        return EFX_HOOK_LIST_KB_DOWN;
    case EFX_INPUT_KEY_UP:
        return EFX_HOOK_LIST_KB_UP;
    case EFX_INPUT_CHAR:
        return EFX_HOOK_LIST_KB_CHAR;
    case EFX_INPUT_MOUSE_DOWN:
        return EFX_HOOK_LIST_MOUSE_DOWN;
    case EFX_INPUT_MOUSE_UP:
        return EFX_HOOK_LIST_MOUSE_UP;
    case EFX_INPUT_MOUSE_MOVE:
        return EFX_HOOK_LIST_MOUSE_MOVE;
    case EFX_INPUT_WHEEL:
        return EFX_HOOK_LIST_MOUSE_WHEEL;
    default:
        return -1;
    }
}

int efx_runtime_dispatch_input(efx_runtime *rt) {
    int n = efx_input_event_count();
    for (int i = 0; i < n; i++) {
        const efx_input_event *ev = efx_input_event_at(i);
        int which = input_event_list(ev);
        if (which < 0) {
            continue;
        }
        struct efx_hook_list *list = efx_host_hook_list(&rt->host, which);
        JSValue arg = make_input_event(rt->ctx, ev);
        if (JS_IsException(arg)) {
            return finish_exception(rt) ? EFX_HOOK_ERROR : EFX_HOOK_QUIT;
        }
        for (int j = 0; j < list->count; j++) {
            if (!list->entries[j].active) {
                continue;
            }
            JSValue result = JS_Call(rt->ctx, list->entries[j].fn,
                                     JS_UNDEFINED, 1, &arg);
            if (JS_IsException(result)) {
                JS_FreeValue(rt->ctx, arg);
                int rc = finish_exception(rt);
                return rc == 0 ? EFX_HOOK_QUIT : EFX_HOOK_ERROR;
            }
            JS_FreeValue(rt->ctx, result);
            if (rt->host.quit_requested) {
                JS_FreeValue(rt->ctx, arg);
                return EFX_HOOK_QUIT;
            }
        }
        JS_FreeValue(rt->ctx, arg);
    }
    efx_input_clear_events();
    return EFX_HOOK_OK;
}

int efx_runtime_quit_requested(const efx_runtime *rt) {
    return rt->host.quit_requested;
}

int efx_runtime_quit_code(const efx_runtime *rt) {
    return rt->host.quit_code;
}

int efx_runtime_in_error(const efx_runtime *rt) {
    return rt->in_error;
}

void efx_runtime_collect(efx_runtime *rt) {
    if (rt) {
        JS_RunGC(rt->js_rt);
    }
}
