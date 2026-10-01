#include "input/input.h"
#include "input/gamepad.h"

#include <string.h>

/* largest key id we track (keycode values reach 348; round up) */
#define EFX_INPUT_KEY_MAX 512

typedef struct {
    const char *name;
    int id;
} efx_input_name_entry;

/* The documented key-name set (design D3 open question settled here). The
 * numeric ids are the platform virtual-keycode values; the table is the
 * engine-owned name source of truth for scripts. Every name here is
 * exercised by the input unit test. */
static const efx_input_name_entry KEY_NAMES[] = {
    {"space", 32},
    {"apostrophe", 39},
    {"comma", 44},
    {"minus", 45},
    {"period", 46},
    {"slash", 47},
    {"0", 48}, {"1", 49}, {"2", 50}, {"3", 51}, {"4", 52},
    {"5", 53}, {"6", 54}, {"7", 55}, {"8", 56}, {"9", 57},
    {"semicolon", 59},
    {"equal", 61},
    {"a", 65}, {"b", 66}, {"c", 67}, {"d", 68}, {"e", 69},
    {"f", 70}, {"g", 71}, {"h", 72}, {"i", 73}, {"j", 74},
    {"k", 75}, {"l", 76}, {"m", 77}, {"n", 78}, {"o", 79},
    {"p", 80}, {"q", 81}, {"r", 82}, {"s", 83}, {"t", 84},
    {"u", 85}, {"v", 86}, {"w", 87}, {"x", 88}, {"y", 89}, {"z", 90},
    {"leftbracket", 91},
    {"backslash", 92},
    {"rightbracket", 93},
    {"grave", 96},
    {"escape", 256},
    {"enter", 257},
    {"tab", 258},
    {"backspace", 259},
    {"insert", 260},
    {"delete", 261},
    {"right", 262},
    {"left", 263},
    {"down", 264},
    {"up", 265},
    {"pageup", 266},
    {"pagedown", 267},
    {"home", 268},
    {"end", 269},
    {"capslock", 280},
    {"scrolllock", 281},
    {"numlock", 282},
    {"printscreen", 283},
    {"pause", 284},
    {"f1", 290}, {"f2", 291}, {"f3", 292}, {"f4", 293},
    {"f5", 294}, {"f6", 295}, {"f7", 296}, {"f8", 297},
    {"f9", 298}, {"f10", 299}, {"f11", 300}, {"f12", 301},
    {"kp0", 320}, {"kp1", 321}, {"kp2", 322}, {"kp3", 323},
    {"kp4", 324}, {"kp5", 325}, {"kp6", 326}, {"kp7", 327},
    {"kp8", 328}, {"kp9", 329},
    {"kpdecimal", 330},
    {"kpdivide", 331},
    {"kpmultiply", 332},
    {"kpsubtract", 333},
    {"kpadd", 334},
    {"kpenter", 335},
    {"kpequal", 336},
    {"lshift", 340},
    {"lctrl", 341},
    {"lalt", 342},
    {"lsuper", 343},
    {"rshift", 344},
    {"rctrl", 345},
    {"ralt", 346},
    {"rsuper", 347},
    {"menu", 348},
};

#define KEY_NAME_COUNT (int)(sizeof(KEY_NAMES) / sizeof(KEY_NAMES[0]))

static const efx_input_name_entry BUTTON_NAMES[] = {
    {"left", EFX_INPUT_MOUSE_LEFT},
    {"right", EFX_INPUT_MOUSE_RIGHT},
    {"middle", EFX_INPUT_MOUSE_MIDDLE},
};

#define BUTTON_NAME_COUNT (int)(sizeof(BUTTON_NAMES) / sizeof(BUTTON_NAMES[0]))

static struct {
    unsigned char down[EFX_INPUT_KEY_MAX];
    unsigned char pressed[EFX_INPUT_KEY_MAX];
    unsigned char released[EFX_INPUT_KEY_MAX];
} K;

static struct {
    unsigned char down[EFX_INPUT_MOUSE_MAX];
    unsigned char pressed[EFX_INPUT_MOUSE_MAX];
    unsigned char released[EFX_INPUT_MOUSE_MAX];
} B;

static struct {
    float x, y;        /* current pointer position (surface px) */
    float frame_dx, frame_dy;   /* committed per-frame movement */
    float accum_dx, accum_dy;   /* movement arriving between frames */
    float frame_wx, frame_wy;   /* committed per-frame wheel */
    float accum_wx, accum_wy;   /* wheel arriving between frames */
    int w, h;          /* surface size */
    float dpi_scale;
} P = {.dpi_scale = 1.0f};

static efx_input_event EVENTS[EFX_INPUT_QUEUE_MAX];
static int event_count;

/* ------------------------------------------------------- name lookup */

static int name_to_id(const efx_input_name_entry *t, int n, const char *name) {
    for (int i = 0; name && i < n; i++) {
        if (strcmp(t[i].name, name) == 0) {
            return t[i].id;
        }
    }
    return -1;
}

static const char *id_to_name(const efx_input_name_entry *t, int n, int id) {
    for (int i = 0; i < n; i++) {
        if (t[i].id == id) {
            return t[i].name;
        }
    }
    return NULL;
}

int efx_input_key_id(const char *name) {
    return name_to_id(KEY_NAMES, KEY_NAME_COUNT, name);
}

const char *efx_input_key_name(int key) {
    return id_to_name(KEY_NAMES, KEY_NAME_COUNT, key);
}

int efx_input_button_id(const char *name) {
    return name_to_id(BUTTON_NAMES, BUTTON_NAME_COUNT, name);
}

const char *efx_input_button_name(int button) {
    return id_to_name(BUTTON_NAMES, BUTTON_NAME_COUNT, button);
}

const char *efx_input_mod_name(unsigned bit) {
    switch (bit) {
    case EFX_INPUT_MOD_SHIFT:
        return "shift";
    case EFX_INPUT_MOD_CTRL:
        return "ctrl";
    case EFX_INPUT_MOD_ALT:
        return "alt";
    case EFX_INPUT_MOD_SUPER:
        return "super";
    default:
        return NULL;
    }
}

/* ------------------------------------------------------------ queue */

static void queue_push(const efx_input_event *ev) {
    if (event_count < EFX_INPUT_QUEUE_MAX) {
        EVENTS[event_count++] = *ev;
    }
}

int efx_input_event_count(void) {
    return event_count;
}

const efx_input_event *efx_input_event_at(int index) {
    if (index < 0 || index >= event_count) {
        return NULL;
    }
    return &EVENTS[index];
}

void efx_input_clear_events(void) {
    event_count = 0;
}

/* --------------------------------------------------------- lifecycle */

void efx_input_reset(void) {
    memset(&K, 0, sizeof(K));
    memset(&B, 0, sizeof(B));
    memset(&P, 0, sizeof(P));
    P.dpi_scale = 1.0f;
    event_count = 0;
    efx_input_gamepad_reset();
}

void efx_input_begin_frame(void) {
    /* F13: gamepads are poll-based, so sample them at frame begin before
     * edges are finalized (design D2) */
    efx_input_gamepad_poll();
    /* commit the movement/wheel that arrived since the previous frame; the
     * level/edge state is already current (design D2) */
    P.frame_dx = P.accum_dx;
    P.frame_dy = P.accum_dy;
    P.accum_dx = 0;
    P.accum_dy = 0;
    P.frame_wx = P.accum_wx;
    P.frame_wy = P.accum_wy;
    P.accum_wx = 0;
    P.accum_wy = 0;
}

void efx_input_end_frame(void) {
    /* edges are valid for exactly one frame */
    memset(K.pressed, 0, sizeof(K.pressed));
    memset(K.released, 0, sizeof(K.released));
    memset(B.pressed, 0, sizeof(B.pressed));
    memset(B.released, 0, sizeof(B.released));
    /* F13: gamepad edges expire on the same one-frame schedule */
    efx_input_gamepad_end_frame();
    /* the queue was consumed by the binding; clear defensively */
    event_count = 0;
    /* per-frame deltas read as 0 outside a frame */
    P.frame_dx = 0;
    P.frame_dy = 0;
    P.frame_wx = 0;
    P.frame_wy = 0;
}

/* --------------------------------------------------------- arrival */

static int key_in_range(int key) {
    return key > 0 && key < EFX_INPUT_KEY_MAX;
}

static int button_in_range(int button) {
    return button >= 0 && button < EFX_INPUT_MOUSE_MAX;
}

/* level change with its one-frame edge; a repeated level is not an edge */
static void set_level(unsigned char *down, unsigned char *pressed,
                      unsigned char *released, int i, int is_down) {
    if (down[i] == is_down) {
        return;
    }
    down[i] = (unsigned char)is_down;
    (is_down ? pressed : released)[i] = 1;
}

void efx_input_key_down(int key, int repeat, unsigned mods) {
    if (!key_in_range(key)) {
        return;
    }
    set_level(K.down, K.pressed, K.released, key, 1);
    queue_push(&(efx_input_event){.type = EFX_INPUT_KEY_DOWN, .key = key,
                                  .repeat = repeat ? 1 : 0, .mods = mods});
}

void efx_input_key_up(int key, unsigned mods) {
    if (!key_in_range(key)) {
        return;
    }
    set_level(K.down, K.pressed, K.released, key, 0);
    queue_push(&(efx_input_event){.type = EFX_INPUT_KEY_UP, .key = key,
                                  .mods = mods});
}

void efx_input_char(uint32_t codepoint) {
    queue_push(&(efx_input_event){.type = EFX_INPUT_CHAR,
                                  .codepoint = codepoint});
}

static void mouse_button(int type, int button, float x, float y,
                         unsigned mods) {
    if (!button_in_range(button)) {
        return;
    }
    set_level(B.down, B.pressed, B.released, button,
              type == EFX_INPUT_MOUSE_DOWN);
    P.x = x;
    P.y = y;
    queue_push(&(efx_input_event){.type = type, .button = button, .x = x,
                                  .y = y, .mods = mods});
}

void efx_input_mouse_down(int button, float x, float y, unsigned mods) {
    mouse_button(EFX_INPUT_MOUSE_DOWN, button, x, y, mods);
}

void efx_input_mouse_up(int button, float x, float y, unsigned mods) {
    mouse_button(EFX_INPUT_MOUSE_UP, button, x, y, mods);
}

void efx_input_mouse_move(float x, float y, float dx, float dy) {
    P.x = x;
    P.y = y;
    P.accum_dx += dx;
    P.accum_dy += dy;
    queue_push(&(efx_input_event){.type = EFX_INPUT_MOUSE_MOVE, .x = x,
                                  .y = y, .dx = dx, .dy = dy});
}

void efx_input_wheel(float dx, float dy) {
    P.accum_wx += dx;
    P.accum_wy += dy;
    queue_push(&(efx_input_event){.type = EFX_INPUT_WHEEL, .dx = dx,
                                  .dy = dy});
}

void efx_input_focus_lost(void) {
    /* clear held state; no synthetic up events (design D7) */
    memset(K.down, 0, sizeof(K.down));
    memset(B.down, 0, sizeof(B.down));
}

void efx_input_set_window(int w, int h, float dpi_scale) {
    P.w = w;
    P.h = h;
    P.dpi_scale = dpi_scale > 0 ? dpi_scale : 1.0f;
}

/* ---------------------------------------------------------- queries */

int efx_input_key_is_down(int key) {
    return key_in_range(key) ? K.down[key] : 0;
}

int efx_input_key_is_pressed(int key) {
    return key_in_range(key) ? K.pressed[key] : 0;
}

int efx_input_key_is_released(int key) {
    return key_in_range(key) ? K.released[key] : 0;
}

int efx_input_button_is_down(int button) {
    return button_in_range(button) ? B.down[button] : 0;
}

int efx_input_button_is_pressed(int button) {
    return button_in_range(button) ? B.pressed[button] : 0;
}

int efx_input_button_is_released(int button) {
    return button_in_range(button) ? B.released[button] : 0;
}

void efx_input_pointer(float *out_x, float *out_y) {
    if (out_x) {
        *out_x = P.x;
    }
    if (out_y) {
        *out_y = P.y;
    }
}

void efx_input_delta(float *out_dx, float *out_dy) {
    if (out_dx) {
        *out_dx = P.frame_dx;
    }
    if (out_dy) {
        *out_dy = P.frame_dy;
    }
}

void efx_input_wheel_delta(float *out_dx, float *out_dy) {
    if (out_dx) {
        *out_dx = P.frame_wx;
    }
    if (out_dy) {
        *out_dy = P.frame_wy;
    }
}

void efx_input_window_size(int *out_w, int *out_h, float *out_dpi_scale) {
    if (out_w) {
        *out_w = P.w;
    }
    if (out_h) {
        *out_h = P.h;
    }
    if (out_dpi_scale) {
        *out_dpi_scale = P.dpi_scale;
    }
}

/* --------------------------------------------------------- injection */

void efx_input_inject_key(int key, int down, int repeat, unsigned mods) {
    if (down) {
        efx_input_key_down(key, repeat, mods);
    } else {
        efx_input_key_up(key, mods);
    }
}

void efx_input_inject_char(uint32_t codepoint) {
    efx_input_char(codepoint);
}

void efx_input_inject_mouse_button(int button, int down, float x, float y,
                                   unsigned mods) {
    if (down) {
        efx_input_mouse_down(button, x, y, mods);
    } else {
        efx_input_mouse_up(button, x, y, mods);
    }
}

void efx_input_inject_mouse_move(float x, float y, float dx, float dy) {
    efx_input_mouse_move(x, y, dx, dy);
}

void efx_input_inject_wheel(float dx, float dy) {
    efx_input_wheel(dx, dy);
}
