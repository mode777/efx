/*
 * Headless unit tests for the F9 input core (no window, no script runtime):
 * name lookup, level/edge semantics, frame staging, arrival ordering,
 * per-frame deltas, focus clearing, and the injection seam.
 * Usage: efx_input_tests <case> ; exit 0 = pass.
 */
#include "input/efx_input.h"

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
    fprintf(stderr, "unknown case: %s\n", c);
    return 2;
}
