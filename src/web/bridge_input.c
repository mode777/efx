#include "bridge_internal.h"

/* F9/F13: name lookups, level/edge queries, the event count/clear and the
 * gamepad accessors are core functions exported directly
 * (EFX_WEB_CORE_EXPORTS in CMakeLists.txt). These batch the per-field reads
 * into one call each, as doubles so every int and float stays exact. */

/* one queued event: type, key, button, repeat, mods, codepoint, x, y, dx, dy
 * (type -1 when out of range) */
EMSCRIPTEN_KEEPALIVE void efx_bridge_input_event(int index, double *out) {
    const efx_input_event *ev = efx_input_event_at(index);
    if (!ev) {
        out[0] = -1;
        return;
    }
    const double v[10] = {ev->type, ev->key,       ev->button, ev->repeat,
                          ev->mods, ev->codepoint, ev->x,      ev->y,
                          ev->dx,   ev->dy};
    memcpy(out, v, sizeof(v));
}

/* x, y, dx, dy, wheel dx, wheel dy, window width, height, dpi scale */
EMSCRIPTEN_KEEPALIVE void efx_bridge_input_state(double *out) {
    float x = 0, y = 0, dx = 0, dy = 0, wx = 0, wy = 0, dpi = 1;
    int w = 0, h = 0;
    efx_input_pointer(&x, &y);
    efx_input_delta(&dx, &dy);
    efx_input_wheel_delta(&wx, &wy);
    efx_input_window_size(&w, &h, &dpi);
    const double v[9] = {x, y, dx, dy, wx, wy, w, h, dpi};
    memcpy(out, v, sizeof(v));
}

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_gamepad_name(int slot) {
    const char *n = efx_input_gamepad_name(slot);
    return n ? n : "";
}

