/*
 * F13 platform gamepad backend.
 *
 * The vendored minigamepad snapshot is the single poll backend on all four
 * targets (design D1). It is included exactly once here, with MG_API left
 * empty so its symbols stay local to this translation unit, and it is never
 * linked into the pure-C core or the headless tests (design D2).
 *
 * minigamepad already reconstructs the platform raw layout into its semantic
 * mg_button/mg_axis enums (its GLFW-style GUID generation plus platform
 * mapping tables); this backend re-encodes that into the engine's canonical
 * standard descriptor, so the core's normalization boundary is uniform on
 * every target. Local patches to the vendored snapshot fix the web
 * enumeration and web right-trigger defects (see vendor/README.md).
 */

#define MG_IMPLEMENTATION
#define MG_MAX_GAMEPADS 4
#define MG_API

#include "platform/gamepad_backend.h"

#include <string.h>

#include "input/efx_gamepad.h"

#if defined(_WIN32)
/* minigamepad's Windows section includes <xinput.h>/<dinput.h>, which expect
 * the base Win32 headers to be present first (otherwise the SDK's winnt.h
 * raises "No Target Architecture"). */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "minigamepad.h"

#if defined(__linux__)
#include <unistd.h>
#endif

static mg_gamepads g_gamepads;
static int g_ready;

/* minigamepad's mg_button order differs from the engine's canonical standard
 * button order; map it explicitly. mg_axis order already matches. */
static const int MG_TO_CANONICAL[EFX_GPB_COUNT] = {
    MG_BUTTON_SOUTH,        /* EFX_GPB_SOUTH */
    MG_BUTTON_EAST,         /* EFX_GPB_EAST */
    MG_BUTTON_WEST,         /* EFX_GPB_WEST */
    MG_BUTTON_NORTH,        /* EFX_GPB_NORTH */
    MG_BUTTON_LEFT_SHOULDER,  /* EFX_GPB_LEFT_SHOULDER */
    MG_BUTTON_RIGHT_SHOULDER, /* EFX_GPB_RIGHT_SHOULDER */
    MG_BUTTON_LEFT_TRIGGER,   /* EFX_GPB_LEFT_TRIGGER */
    MG_BUTTON_RIGHT_TRIGGER,  /* EFX_GPB_RIGHT_TRIGGER */
    MG_BUTTON_BACK,         /* EFX_GPB_BACK */
    MG_BUTTON_START,        /* EFX_GPB_START */
    MG_BUTTON_GUIDE,        /* EFX_GPB_GUIDE */
    MG_BUTTON_LEFT_STICK,   /* EFX_GPB_LEFT_STICK */
    MG_BUTTON_RIGHT_STICK,  /* EFX_GPB_RIGHT_STICK */
    MG_BUTTON_DPAD_UP,      /* EFX_GPB_DPAD_UP */
    MG_BUTTON_DPAD_DOWN,    /* EFX_GPB_DPAD_DOWN */
    MG_BUTTON_DPAD_LEFT,    /* EFX_GPB_DPAD_LEFT */
    MG_BUTTON_DPAD_RIGHT,   /* EFX_GPB_DPAD_RIGHT */
};

static int backend_poll(efx_gamepad_device *out, int max, void *ud) {
    (void)ud;
    if (!g_ready) {
        return -1;
    }

    mg_gamepads_poll(&g_gamepads);

    int n = 0;
    for (mg_gamepad *cur = g_gamepads.list.head; cur && n < max;
         cur = cur->next) {
        efx_gamepad_device *d = &out[n];
        memset(d, 0, sizeof(*d));
        d->connected = 1;
        d->normalized = 1;
        d->mapped_hint = 1;
        strncpy(d->name, cur->name, sizeof(d->name) - 1);

        d->raw_button_count = EFX_GPB_COUNT;
        for (int b = 0; b < EFX_GPB_COUNT; b++) {
            int mgb = MG_TO_CANONICAL[b];
            d->raw_buttons[b] =
                (unsigned char)(cur->buttons[mgb].current ? 1 : 0);
        }

        d->raw_axis_count = EFX_GPA_COUNT;
        for (int a = 0; a < EFX_GPA_COUNT; a++) {
            float v = cur->axes[a].value;
            if (a == EFX_GPA_LEFT_TRIGGER || a == EFX_GPA_RIGHT_TRIGGER) {
                /* minigamepad's desktop paths report triggers in -1..1; the
                 * web path already reports 0..1. Collapse to the canonical
                 * 0..1 trigger range (design D6). */
#if defined(__EMSCRIPTEN__)
                d->raw_axes[a] = v;
#else
                d->raw_axes[a] = (v + 1.0f) * 0.5f;
#endif
            } else {
                d->raw_axes[a] = v;
            }
        }
        n++;
    }
    return n;
}

void efx_gamepad_backend_init(void) {
    if (g_ready) {
        return;
    }
#if defined(__linux__)
    /* avoid minigamepad's "Can't open /dev/input/" stderr noise on hosts
       without an input device tree (CI, headless captures) */
    if (access("/dev/input", F_OK) != 0) {
        return;
    }
#endif
    memset(&g_gamepads, 0, sizeof(g_gamepads));
    mg_gamepads_init(&g_gamepads);
    g_ready = 1;
    efx_input_gamepad_set_source(backend_poll, NULL);
}

void efx_gamepad_backend_shutdown(void) {
    if (!g_ready) {
        return;
    }
    efx_input_gamepad_set_source(NULL, NULL);
    mg_gamepads_free(&g_gamepads);
    g_ready = 0;
}
