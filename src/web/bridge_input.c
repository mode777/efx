#include "bridge_internal.h"

/* ------------------------------------------------- F9 (input) */

/* name lookup + validation (single C source shared with the desktop binding) */
EMSCRIPTEN_KEEPALIVE int efx_bridge_key_id(const char *name) {
    return efx_input_key_id(name);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_button_id(const char *name) {
    return efx_input_button_id(name);
}

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_key_name(int key) {
    return efx_input_key_name(key);
}

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_button_name(int button) {
    return efx_input_button_name(button);
}

/* level / edge queries */
EMSCRIPTEN_KEEPALIVE int efx_bridge_key_down(int key) {
    return efx_input_key_is_down(key);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_key_pressed(int key) {
    return efx_input_key_is_pressed(key);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_key_released(int key) {
    return efx_input_key_is_released(key);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_button_down(int button) {
    return efx_input_button_is_down(button);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_button_pressed(int button) {
    return efx_input_button_is_pressed(button);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_button_released(int button) {
    return efx_input_button_is_released(button);
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_mouse_x(void) {
    float x = 0, y = 0;
    efx_input_pointer(&x, &y);
    return x;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_mouse_y(void) {
    float x = 0, y = 0;
    efx_input_pointer(&x, &y);
    return y;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_mouse_dx(void) {
    float dx = 0, dy = 0;
    efx_input_delta(&dx, &dy);
    return dx;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_mouse_dy(void) {
    float dx = 0, dy = 0;
    efx_input_delta(&dx, &dy);
    return dy;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_wheel_dx(void) {
    float dx = 0, dy = 0;
    efx_input_wheel_delta(&dx, &dy);
    return dx;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_wheel_dy(void) {
    float dx = 0, dy = 0;
    efx_input_wheel_delta(&dx, &dy);
    return dy;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_window_width(void) {
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return w;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_window_height(void) {
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return h;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_window_dpi(void) {
    int w = 0, h = 0;
    float dpi = 1;
    efx_input_window_size(&w, &h, &dpi);
    return dpi;
}

/* frame event queue (entry.js builds the plain event objects) */
EMSCRIPTEN_KEEPALIVE int efx_bridge_input_count(void) {
    return efx_input_event_count();
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_type(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->type : -1;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_key(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->key : -1;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_button(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->button : -1;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_repeat(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->repeat : 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_mods(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? (int)ev->mods : 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_input_char(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? (int)ev->codepoint : -1;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_input_x(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->x : 0;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_input_y(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->y : 0;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_input_dx(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->dx : 0;
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_input_dy(int index) {
    const efx_input_event *ev = efx_input_event_at(index);
    return ev ? ev->dy : 0;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_input_clear(void) {
    efx_input_clear_events();
}

/* ------------------------------------------------- F13 (gamepad) */

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_count(void) {
    return efx_input_gamepad_count();
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_connected(int slot) {
    return efx_input_gamepad_connected(slot);
}

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_gamepad_name(int slot) {
    const char *n = efx_input_gamepad_name(slot);
    return n ? n : "";
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_mapped(int slot) {
    return efx_input_gamepad_mapped(slot);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_button_id(const char *name) {
    return efx_input_gamepad_button_id(name);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_axis_id(const char *name) {
    return efx_input_gamepad_axis_id(name);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_button_down(int slot, int button) {
    return efx_input_gamepad_button_is_down(slot, button);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_button_pressed(int slot,
                                                           int button) {
    return efx_input_gamepad_button_is_pressed(slot, button);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_button_released(int slot,
                                                            int button) {
    return efx_input_gamepad_button_is_released(slot, button);
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_gamepad_axis(int slot, int axis) {
    return efx_input_gamepad_axis(slot, axis);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_raw_button(int slot, int index) {
    return efx_input_gamepad_raw_button(slot, index);
}

EMSCRIPTEN_KEEPALIVE float efx_bridge_gamepad_raw_axis(int slot, int index) {
    return efx_input_gamepad_raw_axis(slot, index);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_connect_count(void) {
    return efx_input_gamepad_connect_count();
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_connect_at(int i) {
    return efx_input_gamepad_connect_at(i);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_disconnect_count(void) {
    return efx_input_gamepad_disconnect_count();
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_gamepad_disconnect_at(int i) {
    return efx_input_gamepad_disconnect_at(i);
}

