#ifndef EFX_INPUT_H
#define EFX_INPUT_H

/*
 * F9 input core (design D1/D2/D5): a pure-C keyboard/mouse state model with
 * frame-staged event delivery. No Sokol, no quickjs (ADR 0003 module walls).
 * The platform layer translates the backend's event callback into the
 * arrival functions below; both script bindings and the tests consume the
 * same core.
 *
 * Key ids are stable engine integers that intentionally use the platform's
 * virtual-keycode values (the GLFW/Sokol keycode contract, identical on all
 * four backends) so the platform translation is a direct cast. Script-visible
 * identifiers are the engine-owned lowercase names in the lookup table
 * (efx_input_key_id / efx_input_button_id); the numeric ids never reach a
 * script.
 */

#include <stdint.h>

/* modifier bits (arrival + event objects); values mirror the platform
 * modifier contract so the platform translation is a mask copy */
#define EFX_INPUT_MOD_SHIFT 0x1u
#define EFX_INPUT_MOD_CTRL 0x2u
#define EFX_INPUT_MOD_ALT 0x4u
#define EFX_INPUT_MOD_SUPER 0x8u

/* event types carried in the frame queue */
#define EFX_INPUT_KEY_DOWN 0
#define EFX_INPUT_KEY_UP 1
#define EFX_INPUT_CHAR 2
#define EFX_INPUT_MOUSE_DOWN 3
#define EFX_INPUT_MOUSE_UP 4
#define EFX_INPUT_MOUSE_MOVE 5
#define EFX_INPUT_WHEEL 6

/* mouse buttons (stable ids; names 'left'/'right'/'middle') */
#define EFX_INPUT_MOUSE_LEFT 0
#define EFX_INPUT_MOUSE_RIGHT 1
#define EFX_INPUT_MOUSE_MIDDLE 2
#define EFX_INPUT_MOUSE_MAX 3

/* per-frame event queue capacity (fixed; overflow events are dropped) */
#define EFX_INPUT_QUEUE_MAX 1024

/* one queued input event (value snapshot taken on arrival) */
typedef struct efx_input_event {
    int type;         /* EFX_INPUT_* */
    int key;          /* key id for KEY_DOWN/KEY_UP */
    int button;       /* EFX_INPUT_MOUSE_* for MOUSE_DOWN/UP */
    int repeat;       /* key-down auto-repeat flag */
    unsigned mods;    /* EFX_INPUT_MOD_* */
    uint32_t codepoint; /* CHAR */
    float x, y;       /* pointer position (mouse events) */
    float dx, dy;     /* movement or wheel delta */
} efx_input_event;

/* frame lifecycle (called by the platform frame loop) */
void efx_input_begin_frame(void);
void efx_input_end_frame(void);
void efx_input_reset(void);

/* arrival: level state updates immediately, event is queued (platform path
 * and the injection seam share these) */
void efx_input_key_down(int key, int repeat, unsigned mods);
void efx_input_key_up(int key, unsigned mods);
void efx_input_char(uint32_t codepoint);
void efx_input_mouse_down(int button, float x, float y, unsigned mods);
void efx_input_mouse_up(int button, float x, float y, unsigned mods);
void efx_input_mouse_move(float x, float y, float dx, float dy);
void efx_input_wheel(float dx, float dy);
void efx_input_focus_lost(void);
void efx_input_set_window(int w, int h, float dpi_scale);

/* level / edge queries (edges valid for one frame) */
int efx_input_key_is_down(int key);
int efx_input_key_is_pressed(int key);
int efx_input_key_is_released(int key);
int efx_input_button_is_down(int button);
int efx_input_button_is_pressed(int button);
int efx_input_button_is_released(int button);
void efx_input_pointer(float *out_x, float *out_y);
void efx_input_delta(float *out_dx, float *out_dy);
void efx_input_wheel_delta(float *out_dx, float *out_dy);
void efx_input_window_size(int *out_w, int *out_h, float *out_dpi_scale);

/* engine-owned name lookup (single source for both bindings); -1/NULL when
 * the name/id is not in the documented set */
int efx_input_key_id(const char *name);
const char *efx_input_key_name(int key);
int efx_input_button_id(const char *name);
const char *efx_input_button_name(int button);
const char *efx_input_mod_name(unsigned bit); /* bit = EFX_INPUT_MOD_* */

/* frame event queue (drained by the binding before the update hooks) */
int efx_input_event_count(void);
const efx_input_event *efx_input_event_at(int index);
void efx_input_clear_events(void);

/* deterministic injection seam (tests only; never script-visible) */
void efx_input_inject_key(int key, int down, int repeat, unsigned mods);
void efx_input_inject_char(uint32_t codepoint);
void efx_input_inject_mouse_button(int button, int down, float x, float y,
                                   unsigned mods);
void efx_input_inject_mouse_move(float x, float y, float dx, float dy);
void efx_input_inject_wheel(float dx, float dy);

#endif
