#ifndef EFX_GAMEPAD_H
#define EFX_GAMEPAD_H

/*
 * F13 gamepad core (ADR 0041): a pure-C fixed bank of gamepad
 * slots plus a portable SDL game-controller-mapping evaluator. No Sokol, no
 * quickjs, and no vendored backend types (ADR 0003 module walls). The
 * platform layer translates the vendored poll backend into device
 * descriptors through efx_input_gamepad_set_source(); tests inject synthetic
 * descriptors through the injection seam. Both paths flow through the same
 * normalization + edge staging below.
 *
 * Semantic names are engine-owned; the numeric ids never reach a script.
 */

#include <stdint.h>

/* fixed pad bank (ADR 0041) */
#define EFX_GAMEPAD_MAX 4

/* raw device layout caps (a descriptor may report fewer) */
#define EFX_GAMEPAD_RAW_BUTTON_MAX 32
#define EFX_GAMEPAD_RAW_AXIS_MAX 16
#define EFX_GAMEPAD_RAW_HAT_MAX 8 /* hat pairs (x,y); up to 4 hats */

/* semantic button ids (the documented set) */
#define EFX_GPB_SOUTH 0
#define EFX_GPB_EAST 1
#define EFX_GPB_WEST 2
#define EFX_GPB_NORTH 3
#define EFX_GPB_LEFT_SHOULDER 4
#define EFX_GPB_RIGHT_SHOULDER 5
#define EFX_GPB_LEFT_TRIGGER 6
#define EFX_GPB_RIGHT_TRIGGER 7
#define EFX_GPB_BACK 8
#define EFX_GPB_START 9
#define EFX_GPB_GUIDE 10
#define EFX_GPB_LEFT_STICK 11
#define EFX_GPB_RIGHT_STICK 12
#define EFX_GPB_DPAD_UP 13
#define EFX_GPB_DPAD_DOWN 14
#define EFX_GPB_DPAD_LEFT 15
#define EFX_GPB_DPAD_RIGHT 16
#define EFX_GPB_COUNT 17

/* semantic axis ids (the documented set) */
#define EFX_GPA_LEFT_X 0
#define EFX_GPA_LEFT_Y 1
#define EFX_GPA_RIGHT_X 2
#define EFX_GPA_RIGHT_Y 3
#define EFX_GPA_LEFT_TRIGGER 4
#define EFX_GPA_RIGHT_TRIGGER 5
#define EFX_GPA_COUNT 6

/* canonical ranges (ADR 0041) */
#define EFX_GAMEPAD_TRIGGER_THRESHOLD 0.5f

/* one device snapshot, device-agnostic (ADR 0041). When `normalized` is set
 * the raw arrays already use the SDL-standard layout (web `mapping ===
 * 'standard'` pads and the platform backend, which reconstructs the layout);
 * otherwise the evaluator selects a mapping by `guid` from the loaded SDL
 * database. Raw axes are device-native floats; raw buttons are 0/1. */
typedef struct efx_gamepad_device {
    int connected;
    int normalized;      /* raw arrays are in the canonical standard layout */
    int mapped_hint;     /* backend's own "has a mapping" hint (normalized only) */
    char name[128];
    char guid[33];
    int raw_button_count;
    unsigned char raw_buttons[EFX_GAMEPAD_RAW_BUTTON_MAX];
    int raw_axis_count;
    float raw_axes[EFX_GAMEPAD_RAW_AXIS_MAX];
    int raw_hat_count;
    float raw_hats[EFX_GAMEPAD_RAW_HAT_MAX];
} efx_gamepad_device;

/* backend source: fills up to `max` descriptors and returns the count (or a
 * negative value when no backend is available). Registered by the platform
 * layer; never reachable from scripts. */
typedef int (*efx_gamepad_source_fn)(efx_gamepad_device *out, int max, void *ud);
void efx_input_gamepad_set_source(efx_gamepad_source_fn fn, void *ud);

/* frame lifecycle: called from efx_input_begin_frame() */
void efx_input_gamepad_poll(void);
void efx_input_gamepad_reset(void);
void efx_input_gamepad_end_frame(void);

/* mapping database (SDL format; the backend loads the vendored table, tests
 * load synthetic lines). Returns the number of entries parsed. */
int efx_input_gamepad_load_mappings(const char *const *lines, int count);
int efx_input_gamepad_load_mapping_line(const char *line);
void efx_input_gamepad_clear_mappings(void);

/* connection / query surface (slots are 0..EFX_GAMEPAD_MAX-1) */
int efx_input_gamepad_count(void);
int efx_input_gamepad_connected(int slot);
const char *efx_input_gamepad_name(int slot);
int efx_input_gamepad_mapped(int slot);
int efx_input_gamepad_button_is_down(int slot, int button);
int efx_input_gamepad_button_is_pressed(int slot, int button);
int efx_input_gamepad_button_is_released(int slot, int button);
float efx_input_gamepad_axis(int slot, int axis);
int efx_input_gamepad_raw_button(int slot, int index);
float efx_input_gamepad_raw_axis(int slot, int index);

/* per-frame connection events (valid for one frame; slot indices) */
int efx_input_gamepad_connect_count(void);
int efx_input_gamepad_connect_at(int i);
int efx_input_gamepad_disconnect_count(void);
int efx_input_gamepad_disconnect_at(int i);

/* engine-owned name lookup (single source for both bindings) */
int efx_input_gamepad_button_id(const char *name);
const char *efx_input_gamepad_button_name(int button);
int efx_input_gamepad_axis_id(const char *name);
const char *efx_input_gamepad_axis_name(int axis);

/* deterministic injection seam (tests only; never script-visible). A call
 * stages a synthetic descriptor; the next poll processes it through the
 * production normalization + edge path. */
void efx_input_gamepad_inject_connect(int slot, const char *name,
                                      const char *guid, int normalized);
void efx_input_gamepad_inject_state(int slot, int raw_button_count,
                                    const unsigned char *raw_buttons,
                                    int raw_axis_count,
                                    const float *raw_axes);
void efx_input_gamepad_inject_hats(int slot, int raw_hat_count,
                                   const float *raw_hats);
void efx_input_gamepad_inject_disconnect(int slot);
void efx_input_gamepad_inject_clear(void);

#endif
