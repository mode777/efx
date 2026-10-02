#define SOKOL_IMPL
#define SOKOL_NO_ENTRY

#if defined(_WIN32)
#define SOKOL_D3D11
#elif defined(__APPLE__)
#define SOKOL_METAL
#elif defined(__EMSCRIPTEN__)
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif

#include <stdio.h>
#include <string.h>

#if defined(__APPLE__)
#include <unistd.h> /* _exit */
#endif

#include "platform/platform.h"
#include "platform/pipeline.h"
#include "platform/capture.h"
#include "platform/gamepad_backend.h"
#include "platform/audio_backend.h"
#include "render/render.h"
#include "input/input.h"

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

/* fixed virtual frame for golden captures (ADR 0020) */
#define EFX_CAP_W 640
#define EFX_CAP_H 480

static efx_frame_hooks g_hooks;
static efx_platform_capture g_capture;
static int g_frame;
static int g_exit_code;

#ifdef SOKOL_METAL
/* capture pass renders into an injected Managed texture instead of the
   framebuffer-only swapchain drawable */
static sg_image g_cap_img;
static sg_view g_cap_view;
static sg_image g_cap_depth_img;
static sg_view g_cap_depth_view;
static sg_attachments g_cap_atts;
static void *g_cap_mtl;
static void *g_cap_depth_mtl;
static int g_cap_active;
#endif

/* surface sokol validation/creation errors on stderr (they are silent
   otherwise and render as inexplicable missing geometry) */
static void efx_sokol_log(const char *tag, uint32_t level,
                          uint32_t item_id, const char *message,
                          uint32_t line_nr, const char *filename,
                          void *ud) {
    (void)item_id;
    (void)ud;
    if (level <= 2) { /* panic + error + warning */
        fprintf(stderr, "sokol[%s] %s:%u: %s\n", tag,
                filename ? filename : "?", line_nr,
                message ? message : "<no message>");
        fflush(stderr);
    }
}

#ifdef SOKOL_METAL
static void efx_capture_setup(void) {
    const void *dev = sapp_get_environment().metal.device;
    if (!dev) {
        return;
    }
    id<MTLDevice> mtl = (__bridge id<MTLDevice>)dev;
    /* BGRA8: the sapp swapchain format — matches the environment default
       the pipelines are built with */
    MTLTextureDescriptor *td = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                     width:EFX_CAP_W height:EFX_CAP_H
                                 mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget;
    td.storageMode = MTLStorageModeManaged;
    id<MTLTexture> tex = [mtl newTextureWithDescriptor:td];
    if (!tex) {
        return;
    }
    g_cap_mtl = (__bridge_retained void *)tex;
    efx_capture_metal_set_texture(g_cap_mtl);
    g_cap_img = sg_make_image(&(sg_image_desc){
        .width = EFX_CAP_W,
        .height = EFX_CAP_H,
        .pixel_format = SG_PIXELFORMAT_BGRA8, /* must match the MTL texture and the pipelines' env-default color format */
        .usage.color_attachment = true,
        .mtl_textures[0] = g_cap_mtl,
    });
    g_cap_view = sg_make_view(&(sg_view_desc){
        .color_attachment.image = g_cap_img,
    });
    /* depth attachment so golden captures depth-test like the window pass */
    MTLTextureDescriptor *dd = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                     width:EFX_CAP_W height:EFX_CAP_H
                                 mipmapped:NO];
    dd.usage = MTLTextureUsageRenderTarget;
    dd.storageMode = MTLStorageModePrivate;
    id<MTLTexture> dtex = [mtl newTextureWithDescriptor:dd];
    if (!dtex) {
        return;
    }
    g_cap_depth_mtl = (__bridge_retained void *)dtex;
    g_cap_depth_img = sg_make_image(&(sg_image_desc){
        .width = EFX_CAP_W,
        .height = EFX_CAP_H,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .usage.depth_stencil_attachment = true,
        .mtl_textures[0] = g_cap_depth_mtl,
    });
    g_cap_depth_view = sg_make_view(&(sg_view_desc){
        .depth_stencil_attachment.image = g_cap_depth_img,
    });
    memset(&g_cap_atts, 0, sizeof(g_cap_atts));
    g_cap_atts.colors[0] = g_cap_view;
    g_cap_atts.depth_stencil = g_cap_depth_view;
    g_cap_active = 1;
}
#endif

/* F9 input capture: the single platform event callback on all four targets
 * translates backend events into the pure-C input core (ADR 0036). Sokol's
 * modifier constants share the engine's modifier bit layout. */
static unsigned efx_input_mods(uint32_t mods) {
    unsigned out = 0;
    if (mods & SAPP_MODIFIER_SHIFT) {
        out |= EFX_INPUT_MOD_SHIFT;
    }
    if (mods & SAPP_MODIFIER_CTRL) {
        out |= EFX_INPUT_MOD_CTRL;
    }
    if (mods & SAPP_MODIFIER_ALT) {
        out |= EFX_INPUT_MOD_ALT;
    }
    if (mods & SAPP_MODIFIER_SUPER) {
        out |= EFX_INPUT_MOD_SUPER;
    }
    return out;
}

#if defined(__EMSCRIPTEN__)
/* game keys whose browser default (scrolling, focus movement) must be
 * suppressed while the canvas has focus; plain modifiers are left alone so
 * browser shortcuts keep working (ADR 0043) */
static int efx_web_game_key(int key) {
    switch (key) {
    case 32:  /* space */
    case 258: /* tab */
    case 259: /* backspace */
    case 262:
    case 263:
    case 264:
    case 265: /* arrows */
    case 266:
    case 267: /* page up/down */
    case 268:
    case 269: /* home/end */
        return 1;
    default:
        return 0;
    }
}
#endif

static void efx_event_cb(const sapp_event *e) {
    switch (e->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
#if defined(__EMSCRIPTEN__)
        if (efx_web_game_key((int)e->key_code) &&
            !(e->modifiers & (SAPP_MODIFIER_CTRL | SAPP_MODIFIER_ALT |
                              SAPP_MODIFIER_SUPER))) {
            sapp_consume_event();
        }
#endif
        efx_input_key_down((int)e->key_code, e->key_repeat ? 1 : 0,
                           efx_input_mods(e->modifiers));
        break;
    case SAPP_EVENTTYPE_KEY_UP:
        efx_input_key_up((int)e->key_code, efx_input_mods(e->modifiers));
        break;
    case SAPP_EVENTTYPE_CHAR:
        efx_input_char(e->char_code);
        break;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
        efx_input_mouse_down((int)e->mouse_button, e->mouse_x, e->mouse_y,
                             efx_input_mods(e->modifiers));
        break;
    case SAPP_EVENTTYPE_MOUSE_UP:
        efx_input_mouse_up((int)e->mouse_button, e->mouse_x, e->mouse_y,
                           efx_input_mods(e->modifiers));
        break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        efx_input_mouse_move(e->mouse_x, e->mouse_y, e->mouse_dx, e->mouse_dy);
        break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        efx_input_wheel(e->scroll_x, e->scroll_y);
        break;
    case SAPP_EVENTTYPE_UNFOCUSED:
        efx_input_focus_lost();
        break;
    case SAPP_EVENTTYPE_RESIZED:
        if (e->window_width > 0) {
            efx_input_set_window(e->framebuffer_width, e->framebuffer_height,
                                 (float)e->framebuffer_width /
                                     (float)e->window_width);
        }
        break;
    default:
        break;
    }
}

static void efx_init_cb(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger = {.func = efx_sokol_log},
    });
    efx_pipeline_install();
    /* F13: initialize the gamepad poll backend once, before the frame loop */
    efx_gamepad_backend_init();
    /* F14: initialize the audio device (push model); idempotent, soft-fails
       when no device is available */
    efx_audio_backend_init();
#ifdef SOKOL_METAL
    if (g_capture.frame > 0) {
        efx_capture_setup();
    }
#endif
    /* the rendering surface and engine subsystems are ready: make the window
       size visible to the script and run the entry evaluation, which now
       happens after the surface exists (ADR 0016) */
    efx_input_set_window(sapp_width(), sapp_height(), sapp_dpi_scale());
    if (g_hooks.on_init && g_hooks.on_init(g_hooks.ud)) {
#if defined(__APPLE__)
        _exit(g_exit_code);
#else
        sapp_quit();
#endif
    }
}

static void efx_frame_cb(void) {
    g_frame++;
    double dt = (g_frame == 1) ? 0.0 : sapp_frame_duration();
    efx_input_set_window(sapp_width(), sapp_height(), sapp_dpi_scale());
    efx_input_begin_frame();
    efx_render_begin_frame();
    efx_render_set_viewport(sapp_width(), sapp_height());
    if (g_hooks.on_frame && g_hooks.on_frame(g_hooks.ud, dt)) {
#if defined(__APPLE__)
        /* macOS: sokol's [NSApp run] never returns (AppKit terminates the
           process). Exit here with the recorded code so a windowed
           efx.quit(n)/REPL quit keeps its exit status instead of 0. */
        _exit(g_exit_code);
#endif
        sapp_quit();
        return;
    }

#ifdef SOKOL_METAL
    /* the default segment renders into the capture attachments on the
       capture frame; playback owns its passes (pipeline.h) */
    static sg_attachments no_atts;
    memset(&no_atts, 0, sizeof(no_atts));
    efx_pipeline_set_default_attachments(
        (g_cap_active && g_frame == g_capture.frame) ? (void *)&g_cap_atts
                                                     : (void *)&no_atts);
#endif

    efx_pipeline_play();
    sg_commit();
    efx_render_end_frame();
    efx_input_end_frame();
    /* F14: pump the music decoder and push mixed audio for this frame */
    efx_audio_backend_frame();

#if defined(__EMSCRIPTEN__)
    if (g_capture.frame > 0 && g_frame >= g_capture.frame) {
        /* web: read back via canvas.toDataURL (native glReadPixels from the
           default framebuffer crashes headless shells with SwiftShader) */
        EM_ASM({
            try {
                const url = document.getElementById('canvas').toDataURL('image/png');
                Module['webGoldenCapture'] = url.substring(url.indexOf(',') + 1);
            } catch (e) {
                console.error('golden capture export failed:', e);
            }
        });
        sapp_quit();
    }
#else
    if (g_capture.frame > 0 && g_frame >= g_capture.frame) {
        uint8_t *px = NULL;
        int w = 0, h = 0;
        if (efx_capture_read_rgba(&px, &w, &h) == 0) {
            if (efx_capture_write_png(g_capture.output, w, h, px) != 0) {
                fprintf(stderr, "player: capture PNG write failed: %s\n", g_capture.output);
                fflush(stderr);
            }
            free(px);
        } else {
            fprintf(stderr, "player: capture readback failed\n");
            fflush(stderr);
        }
        sapp_quit();
    }
#endif
}

static void efx_cleanup_cb(void) {
    efx_gamepad_backend_shutdown();
    efx_audio_backend_shutdown();
#ifdef SOKOL_METAL
    if (g_cap_mtl) {
        id<MTLTexture> tex = (__bridge id<MTLTexture>)g_cap_mtl;
        [tex release];
        g_cap_mtl = NULL;
    }
#endif
#if defined(__EMSCRIPTEN__)
    /* the web loop has no C caller after sapp_run returns (ADR 0022): the
       full render-stack teardown happens here, when the sokol loop ends */
    efx_render_end_frame();
    efx_render_shutdown();
    efx_pipeline_shutdown();
    sg_shutdown();
#else
    /* sg_shutdown is deferred to efx_platform_shutdown() so callers can
       release their GPU resources first */
#endif
}

void efx_platform_set_exit_code(int code) {
    g_exit_code = code;
}

int efx_platform_run(const efx_platform_desc *desc, efx_frame_hooks hooks) {
    g_hooks = hooks;
    g_exit_code = 0;
    memset(&g_capture, 0, sizeof(g_capture));
    if (desc) {
        g_capture = desc->capture;
    }
    g_frame = 0;
    sapp_desc d;
    memset(&d, 0, sizeof(d));
    d.init_cb = efx_init_cb;
    d.frame_cb = efx_frame_cb;
    d.cleanup_cb = efx_cleanup_cb;
    d.event_cb = efx_event_cb;
    d.width = (desc && desc->width > 0) ? desc->width : 1024;
    d.height = (desc && desc->height > 0) ? desc->height : 600;
    if (g_capture.frame > 0) {
        d.width = EFX_CAP_W;
        d.height = EFX_CAP_H;
    }
    d.window_title = "EFX";
#if defined(__EMSCRIPTEN__)
    if (g_capture.frame > 0) {
        d.html5.preserve_drawing_buffer = true; /* canvas readback after commit */
    }
    /* Keyboard listeners live on the embedding document's window (Sokol's web
       backend), so keys only arrive once that document has focus. Consuming
       mouse events (the Sokol default) calls preventDefault() on mousedown,
       which cancels the browser's focus transfer and leaves an iframe embed
       unfocused. Let pointer events bubble so native focus-on-click works
       (ADR 0043); key/char default suppression is independent and stays on. */
    d.html5.bubble_mouse_events = true;
#endif
    sapp_run(&d);
    return 0;
}

void efx_platform_shutdown(void) {
    efx_pipeline_shutdown();
#ifdef SOKOL_METAL
    if (g_cap_active) {
        sg_destroy_view(g_cap_view);
        sg_destroy_image(g_cap_img);
        sg_destroy_view(g_cap_depth_view);
        sg_destroy_image(g_cap_depth_img);
        g_cap_active = 0;
    }
    if (g_cap_depth_mtl) {
        id<MTLTexture> dtex = (__bridge id<MTLTexture>)g_cap_depth_mtl;
        [dtex release];
        g_cap_depth_mtl = NULL;
    }
#endif
    sg_shutdown();
}
