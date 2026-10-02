#include "bridge_internal.h"

efx_web_state W;

EM_JS(int, efx_web_has_dom_js, (void), {
    return (typeof document !== 'undefined') ? 1 : 0;
});

EM_JS(int, efx_web_call_hook_js, (int which, double dt), {
    return globalThis.__efxDispatchHook(which, dt);
});

/* ADR 0016: the entry is evaluated from the platform init callback, after
 * WebGL is initialized; the JS boot owns the evaluation body */
EM_JS(void, efx_web_eval_entry_js, (void), {
    if (typeof globalThis.__efxEvaluateEntry === 'function') {
        globalThis.__efxEvaluateEntry();
    }
});

EM_JS(void, efx_web_publish_exit, (int code), {
    Module['efxExitCode'] = code;
    Module['efxRunEnded'] = true;
});

static char *dup_string(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) {
        memcpy(p, s, n);
    }
    return p;
}

static void set_args(char *const *args, int count) {
    if (count > 0 && args) {
        W.args = calloc((size_t)count, sizeof(char *));
        if (!W.args) {
            return;
        }
        for (int i = 0; i < count; i++) {
            W.args[i] = dup_string(args[i]);
        }
        W.arg_count = count;
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_quit(int code) {
    W.quit_requested = 1;
    W.quit_code = code;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_error(void) {
    W.in_error = 1;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_fail(void) {
    W.in_error = 1;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_exit_code(void) {
    if (W.in_error) {
        return 1;
    }
    if (W.quit_requested) {
        return W.quit_code;
    }
    return 0;
}

/* F6d: the interactive console has no stdin on the web build; the boot JS
 * checks this to report unavailability (and a non-zero exit) instead of
 * silently ignoring `--repl`. */
EMSCRIPTEN_KEEPALIVE int efx_bridge_repl_requested(void) {
    return W.repl_requested;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_arg_count(void) {
    return W.arg_count;
}

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_arg(int i) {
    if (i < 0 || i >= W.arg_count || !W.args) {
        return "";
    }
    return W.args[i];
}
static int web_frame(void *ud, double dt) {
    (void)ud;
    int stop = 0;
    if (W.quit_requested || W.in_error) {
        stop = 1;
    } else if (efx_web_call_hook_js(1, dt) != 0) {
        stop = 1;
    } else if (W.quit_requested || W.in_error) {
        stop = 1;
    } else {
        /* F11: advance engine-owned particle systems after the update hooks
           and before the render hooks (auto-update, ADR 0039) */
        efx_render_particles_step((float)(dt > 0.0 ? dt : 0.0));
    }
    if (!stop && !(W.quit_requested || W.in_error) &&
        efx_web_call_hook_js(0, dt) != 0) {
        stop = 1;
    } else if (!stop && (W.quit_requested || W.in_error)) {
        stop = 1;
    }
    if (stop) {
        efx_web_publish_exit(efx_bridge_exit_code());
    }
    return stop;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_frame(void) {
    /* direct (Node harness) path bypasses the sokol frame loop, so derive dt
       here; the first frame reports 0 (desktop parity) */
    double now = emscripten_get_now();
    double dt = W.frame_have_now ? (now - W.frame_last_now) / 1000.0 : 0.0;
    W.frame_have_now = 1;
    W.frame_last_now = now;
    efx_input_begin_frame();
    int rc = web_frame(NULL, dt);
    efx_input_end_frame();
    return rc;
}

EMSCRIPTEN_KEEPALIVE const char *efx_web_root(void) {
    return W.root;
}

EMSCRIPTEN_KEEPALIVE void efx_web_set_golden_mode(void) {
    W.golden_mode = 1;
}

int efx_web_main(int argc, char *const *argv) {
    int golden = W.golden_mode;
    memset(&W, 0, sizeof(W));
    W.golden_mode = golden;
    if (golden) {
        static char outbuf[160];
        char scene[64] = "clear";
        const char *s = emscripten_run_script_string(
            "(function(){ try { return new URLSearchParams(location.search).get('scene') || 'clear'; } catch (e) { return 'clear'; } })()");
        if (s && !strchr(s, '/') && !strstr(s, "..")) {
            snprintf(scene, sizeof(scene), "%s", s);
        }
        const char *r = emscripten_run_script_string(
            "(function(){ try { return new URLSearchParams(location.search).get('root') || ''; } catch (e) { return ''; } })()");
        if (r && r[0] && !strstr(r, "..")) {
            snprintf(W.root, sizeof(W.root), "%s", r);
        } else {
            snprintf(W.root, sizeof(W.root), "/goldens/%s", scene);
        }
        snprintf(outbuf, sizeof(outbuf), "/captures/%s.png", scene);
        W.capture.frame = 2;
        W.capture.output = outbuf;
    } else {
        if (argc >= 2 && strcmp(argv[1], "--repl") == 0) {
            /* no stdin console on the web build (F6d) */
            W.repl_requested = 1;
            W.in_error = 1;
            fprintf(stderr, "player: repl mode is unavailable on the web build\n");
            fflush(stderr);
        } else if (argc >= 2) {
            if (argv[1][0] == '/') {
                snprintf(W.root, sizeof(W.root), "%s", argv[1]);
            } else {
                char cwd[EFX_WEB_ROOT_MAX];
                if (getcwd(cwd, sizeof(cwd))) {
                    snprintf(W.root, sizeof(W.root), "%s/%s", cwd, argv[1]);
                } else {
                    snprintf(W.root, sizeof(W.root), "%s", argv[1]);
                }
            }
        } else {
            snprintf(W.root, sizeof(W.root), "%s", "/examples/browser");
        }
        if (argc > 2) {
            set_args(argv + 2, argc - 2);
        }
    }
    W.dom = efx_web_has_dom_js();
    /* F14: with a DOM (real browser) initialize audio before the script runs,
       so load-time music uses the real device rate; under the Node harness
       (no DOM) the core lazy-inits device-free, matching the desktop --script
       runtime for the cross-runtime comparison */
    if (W.dom) {
        efx_audio_backend_init();
    }
    web_open_root();
    return 0;
}

/* ADR 0016: run the entry from the init callback, once WebGL and the engine
 * pipelines exist; a failure or quit stops the run */
static int web_on_init(void *ud) {
    (void)ud;
    efx_web_eval_entry_js();
    return (W.in_error || W.quit_requested) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE void efx_web_start_loop(void) {
    if (!W.dom) {
        return;
    }
    efx_platform_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.capture = W.capture;
    efx_frame_hooks hooks;
    hooks.ud = NULL;
    hooks.on_init = web_on_init;
    hooks.on_frame = web_frame;
    efx_platform_run(&desc, hooks);
}

