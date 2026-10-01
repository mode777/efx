/*
 * Private contract shared by the src/web/bridge_*.c fragments (P10). The
 * public surface is web/web.h; this header holds the process-wide bridge
 * state and the one helper that crosses fragment boundaries.
 */
#ifndef EFX_WEB_BRIDGE_INTERNAL_H
#define EFX_WEB_BRIDGE_INTERNAL_H

#include <emscripten.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/platform.h"
#include "platform/audio_backend.h"
#include "audio/audio.h"
#include "physics/physics.h"
#include "physics/broadphase.h"
#include "render/render.h"
#include "input/efx_input.h"
#include "input/efx_gamepad.h"
#include "prelude/prelude.h"
#include "resource/gltf.h"
#include "resource/image.h"
#include "resource/resource.h"
#include "render/text.h"
#include "web/web.h"

#define EFX_WEB_ROOT_MAX 512

/* process-wide bridge state (formerly the anonymous `W` in bridge.c) */
typedef struct {
    int quit_requested;
    int quit_code;
    int in_error;
    char **args;
    int arg_count;
    char root[EFX_WEB_ROOT_MAX];
    int dom;
    int golden_mode;
    int repl_requested;
    efx_platform_capture capture;
    double frame_last_now;
    int frame_have_now;
    efx_resource *resource; /* F6a provider for the current root */
    efx_physics_world *physics; /* F12 single world */
} efx_web_state;

extern efx_web_state W;

/* opened by efx_web_main (bridge_core.c) and efx_bridge_set_root (resource) */
void web_open_root(void);

/* bridge entry points called across fragments (defined elsewhere, external) */
int efx_bridge_imagedata_commit(int w, int h, uint8_t *pixels);

#endif
