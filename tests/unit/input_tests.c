/*
 * Headless unit tests for the F9 input core (no window, no script runtime):
 * name lookup, level/edge semantics, frame staging, arrival ordering,
 * per-frame deltas, focus clearing, and the injection seam.
 * Usage: efx_input_tests <case> ; exit 0 = pass.
 */
#include "input/efx_input.h"
#include "input/efx_gamepad.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(const char *what) {
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

/* every documented key name resolves to an id that round-trips back */
static int names(void) {
    static const char *keys[] = {
        "space", "a", "z", "0", "9", "f1", "f12", "left", "right", "up",
        "down", "home", "end", "pageup", "pagedown", "insert", "delete",
        "enter", "escape", "tab", "backspace", "lshift", "rshift", "lctrl",
        "rctrl", "lalt", "ralt", "lsuper", "rsuper", "apostrophe", "comma",
        "minus", "period", "slash", "semicolon", "equal", "leftbracket",
        "backslash", "rightbracket", "grave", "capslock", "scrolllock",
        "numlock", "printscreen", "pause", "kp0", "kp9", "kpdecimal",
        "kpdivide", "kpmultiply", "kpsubtract", "kpadd", "kpenter",
        "kpequal", "menu",
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        int id = efx_input_key_id(keys[i]);
        if (id < 0) {
            fprintf(stderr, "FAIL: key name not resolved: %s\n", keys[i]);
            return 1;
        }
        if (!efx_input_key_name(id) ||
            strcmp(efx_input_key_name(id), keys[i]) != 0) {
            fprintf(stderr, "FAIL: key name round-trip: %s\n", keys[i]);
            return 1;
        }
    }
    if (efx_input_key_id("notakey") != -1 || efx_input_key_id("") != -1 ||
        efx_input_key_id(NULL) != -1) {
        return fail("unknown key must fail lookup");
    }
    if (efx_input_button_id("left") != EFX_INPUT_MOUSE_LEFT ||
        efx_input_button_id("right") != EFX_INPUT_MOUSE_RIGHT ||
        efx_input_button_id("middle") != EFX_INPUT_MOUSE_MIDDLE) {
        return fail("button names");
    }
    if (efx_input_button_id("side") != -1) {
        return fail("unknown button must fail lookup");
    }
    return 0;
}

/* level/edge semantics and one-frame edge expiry */
static int level_edge(void) {
    efx_input_reset();
    int key = efx_input_key_id("space");
    efx_input_begin_frame();
    efx_input_inject_key(key, 1, 0, 0);
    if (!efx_input_key_is_down(key) || !efx_input_key_is_pressed(key) ||
        efx_input_key_is_released(key)) {
        return fail("press state");
    }
    if (efx_input_event_count() != 1 ||
        efx_input_event_at(0)->type != EFX_INPUT_KEY_DOWN) {
        return fail("down event queued");
    }
    efx_input_end_frame();
    efx_input_begin_frame();
    if (!efx_input_key_is_down(key) || efx_input_key_is_pressed(key)) {
        return fail("press edge must expire after one frame");
    }
    efx_input_inject_key(key, 0, 0, 0);
    if (efx_input_key_is_down(key) || !efx_input_key_is_released(key)) {
        return fail("release edge");
    }
    efx_input_end_frame();
    efx_input_begin_frame();
    if (efx_input_key_is_released(key)) {
        return fail("release edge must expire");
    }
    return 0;
}

/* auto-repeat sets the repeat flag but not a new press edge */
static int repeat(void) {
    efx_input_reset();
    int key = efx_input_key_id("w");
    efx_input_begin_frame();
    efx_input_inject_key(key, 1, 0, 0);
    efx_input_end_frame();
    efx_input_begin_frame();
    efx_input_inject_key(key, 1, 1, 0); /* auto-repeat */
    if (!efx_input_key_is_down(key) || efx_input_key_is_pressed(key)) {
        return fail("repeat must not set press edge");
    }
    if (efx_input_event_count() != 1 || !efx_input_event_at(0)->repeat) {
        return fail("repeat event flag");
    }
    return 0;
}

/* events drain in arrival order */
static int ordering(void) {
    efx_input_reset();
    int a = efx_input_key_id("a");
    int b = efx_input_key_id("b");
    efx_input_begin_frame();
    efx_input_inject_key(a, 1, 0, 0);
    efx_input_inject_key(b, 1, 0, 0);
    efx_input_inject_key(a, 0, 0, 0);
    if (efx_input_event_count() != 3 ||
        efx_input_event_at(0)->key != a ||
        efx_input_event_at(1)->key != b ||
        efx_input_event_at(2)->key != a ||
        efx_input_event_at(0)->type != EFX_INPUT_KEY_DOWN ||
        efx_input_event_at(2)->type != EFX_INPUT_KEY_UP) {
        return fail("arrival order");
    }
    efx_input_clear_events();
    if (efx_input_event_count() != 0) {
        return fail("clear events");
    }
    return 0;
}

/* movement/wheel accumulate between frames and reset per frame */
static int deltas(void) {
    efx_input_reset();
    efx_input_begin_frame();
    efx_input_inject_mouse_move(10, 20, 3, 4);
    efx_input_inject_mouse_move(13, 24, 3, 4);
    efx_input_inject_wheel(0, 1);
    efx_input_inject_wheel(0, 2);
    efx_input_begin_frame(); /* second begin commits the accumulated values */
    float dx = 0, dy = 0, wx = 0, wy = 0;
    efx_input_pointer(&dx, &dy);
    if (dx != 13 || dy != 24) {
        return fail("pointer position");
    }
    efx_input_delta(&dx, &dy);
    if (dx != 6 || dy != 8) {
        return fail("movement accumulation");
    }
    efx_input_wheel_delta(&wx, &wy);
    if (wx != 0 || wy != 3) {
        return fail("wheel accumulation");
    }
    efx_input_end_frame();
    efx_input_delta(&dx, &dy);
    efx_input_wheel_delta(&wx, &wy);
    if (dx != 0 || dy != 0 || wx != 0 || wy != 0) {
        return fail("deltas reset at frame end");
    }
    return 0;
}

/* mouse button level/edge */
static int mouse(void) {
    efx_input_reset();
    efx_input_begin_frame();
    efx_input_inject_mouse_button(EFX_INPUT_MOUSE_LEFT, 1, 5, 6, 0);
    if (!efx_input_button_is_down(EFX_INPUT_MOUSE_LEFT) ||
        !efx_input_button_is_pressed(EFX_INPUT_MOUSE_LEFT)) {
        return fail("mouse press");
    }
    float x = 0, y = 0;
    efx_input_pointer(&x, &y);
    if (x != 5 || y != 6) {
        return fail("mouse press position");
    }
    efx_input_end_frame();
    efx_input_begin_frame();
    efx_input_inject_mouse_button(EFX_INPUT_MOUSE_LEFT, 0, 7, 8, 0);
    if (efx_input_button_is_down(EFX_INPUT_MOUSE_LEFT) ||
        !efx_input_button_is_released(EFX_INPUT_MOUSE_LEFT)) {
        return fail("mouse release");
    }
    return 0;
}

/* focus loss clears held state and emits no up events */
static int focus(void) {
    efx_input_reset();
    int key = efx_input_key_id("lshift");
    efx_input_begin_frame();
    efx_input_inject_key(key, 1, 0, 0);
    efx_input_inject_mouse_button(EFX_INPUT_MOUSE_RIGHT, 1, 1, 1, 0);
    efx_input_clear_events();
    efx_input_focus_lost();
    if (efx_input_key_is_down(key) ||
        efx_input_button_is_down(EFX_INPUT_MOUSE_RIGHT)) {
        return fail("focus loss must clear held state");
    }
    if (efx_input_event_count() != 0) {
        return fail("focus loss must not emit events");
    }
    return 0;
}

/* char events carry the decoded codepoint */
static int chars(void) {
    efx_input_reset();
    efx_input_begin_frame();
    efx_input_inject_char(0x41);
    efx_input_inject_char(0x20AC);
    if (efx_input_event_count() != 2 ||
        efx_input_event_at(0)->type != EFX_INPUT_CHAR ||
        efx_input_event_at(0)->codepoint != 0x41 ||
        efx_input_event_at(1)->codepoint != 0x20AC) {
        return fail("char events");
    }
    return 0;
}

/* window size/dpi reporting */
static int window(void) {
    efx_input_reset();
    efx_input_set_window(1280, 720, 2.0f);
    int w = 0, h = 0;
    float dpi = 0;
    efx_input_window_size(&w, &h, &dpi);
    if (w != 1280 || h != 720 || dpi != 2.0f) {
        return fail("window size/dpi");
    }
    return 0;
}

/* mods names resolve for every supported bit */
static int mods(void) {
    if (strcmp(efx_input_mod_name(EFX_INPUT_MOD_SHIFT), "shift") != 0 ||
        strcmp(efx_input_mod_name(EFX_INPUT_MOD_CTRL), "ctrl") != 0 ||
        strcmp(efx_input_mod_name(EFX_INPUT_MOD_ALT), "alt") != 0 ||
        strcmp(efx_input_mod_name(EFX_INPUT_MOD_SUPER), "super") != 0) {
        return fail("mod names");
    }
    if (efx_input_mod_name(0x80u) != NULL) {
        return fail("unknown mod must be NULL");
    }
    return 0;
}

/* ------------------------------------------------------------ F13 gamepad */

static int gp_names(void) {
    static const char *btns[] = {
        "south", "east", "west", "north", "leftShoulder", "rightShoulder",
        "leftTrigger", "rightTrigger", "back", "start", "guide", "leftStick",
        "rightStick", "dpadUp", "dpadDown", "dpadLeft", "dpadRight",
    };
    for (size_t i = 0; i < sizeof(btns) / sizeof(btns[0]); i++) {
        int id = efx_input_gamepad_button_id(btns[i]);
        if (id < 0 || !efx_input_gamepad_button_name(id) ||
            strcmp(efx_input_gamepad_button_name(id), btns[i]) != 0) {
            return fail("gamepad button name round-trip");
        }
    }
    static const char *axes[] = {"leftX", "leftY", "rightX", "rightY",
                                 "leftTrigger", "rightTrigger"};
    for (size_t i = 0; i < sizeof(axes) / sizeof(axes[0]); i++) {
        int id = efx_input_gamepad_axis_id(axes[i]);
        if (id < 0 || !efx_input_gamepad_axis_name(id) ||
            strcmp(efx_input_gamepad_axis_name(id), axes[i]) != 0) {
            return fail("gamepad axis name round-trip");
        }
    }
    if (efx_input_gamepad_button_id("notabutton") != -1 ||
        efx_input_gamepad_axis_id("leftZ") != -1 ||
        efx_input_gamepad_button_id(NULL) != -1) {
        return fail("unknown gamepad name must fail lookup");
    }
    return 0;
}

/* press/release edges valid one frame on a normalized (standard) pad */
static int gp_edges(void) {
    efx_input_reset();
    unsigned char btns[1] = {0};
    float axes[6] = {0, 0, 0, 0, 0, 0};
    efx_input_gamepad_inject_connect(0, "Pad", NULL, 1);
    efx_input_gamepad_inject_state(0, 1, btns, 6, axes);
    efx_input_begin_frame();
    if (efx_input_gamepad_count() != 1 ||
        efx_input_gamepad_connected(0) != 1) {
        return fail("gamepad connect");
    }
    if (efx_input_gamepad_button_is_down(0, EFX_GPB_SOUTH)) {
        return fail("gamepad initially up");
    }
    efx_input_end_frame();

    btns[0] = 1;
    efx_input_gamepad_inject_state(0, 1, btns, 6, axes);
    efx_input_begin_frame();
    if (!efx_input_gamepad_button_is_down(0, EFX_GPB_SOUTH) ||
        !efx_input_gamepad_button_is_pressed(0, EFX_GPB_SOUTH) ||
        efx_input_gamepad_button_is_released(0, EFX_GPB_SOUTH)) {
        return fail("gamepad press edge");
    }
    efx_input_end_frame();
    efx_input_begin_frame();
    if (!efx_input_gamepad_button_is_down(0, EFX_GPB_SOUTH) ||
        efx_input_gamepad_button_is_pressed(0, EFX_GPB_SOUTH)) {
        return fail("gamepad press edge must expire");
    }
    efx_input_end_frame();

    btns[0] = 0;
    efx_input_gamepad_inject_state(0, 1, btns, 6, axes);
    efx_input_begin_frame();
    if (efx_input_gamepad_button_is_down(0, EFX_GPB_SOUTH) ||
        !efx_input_gamepad_button_is_released(0, EFX_GPB_SOUTH)) {
        return fail("gamepad release edge");
    }
    return 0;
}

/* hot-plug: connect/disconnect events and clearing held state on unplug */
static int gp_hotplug(void) {
    efx_input_reset();
    unsigned char down[1] = {1};
    float axes[6] = {0, 0, 0, 0, 0, 0};
    efx_input_gamepad_inject_connect(0, "Pad", NULL, 1);
    efx_input_gamepad_inject_state(0, 1, down, 6, axes);
    efx_input_begin_frame();
    if (efx_input_gamepad_count() != 1 ||
        efx_input_gamepad_connect_count() != 1 ||
        efx_input_gamepad_connect_at(0) != 0) {
        return fail("gamepad connect event");
    }
    if (!efx_input_gamepad_button_is_down(0, EFX_GPB_SOUTH)) {
        return fail("gamepad held before unplug");
    }
    efx_input_end_frame();

    efx_input_gamepad_inject_disconnect(0);
    efx_input_begin_frame();
    if (efx_input_gamepad_count() != 0 || efx_input_gamepad_connected(0)) {
        return fail("gamepad disconnect");
    }
    if (efx_input_gamepad_disconnect_count() != 1 ||
        efx_input_gamepad_disconnect_at(0) != 0) {
        return fail("gamepad disconnect event");
    }
    if (efx_input_gamepad_button_is_down(0, EFX_GPB_SOUTH)) {
        return fail("gamepad unplug must not leave stuck state");
    }
    return 0;
}

/* a synthetic SDL mapping drives the semantic surface */
static int gp_mapping(void) {
    efx_input_reset();
    efx_input_gamepad_clear_mappings();
    if (!efx_input_gamepad_load_mapping_line(
            "11110000000000000000000000000000,Test,a:b0,b:b1,leftx:a0,"
            "lefty:a1,lefttrigger:+a2,righttrigger:+a3,")) {
        return fail("gamepad load mapping");
    }
    unsigned char btns[2] = {1, 0};
    float axes[4] = {0.5f, -0.5f, -1.0f, 1.0f};
    efx_input_gamepad_inject_connect(0, "Test",
                                     "11110000000000000000000000000000", 0);
    efx_input_gamepad_inject_state(0, 2, btns, 4, axes);
    efx_input_begin_frame();
    if (!efx_input_gamepad_mapped(0)) {
        return fail("gamepad mapping selected");
    }
    if (!efx_input_gamepad_button_is_down(0, EFX_GPB_SOUTH) ||
        efx_input_gamepad_button_is_down(0, EFX_GPB_EAST)) {
        return fail("gamepad mapped buttons");
    }
    if (fabsf(efx_input_gamepad_axis(0, EFX_GPA_LEFT_X) - 0.5f) > 1e-5f ||
        fabsf(efx_input_gamepad_axis(0, EFX_GPA_LEFT_Y) + 0.5f) > 1e-5f) {
        return fail("gamepad mapped sticks");
    }
    /* +aN half-axis collapses -1..1 onto 0..1 */
    if (fabsf(efx_input_gamepad_axis(0, EFX_GPA_LEFT_TRIGGER) - 0.0f) > 1e-5f ||
        fabsf(efx_input_gamepad_axis(0, EFX_GPA_RIGHT_TRIGGER) - 1.0f) > 1e-5f) {
        return fail("gamepad mapped triggers");
    }
    if (efx_input_gamepad_button_is_down(0, EFX_GPB_LEFT_TRIGGER) ||
        !efx_input_gamepad_button_is_down(0, EFX_GPB_RIGHT_TRIGGER)) {
        return fail("gamepad digital triggers");
    }
    return 0;
}

/* half-axis, inversion, and the digital-trigger threshold */
static int gp_half_invert(void) {
    efx_input_reset();
    efx_input_gamepad_clear_mappings();
    if (!efx_input_gamepad_load_mapping_line(
            "22220000000000000000000000000000,Half,leftx:+a0,lefty:~a1,"
            "lefttrigger:+a2,")) {
        return fail("gamepad load half mapping");
    }
    unsigned char btns[1] = {0};
    float axes[3] = {-1.0f, 1.0f, 0.0f};
    efx_input_gamepad_inject_connect(0, "Half",
                                     "22220000000000000000000000000000", 0);
    efx_input_gamepad_inject_state(0, 1, btns, 3, axes);
    efx_input_begin_frame();
    /* +a0 at raw -1 -> 0; ~a1 at raw 1 -> -1; +a2 at raw 0 -> 0.5 */
    if (fabsf(efx_input_gamepad_axis(0, EFX_GPA_LEFT_X) - 0.0f) > 1e-5f) {
        return fail("half-axis positive range");
    }
    if (fabsf(efx_input_gamepad_axis(0, EFX_GPA_LEFT_Y) + 1.0f) > 1e-5f) {
        return fail("inverted axis direction");
    }
    if (fabsf(efx_input_gamepad_axis(0, EFX_GPA_LEFT_TRIGGER) - 0.5f) > 1e-5f) {
        return fail("half-axis trigger midpoint");
    }
    /* threshold is inclusive: exactly 0.5 is down */
    if (!efx_input_gamepad_button_is_down(0, EFX_GPB_LEFT_TRIGGER)) {
        return fail("trigger threshold inclusive");
    }
    return 0;
}

/* hat-mapped and button-mapped d-pad both resolve */
static int gp_hat(void) {
    efx_input_reset();
    efx_input_gamepad_clear_mappings();
    if (!efx_input_gamepad_load_mapping_line(
            "33330000000000000000000000000000,Hat,dpup:h0.1,dpright:h0.2,"
            "dpdown:h0.4,dpleft:h0.8,")) {
        return fail("gamepad load hat mapping");
    }
    efx_input_gamepad_inject_connect(0, "Hat",
                                     "33330000000000000000000000000000", 0);
    unsigned char btns[1] = {0};
    float axes[1] = {0.0f};
    float hats[2] = {0.0f, -1.0f}; /* hat 0 y = up */
    efx_input_gamepad_inject_state(0, 1, btns, 1, axes);
    efx_input_gamepad_inject_hats(0, 2, hats);
    efx_input_begin_frame();
    if (!efx_input_gamepad_button_is_down(0, EFX_GPB_DPAD_UP) ||
        efx_input_gamepad_button_is_down(0, EFX_GPB_DPAD_DOWN) ||
        efx_input_gamepad_button_is_down(0, EFX_GPB_DPAD_LEFT) ||
        efx_input_gamepad_button_is_down(0, EFX_GPB_DPAD_RIGHT)) {
        return fail("hat up");
    }
    efx_input_end_frame();

    hats[0] = 1.0f;
    hats[1] = 0.0f; /* hat 0 x = right */
    efx_input_gamepad_inject_hats(0, 2, hats);
    efx_input_begin_frame();
    if (!efx_input_gamepad_button_is_down(0, EFX_GPB_DPAD_RIGHT) ||
        efx_input_gamepad_button_is_down(0, EFX_GPB_DPAD_UP)) {
        return fail("hat right");
    }
    return 0;
}

/* d-pad expressed as buttons */
static int gp_dpad_buttons(void) {
    efx_input_reset();
    efx_input_gamepad_clear_mappings();
    if (!efx_input_gamepad_load_mapping_line(
            "44440000000000000000000000000000,Buttons,dpleft:b5,dpright:b4,")) {
        return fail("gamepad load dpad-button mapping");
    }
    efx_input_gamepad_inject_connect(0, "Buttons",
                                     "44440000000000000000000000000000", 0);
    unsigned char btns[6] = {0, 0, 0, 0, 0, 1};
    efx_input_gamepad_inject_state(0, 6, btns, 0, NULL);
    efx_input_begin_frame();
    if (!efx_input_gamepad_button_is_down(0, EFX_GPB_DPAD_LEFT) ||
        efx_input_gamepad_button_is_down(0, EFX_GPB_DPAD_RIGHT)) {
        return fail("button d-pad");
    }
    return 0;
}

/* desktop-style and web-standard descriptors collapse to identical values */
static int gp_ranges(void) {
    efx_input_reset();
    efx_input_gamepad_clear_mappings();
    if (!efx_input_gamepad_load_mapping_line(
            "55550000000000000000000000000000,Desk,leftx:a0,lefty:a1,"
            "rightx:a2,righty:a3,lefttrigger:+a4,righttrigger:+a5,")) {
        return fail("gamepad load range mapping");
    }
    /* desktop: raw axes are -1..1 including triggers */
    efx_input_gamepad_inject_connect(0, "Desk",
                                     "55550000000000000000000000000000", 0);
    float desktop[6] = {0.25f, -0.75f, 0.0f, 0.5f, 0.0f, 1.0f};
    efx_input_gamepad_inject_state(0, 0, NULL, 6, desktop);
    /* web standard: raw axes are already canonical (triggers 0..1) */
    efx_input_gamepad_inject_connect(1, "Web", NULL, 1);
    float web[6] = {0.25f, -0.75f, 0.0f, 0.5f, 0.5f, 1.0f};
    efx_input_gamepad_inject_state(1, 0, NULL, 6, web);
    efx_input_begin_frame();
    for (int a = 0; a < EFX_GPA_COUNT; a++) {
        float d = efx_input_gamepad_axis(0, a);
        float w = efx_input_gamepad_axis(1, a);
        if (fabsf(d - w) > 1e-5f) {
            return fail("canonical range mismatch desktop vs web");
        }
    }
    return 0;
}

/* an unmapped device is still connected and readable through the raw surface */
static int gp_raw(void) {
    efx_input_reset();
    efx_input_gamepad_clear_mappings();
    efx_input_gamepad_inject_connect(0, "Unknown",
                                     "99990000000000000000000000000000", 0);
    unsigned char btns[2] = {0, 1};
    float axes[3] = {0.1f, 0.2f, 0.3f};
    efx_input_gamepad_inject_state(0, 2, btns, 3, axes);
    efx_input_begin_frame();
    if (!efx_input_gamepad_connected(0) || efx_input_gamepad_mapped(0)) {
        return fail("unmapped pad reports connected+unmapped");
    }
    if (efx_input_gamepad_raw_button(0, 1) != 1 ||
        efx_input_gamepad_raw_button(0, 0) != 0) {
        return fail("raw button fallback");
    }
    if (fabsf(efx_input_gamepad_raw_axis(0, 2) - 0.3f) > 1e-5f) {
        return fail("raw axis fallback");
    }
    /* semantic surface is inert without a mapping */
    if (efx_input_gamepad_button_is_down(0, EFX_GPB_SOUTH) ||
        efx_input_gamepad_axis(0, EFX_GPA_LEFT_X) != 0.0f) {
        return fail("unmapped semantic surface must be inert");
    }
    return 0;
}

/* GUID selection: exact, permissive tail, and vendor/product prefix fallback
 * (the engine-side counterpart of the Windows non-Xbox matching defect). */
static int gp_guid_fallback(void) {
    efx_input_reset();
    efx_input_gamepad_clear_mappings();
    if (!efx_input_gamepad_load_mapping_line(
            "aaaaaaaa000000000000000000000000,Vendor,a:b0,")) {
        return fail("gamepad load guid mapping");
    }
    unsigned char btns[1] = {1};
    /* exact match */
    efx_input_gamepad_inject_connect(0, "Exact",
                                     "aaaaaaaa000000000000000000000000", 0);
    efx_input_gamepad_inject_state(0, 1, btns, 0, NULL);
    /* differs only in the trailing bytes -> permissive tail fallback */
    efx_input_gamepad_inject_connect(1, "Tail",
                                     "aaaaaaaa0000000000000000deadbeef", 0);
    efx_input_gamepad_inject_state(1, 1, btns, 0, NULL);
    /* differs in the vendor/product prefix -> unmapped */
    efx_input_gamepad_inject_connect(2, "Other",
                                     "aaaaaaab000000000000000000000000", 0);
    efx_input_gamepad_inject_state(2, 1, btns, 0, NULL);
    efx_input_begin_frame();
    if (!efx_input_gamepad_mapped(0) || !efx_input_gamepad_mapped(1)) {
        return fail("guid exact/permissive match");
    }
    if (efx_input_gamepad_mapped(2)) {
        return fail("different vendor must not match");
    }
    if (!efx_input_gamepad_button_is_down(0, EFX_GPB_SOUTH) ||
        !efx_input_gamepad_button_is_down(1, EFX_GPB_SOUTH)) {
        return fail("permissive mapping surface");
    }
    return 0;
}

/* full connect -> press -> release -> disconnect through the production path */
static int gp_full(void) {
    efx_input_reset();
    unsigned char btns[1] = {0};
    float axes[6] = {0, 0, 0, 0, 0, 0};
    efx_input_gamepad_inject_connect(0, "Pad", NULL, 1);
    efx_input_gamepad_inject_state(0, 1, btns, 6, axes);
    efx_input_begin_frame();
    if (efx_input_gamepad_count() != 1 ||
        efx_input_gamepad_connect_count() != 1) {
        return fail("full connect");
    }
    efx_input_end_frame();

    btns[0] = 1;
    efx_input_gamepad_inject_state(0, 1, btns, 6, axes);
    efx_input_begin_frame();
    if (!efx_input_gamepad_button_is_pressed(0, EFX_GPB_SOUTH)) {
        return fail("full press");
    }
    efx_input_end_frame();

    btns[0] = 0;
    efx_input_gamepad_inject_state(0, 1, btns, 6, axes);
    efx_input_begin_frame();
    if (!efx_input_gamepad_button_is_released(0, EFX_GPB_SOUTH)) {
        return fail("full release");
    }
    efx_input_end_frame();

    efx_input_gamepad_inject_disconnect(0);
    efx_input_begin_frame();
    if (efx_input_gamepad_count() != 0 ||
        efx_input_gamepad_disconnect_count() != 1) {
        return fail("full disconnect");
    }
    return 0;
}

/* the bulk mapping loader and the injection reset seam (kept for tests) */
static int gp_seams(void) {
    efx_input_reset();
    efx_input_gamepad_clear_mappings();
    const char *lines[1] = {
        "33330000000000000000000000000000,Seam,a:b0,"
    };
    if (efx_input_gamepad_load_mappings(lines, 1) != 1) {
        return fail("gamepad bulk mapping load");
    }
    /* inject_clear cancels a staged descriptor before the poll sees it */
    efx_input_gamepad_inject_connect(0, "Seam",
                                     "33330000000000000000000000000000", 0);
    efx_input_gamepad_inject_clear();
    efx_input_begin_frame();
    if (efx_input_gamepad_count() != 0 || efx_input_gamepad_connected(0)) {
        return fail("inject_clear cancels staged connect");
    }
    efx_input_end_frame();
    /* the bulk-loaded mapping still drives the semantic surface */
    unsigned char btns[1] = {1};
    efx_input_gamepad_inject_connect(0, "Seam",
                                     "33330000000000000000000000000000", 0);
    efx_input_gamepad_inject_state(0, 1, btns, 0, NULL);
    efx_input_begin_frame();
    if (!efx_input_gamepad_mapped(0) ||
        !efx_input_gamepad_button_is_down(0, EFX_GPB_SOUTH)) {
        return fail("bulk mapping drives surface");
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: efx_input_tests <case>\n");
        return 2;
    }
    const char *c = argv[1];
    if (!strcmp(c, "names")) return names();
    if (!strcmp(c, "level_edge")) return level_edge();
    if (!strcmp(c, "repeat")) return repeat();
    if (!strcmp(c, "ordering")) return ordering();
    if (!strcmp(c, "deltas")) return deltas();
    if (!strcmp(c, "mouse")) return mouse();
    if (!strcmp(c, "focus")) return focus();
    if (!strcmp(c, "chars")) return chars();
    if (!strcmp(c, "window")) return window();
    if (!strcmp(c, "mods")) return mods();
    if (!strcmp(c, "gp_names")) return gp_names();
    if (!strcmp(c, "gp_edges")) return gp_edges();
    if (!strcmp(c, "gp_hotplug")) return gp_hotplug();
    if (!strcmp(c, "gp_mapping")) return gp_mapping();
    if (!strcmp(c, "gp_half_invert")) return gp_half_invert();
    if (!strcmp(c, "gp_hat")) return gp_hat();
    if (!strcmp(c, "gp_dpad_buttons")) return gp_dpad_buttons();
    if (!strcmp(c, "gp_ranges")) return gp_ranges();
    if (!strcmp(c, "gp_raw")) return gp_raw();
    if (!strcmp(c, "gp_guid_fallback")) return gp_guid_fallback();
    if (!strcmp(c, "gp_full")) return gp_full();
    if (!strcmp(c, "gp_seams")) return gp_seams();
    fprintf(stderr, "unknown case: %s\n", c);
    return 2;
}
