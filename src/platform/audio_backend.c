/*
 * F14 platform audio backend (design D2, push model).
 *
 * sokol_audio is compiled here (SOKOL_AUDIO_IMPL) and confined to efx_platform;
 * the engine's pure-C core (src/audio/) never sees it. The device is fed in
 * push mode: once per frame we pump the streaming music decoder, mix a block
 * with efx_audio_mix, and saudio_push it. No callback, no threads.
 *
 * No-device soft-fail: when saudio_setup fails or no device is present the
 * player keeps running silently. On web the browser suspends the AudioContext
 * until a user gesture; sokol_audio resumes it on the first DOM gesture, and
 * efx_audio_backend_resume() additionally attempts an explicit resume.
 */

#define SOKOL_AUDIO_IMPL
#include "sokol_audio.h"

#include "platform/audio_backend.h"

#include <string.h>

#include "audio/audio.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

/* device push block; sokol_audio's own queue is ~buffer_frames */
#define EFX_AUDIO_BACKEND_MIX_FRAMES 4096

static int g_up;
static float g_mix[EFX_AUDIO_BACKEND_MIX_FRAMES * 2];

static void update_availability(void) {
    int avail = saudio_isvalid() ? 1 : 0;
    if (avail && saudio_suspended()) {
        avail = 0;
    }
    efx_audio_set_available(avail);
}

void efx_audio_backend_init(void) {
    if (g_up) {
        return;
    }
    saudio_setup(&(saudio_desc){
        .sample_rate = 0,       /* backend default */
        .num_channels = 2,
        .buffer_frames = 4096,  /* ~93ms of slack against frame hitches */
    });
    int rate = EFX_AUDIO_DEFAULT_RATE;
    if (saudio_isvalid() && saudio_sample_rate() > 0) {
        rate = saudio_sample_rate();
    }
    efx_audio_init(rate);
    efx_audio_set_resume_callback(efx_audio_backend_resume, NULL);
    update_availability();
    g_up = 1;
}

void efx_audio_backend_frame(void) {
    if (!g_up) {
        return;
    }
    update_availability();
    if (!saudio_isvalid()) {
        return;
    }
    efx_audio_music_pump();
    int budget = 8; /* bounded; the queue is at most buffer_frames deep */
    while (budget-- > 0) {
        int want = saudio_expect();
        if (want <= 0) {
            break;
        }
        if (want > EFX_AUDIO_BACKEND_MIX_FRAMES) {
            want = EFX_AUDIO_BACKEND_MIX_FRAMES;
        }
        efx_audio_mix(g_mix, want);
        int pushed = saudio_push(g_mix, want);
        if (pushed < want) {
            break;
        }
    }
}

void efx_audio_backend_resume(void) {
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        try {
            var c = Module['_saudio_context'];
            if (c && (c.state === 'suspended' || c.state === 'interrupted')) {
                c.resume().catch(function () {});
            }
        } catch (e) {
            /* no context yet: sokol_audio resumes on the first DOM gesture */
        }
    });
#endif
    update_availability();
}

void efx_audio_backend_shutdown(void) {
    if (!g_up) {
        return;
    }
    efx_audio_shutdown();
    if (saudio_isvalid()) {
        saudio_shutdown();
    }
    g_up = 0;
}
