#include "api/api_internal.h"


/* -------------------------------------------------------- F9 input bindings */

/* keyboard.isDown/isPressed/isReleased — magic 0/1/2 */
static JSValue efx_js_key_query(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int magic) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) {
        return efx_api_type_error(ctx, "keyboard query requires a key name");
    }
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) {
        return JS_EXCEPTION;
    }
    int key = efx_input_key_id(name);
    JS_FreeCString(ctx, name);
    if (key < 0) {
        return efx_api_type_error(ctx, "unknown key");
    }
    int v;
    if (magic == 1) {
        v = efx_input_key_is_pressed(key);
    } else if (magic == 2) {
        v = efx_input_key_is_released(key);
    } else {
        v = efx_input_key_is_down(key);
    }
    return JS_NewBool(ctx, v);
}


/* keyboard.onDown/onUp/onChar — magic 0/1/2; returns an unsubscribe fn */
static JSValue efx_js_key_on(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "input callback registration requires a function");
    }
    int which = magic == 1 ? EFX_HOOK_LIST_KB_UP
              : magic == 2 ? EFX_HOOK_LIST_KB_CHAR
                           : EFX_HOOK_LIST_KB_DOWN;
    return efx_api_register_hook(ctx, argv[0], which);
}


/* mouse.isDown/isPressed/isReleased — magic 0/1/2 */
static JSValue efx_js_mouse_query(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) {
        return efx_api_type_error(ctx, "mouse query requires a button name");
    }
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) {
        return JS_EXCEPTION;
    }
    int button = efx_input_button_id(name);
    JS_FreeCString(ctx, name);
    if (button < 0) {
        return efx_api_type_error(ctx, "unknown mouse button");
    }
    int v;
    if (magic == 1) {
        v = efx_input_button_is_pressed(button);
    } else if (magic == 2) {
        v = efx_input_button_is_released(button);
    } else {
        v = efx_input_button_is_down(button);
    }
    return JS_NewBool(ctx, v);
}


/* mouse.onDown/onUp/onMove/onWheel — magic 0/1/2/3 */
static JSValue efx_js_mouse_on(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx, "input callback registration requires a function");
    }
    int which;
    switch (magic) {
    case 1:
        which = EFX_HOOK_LIST_MOUSE_UP;
        break;
    case 2:
        which = EFX_HOOK_LIST_MOUSE_MOVE;
        break;
    case 3:
        which = EFX_HOOK_LIST_MOUSE_WHEEL;
        break;
    default:
        which = EFX_HOOK_LIST_MOUSE_DOWN;
        break;
    }
    return efx_api_register_hook(ctx, argv[0], which);
}


static JSValue num_pair(JSContext *ctx, float a, float b) {
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat64(ctx, a));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat64(ctx, b));
    return arr;
}


static JSValue efx_js_mouse_getPosition(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    float x = 0, y = 0;
    efx_input_pointer(&x, &y);
    return num_pair(ctx, x, y);
}


static JSValue efx_js_mouse_getX(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    float x = 0, y = 0;
    efx_input_pointer(&x, &y);
    return JS_NewFloat64(ctx, x);
}


static JSValue efx_js_mouse_getY(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    float x = 0, y = 0;
    efx_input_pointer(&x, &y);
    return JS_NewFloat64(ctx, y);
}


static JSValue efx_js_mouse_getDelta(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    float dx = 0, dy = 0;
    efx_input_delta(&dx, &dy);
    return num_pair(ctx, dx, dy);
}


static JSValue efx_js_mouse_getWheel(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    float dx = 0, dy = 0;
    efx_input_wheel_delta(&dx, &dy);
    return num_pair(ctx, dx, dy);
}


static JSValue efx_js_window_getSize(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewInt32(ctx, w));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewInt32(ctx, h));
    return arr;
}


static JSValue efx_js_window_getWidth(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return JS_NewInt32(ctx, w);
}


static JSValue efx_js_window_getHeight(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return JS_NewInt32(ctx, h);
}


static JSValue efx_js_window_getDpiScale(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return JS_NewFloat64(ctx, dpi);
}


/* F13 gamepad: count is a read-only property; get(index) returns the pad
 * view or null; onConnect/onDisconnect return unsubscribe functions. */
static JSValue efx_js_gamepad_count(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    return JS_NewInt32(ctx, efx_input_gamepad_count());
}


static JSValue efx_js_gamepad_get(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsNumber(argv[0])) {
        return efx_api_type_error(ctx, "gamepad.get requires an index");
    }
    int index = 0;
    JS_ToInt32(ctx, &index, argv[0]);
    if (index < 0 || index >= EFX_GAMEPAD_MAX ||
        !efx_input_gamepad_connected(index)) {
        return JS_NULL;
    }
    return efx_runtime_gamepad_view((efx_runtime *)efx_api_host_state(ctx), index);
}


static JSValue efx_js_gamepad_on(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic) {
    (void)this_val;
    if (argc < 1) {
        return efx_api_type_error(ctx,
                          "input callback registration requires a function");
    }
    int which = magic == 1 ? EFX_HOOK_LIST_GP_DISCONNECT
                           : EFX_HOOK_LIST_GP_CONNECT;
    return efx_api_register_hook(ctx, argv[0], which);
}


int efx_api_register_input(JSContext *ctx, JSValueConst efx) {
    static const JSCFunctionListEntry kb_funcs[] = {
        JS_CFUNC_MAGIC_DEF("isDown", 1, efx_js_key_query, 0),
        JS_CFUNC_MAGIC_DEF("isPressed", 1, efx_js_key_query, 1),
        JS_CFUNC_MAGIC_DEF("isReleased", 1, efx_js_key_query, 2),
        JS_CFUNC_MAGIC_DEF("onDown", 1, efx_js_key_on, 0),
        JS_CFUNC_MAGIC_DEF("onUp", 1, efx_js_key_on, 1),
        JS_CFUNC_MAGIC_DEF("onChar", 1, efx_js_key_on, 2),
    };
    static const JSCFunctionListEntry mouse_funcs[] = {
        JS_CFUNC_MAGIC_DEF("isDown", 1, efx_js_mouse_query, 0),
        JS_CFUNC_MAGIC_DEF("isPressed", 1, efx_js_mouse_query, 1),
        JS_CFUNC_MAGIC_DEF("isReleased", 1, efx_js_mouse_query, 2),
        JS_CFUNC_MAGIC_DEF("onDown", 1, efx_js_mouse_on, 0),
        JS_CFUNC_MAGIC_DEF("onUp", 1, efx_js_mouse_on, 1),
        JS_CFUNC_MAGIC_DEF("onMove", 1, efx_js_mouse_on, 2),
        JS_CFUNC_MAGIC_DEF("onWheel", 1, efx_js_mouse_on, 3),
        JS_CGETSET_DEF("position", efx_js_mouse_getPosition, NULL),
        JS_CGETSET_DEF("x", efx_js_mouse_getX, NULL),
        JS_CGETSET_DEF("y", efx_js_mouse_getY, NULL),
        JS_CGETSET_DEF("delta", efx_js_mouse_getDelta, NULL),
        JS_CGETSET_DEF("wheel", efx_js_mouse_getWheel, NULL),
    };
    static const JSCFunctionListEntry window_funcs[] = {
        JS_CGETSET_DEF("size", efx_js_window_getSize, NULL),
        JS_CGETSET_DEF("width", efx_js_window_getWidth, NULL),
        JS_CGETSET_DEF("height", efx_js_window_getHeight, NULL),
        JS_CGETSET_DEF("dpiScale", efx_js_window_getDpiScale, NULL),
    };
    static const JSCFunctionListEntry gamepad_funcs[] = {
        JS_CFUNC_DEF("get", 1, efx_js_gamepad_get),
        JS_CFUNC_MAGIC_DEF("onConnect", 1, efx_js_gamepad_on, 0),
        JS_CFUNC_MAGIC_DEF("onDisconnect", 1, efx_js_gamepad_on, 1),
        JS_CGETSET_DEF("count", efx_js_gamepad_count, NULL),
    };
    JSValue kb = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, kb, kb_funcs,
                               (int)(sizeof(kb_funcs) / sizeof(kb_funcs[0])));
    JSValue mouse = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, mouse, mouse_funcs,
                               (int)(sizeof(mouse_funcs) /
                                     sizeof(mouse_funcs[0])));
    JSValue window = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, window, window_funcs,
                               (int)(sizeof(window_funcs) /
                                     sizeof(window_funcs[0])));
    JSValue gamepad = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, gamepad, gamepad_funcs,
                               (int)(sizeof(gamepad_funcs) /
                                     sizeof(gamepad_funcs[0])));
    /* JS_SetPropertyStr consumes the value reference */
    JS_SetPropertyStr(ctx, efx, "keyboard", kb);
    JS_SetPropertyStr(ctx, efx, "mouse", mouse);
    JS_SetPropertyStr(ctx, efx, "window", window);
    JS_SetPropertyStr(ctx, efx, "gamepad", gamepad);
    return 0;
}
