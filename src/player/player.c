#include "player/player.h"
#include "player/repl.h"
#include "runtime/runtime.h"
#include "platform/platform.h"
#include "platform/audio_backend.h"
#include "render/render.h"
#include "resource/resource.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#ifdef _WIN32
#define EFX_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define EFX_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#else
#define EFX_ISDIR(m) S_ISDIR(m)
#define EFX_ISREG(m) S_ISREG(m)
#endif

/* dropped archives larger than this are rejected before mounting (design
 * D-risk: miniz decompresses whole entries into memory). Directories are not
 * size-capped here. 256 MiB is far above the shipped samples. */
#define EFX_DROP_MAX_ARCHIVE_BYTES (256u * 1024u * 1024u)

static int usage(void) {
    fprintf(stderr,
            "usage:\n"
            "  player <resource-root>            run a resource folder or zip\n"
            "  player --script <file> [--root <dir|zip>] [args...]\n"
            "                                    run a single script headless\n"
            "  player --repl [<root>]            interactive console (desktop)\n"
            "  player --capture-frame <N> --capture-output <file> <resource-root>\n"
            "                                    render N frames, write PNG, exit (golden tests)\n");
    return 1;
}

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && EFX_ISDIR(st.st_mode);
}

static int is_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && EFX_ISREG(st.st_mode);
}

/* Directory containing a file path (malloc'd). Separators: '/' and '\\'.
 * Falls back to "." for a bare filename, and NULL on allocation failure. */
static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *out = malloc(n);
    if (out) {
        memcpy(out, s, n);
    }
    return out;
}

static char *path_dir(const char *path) {
    const char *slash = NULL;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            slash = p;
        }
    }
    if (!slash) {
        return dupstr(".");
    }
    if (slash == path) {
        return dupstr("/");
    }
    size_t n = (size_t)(slash - path);
    char *out = malloc(n + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, path, n);
    out[n] = '\0';
    return out;
}

/* --------------------------------------------------- dropped-root relaunch */

/* set once a drop has been accepted; the frame loop stops and the spawned
 * child takes over (the current process exits 0) */
static int g_drop_relaunch;

#if defined(_WIN32)
static int spawn_player(const char *root) {
    wchar_t exe[4096];
    DWORD n = GetModuleFileNameW(NULL, exe, (DWORD)(sizeof(exe) / sizeof(exe[0])));
    if (n == 0 || n >= sizeof(exe) / sizeof(exe[0])) {
        return 0;
    }
    int rlen = MultiByteToWideChar(CP_UTF8, 0, root, -1, NULL, 0);
    if (rlen <= 0) {
        return 0;
    }
    wchar_t *root_w = malloc((size_t)rlen * sizeof(wchar_t));
    if (!root_w) {
        return 0;
    }
    MultiByteToWideChar(CP_UTF8, 0, root, -1, root_w, rlen);
    const wchar_t *argv[3] = {exe, root_w, NULL};
    intptr_t rc = _wspawnv(_P_NOWAIT, exe, argv);
    free(root_w);
    return rc != -1;
}
#else
static int spawn_player(const char *root) {
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(NULL, &size);
    char *exe = malloc(size + 1);
    if (!exe) {
        return 0;
    }
    if (_NSGetExecutablePath(exe, &size) != 0) {
        free(exe);
        return 0;
    }
#else
    char *exe = dupstr("/proc/self/exe");
    if (!exe) {
        return 0;
    }
#endif
    pid_t pid = fork();
    if (pid < 0) {
        free(exe);
        return 0;
    }
    if (pid == 0) {
        setsid(); /* detach from the terminal/session */
        char *argv[3];
        argv[0] = exe;
        argv[1] = (char *)root;
        argv[2] = NULL;
        execv(exe, argv);
        _exit(127); /* exec failed */
    }
    free(exe);
    return 1;
}
#endif

/* Native drag-and-drop (ADR 0056): validate the dropped root, then relaunch
 * the player on it and stop this run. An unusable drop prints a diagnostic and
 * leaves the running game untouched. */
static void player_on_files_dropped(void *ud, const char *path) {
    (void)ud;
    if (g_drop_relaunch) {
        return; /* coalesce drops while a relaunch is pending */
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "player: dropped path is unreadable: %s\n", path);
        return;
    }
    if (EFX_ISREG(st.st_mode) &&
        (unsigned long long)st.st_size > EFX_DROP_MAX_ARCHIVE_BYTES) {
        fprintf(stderr,
                "player: dropped archive exceeds %u MiB: %s\n",
                (unsigned)(EFX_DROP_MAX_ARCHIVE_BYTES / (1024u * 1024u)), path);
        return;
    }
    if (!efx_resource_probe_root(path)) {
        fprintf(stderr, "player: dropped root has no main.js: %s\n", path);
        return;
    }
    if (!spawn_player(path)) {
        fprintf(stderr, "player: could not relaunch on dropped root: %s\n",
                path);
        return;
    }
    g_drop_relaunch = 1;
}

static int run_script_mode(const char *path, const char *root_override,
                           char *const *args, int arg_count) {
    efx_runtime *rt = efx_runtime_new(args, arg_count);
    if (!rt) {
        return 1;
    }
    char *dir = root_override ? dupstr(root_override) : path_dir(path);
    efx_resource *res = NULL;
    if (dir) {
        int e = EFX_RESOURCE_OK;
        res = efx_resource_open(dir, &e);
        free(dir);
    }
    if (res) {
        efx_runtime_set_resource(rt, res);
    }
    int rc = efx_runtime_eval_file(rt, path);
    int exit_code = rc == -1 ? 1 : efx_player_exit_code(rt);
    efx_runtime_destroy(rt);
    efx_resource_close(res);
    return exit_code;
}

/* F6d interactive console: validate/open the optional root, then hand off to
 * the windowed REPL loop. The root is borrowed by the runtime and released
 * only after the platform teardown. */
static int run_repl_mode(const char *root) {
    efx_resource *res = NULL;
    if (root) {
        if (!is_dir(root) && !is_file(root)) {
            fprintf(stderr, "player: resource root is not a directory: %s\n", root);
            return 1;
        }
        int e = EFX_RESOURCE_OK;
        res = efx_resource_open(root, &e);
        if (!res) {
            fprintf(stderr, "player: cannot open resource root: %s\n", root);
            return 1;
        }
    }
    int rc = efx_repl_run(res);
    efx_resource_close(res);
    return rc;
}

int efx_player_exit_code(efx_runtime *rt) {
    if (efx_runtime_in_error(rt)) {
        return 1;
    }
    return efx_runtime_quit_requested(rt) ? efx_runtime_quit_code(rt) : 0;
}

int efx_player_run_entry(efx_runtime *rt, char *code, int *exit_code) {
    int rc = efx_runtime_run_entry(rt, "main.js", code);
    efx_resource_free(code);
    if (rc == -1 || efx_runtime_in_error(rt)) {
        *exit_code = 1;
        return 1;
    }
    if (efx_runtime_quit_requested(rt)) {
        *exit_code = efx_runtime_quit_code(rt);
        return 1;
    }
    return 0;
}

/* signal the frame loop to stop, recording the intended exit code first
 * (macOS's Cocoa loop never returns, so the platform layer exits for us) */
static int player_stop(efx_runtime *rt) {
    efx_platform_set_exit_code(efx_player_exit_code(rt));
    return 1;
}

int efx_player_frame(void *ud, double dt) {
    efx_runtime *rt = (efx_runtime *)ud;
    if (g_drop_relaunch) {
        return player_stop(rt);
    }
    if (efx_runtime_quit_requested(rt) || efx_runtime_in_error(rt)) {
        return player_stop(rt);
    }
    /* F9: input callbacks run before the update hooks, in arrival order */
    int r = efx_runtime_dispatch_input(rt);
    if (r != EFX_HOOK_OK) {
        return player_stop(rt);
    }
    if (efx_runtime_quit_requested(rt) || efx_runtime_in_error(rt)) {
        return player_stop(rt);
    }
    r = efx_runtime_call_hook(rt, 1, dt);
    if (r != EFX_HOOK_OK) {
        return player_stop(rt);
    }

    /* F11: advance engine-owned particle systems after the update hooks and
       before the render hooks (auto-update, ADR 0039) */
    efx_render_particles_step((float)(dt > 0.0 ? dt : 0.0));

    if (efx_runtime_quit_requested(rt) || efx_runtime_in_error(rt)) {
        return player_stop(rt);
    }
    r = efx_runtime_call_hook(rt, 0, dt);
    if (r != EFX_HOOK_OK) {
        return player_stop(rt);
    }

    /* frame-end collection: unreferenced native resources are finalized
       within roughly a frame (js-api resource lifecycle rules) */
    efx_runtime_collect(rt); /* frame-end GC (js-api lifecycle rules) */
    if (efx_runtime_quit_requested(rt) || efx_runtime_in_error(rt)) {
        return player_stop(rt);
    }
    return 0;
}

static int on_frame(void *ud, double dt) {
    return efx_player_frame(ud, dt);
}

/* Entry evaluation runs from the platform init callback, after the window and
 * GPU context exist (ADR 0016). The source is read before efx_platform_run so
 * a missing main.js still fails without opening a window; it is handed over
 * here and freed by efx_player_run_entry. */
static char *g_root_entry;

static int on_init_root(void *ud) {
    efx_runtime *rt = (efx_runtime *)ud;
    if (g_root_entry) {
        char *code = g_root_entry;
        g_root_entry = NULL;
        int exit_code = 0;
        if (efx_player_run_entry(rt, code, &exit_code)) {
            efx_platform_set_exit_code(exit_code);
            return 1;
        }
    }
    int has_update = 0;
    int has_render = 0;
    efx_runtime_pick_hooks(rt, &has_update, &has_render);
    return 0;
}

static int run_root_mode(const char *root, const efx_platform_capture *capture) {
    if (!is_dir(root) && !is_file(root)) {
        fprintf(stderr, "player: resource root is not a directory: %s\n", root);
        return 1;
    }
    int rerr = EFX_RESOURCE_OK;
    efx_resource *res = efx_resource_open(root, &rerr);
    if (!res) {
        fprintf(stderr, "player: cannot open resource root: %s\n", root);
        return 1;
    }
    int eerr = EFX_RESOURCE_OK;
    char *code = efx_resource_read_text(res, "main.js", &eerr);
    if (!code) {
        fprintf(stderr, "player: no main.js in resource root: %s\n", root);
        efx_resource_close(res);
        return 1;
    }
    efx_runtime *rt = efx_runtime_new(NULL, 0);
    if (!rt) {
        efx_resource_free(code);
        efx_resource_close(res);
        return 1;
    }
    efx_runtime_set_resource(rt, res);
    /* F14: set up the audio device before the script runs, so load-time audio
       calls (e.g. background music at boot) use the real device sample rate */
    efx_audio_backend_init();
    /* evaluation is deferred to on_init_root, once the surface exists */
    g_root_entry = code;
    efx_platform_desc desc;
    memset(&desc, 0, sizeof(desc));
    if (capture) {
        desc.capture = *capture;
    }
    efx_frame_hooks hooks;
    hooks.ud = rt;
    hooks.on_init = on_init_root;
    hooks.on_frame = on_frame;
    hooks.on_files_dropped = player_on_files_dropped;
    efx_platform_run(&desc, hooks);
    int exit_code = efx_player_exit_code(rt);
    /* release native resources while the GPU context is still alive:
       runtime destroy runs finalizers -> deferred texture releases, then
       render shutdown flushes them, then sokol goes down */
    efx_runtime_destroy(rt);
    efx_render_end_frame();
    efx_render_shutdown();
    efx_platform_shutdown();
    efx_resource_close(res);
    return exit_code;
}

int efx_player_main(int argc, char **argv) {
    if (argc < 2) {
        return usage();
    }
    if (strcmp(argv[1], "--script") == 0 || strcmp(argv[1], "-s") == 0) {
        if (argc < 3) {
            fprintf(stderr, "player: --script requires a file argument\n");
            return usage();
        }
        /* script args, with an optional `--root <directory|archive>` override
           stripped out (the script's own directory is the default root) */
        const char *root_override = NULL;
        char **sargs = calloc((size_t)argc, sizeof(char *));
        if (!sargs) {
            return 1;
        }
        int sn = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--root") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "player: --root requires a path\n");
                    free(sargs);
                    return usage();
                }
                root_override = argv[i + 1];
                i++;
            } else {
                sargs[sn++] = argv[i];
            }
        }
        int rc = run_script_mode(argv[2], root_override, sargs, sn);
        free(sargs);
        return rc;
    }
    if (strcmp(argv[1], "--repl") == 0) {
        const char *root = (argc >= 3) ? argv[2] : NULL;
        return run_repl_mode(root);
    }
    /* capture flags must precede the resource root */
    efx_platform_capture capture;
    memset(&capture, 0, sizeof(capture));
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "--capture-frame") == 0) {
            if (argi + 1 >= argc) {
                fprintf(stderr, "player: --capture-frame requires a number\n");
                return usage();
            }
            capture.frame = atoi(argv[argi + 1]);
            argi += 2;
        } else if (strcmp(argv[argi], "--capture-output") == 0) {
            if (argi + 1 >= argc) {
                fprintf(stderr, "player: --capture-output requires a path\n");
                return usage();
            }
            capture.output = argv[argi + 1];
            argi += 2;
        } else {
            fprintf(stderr, "player: unknown option: %s\n", argv[argi]);
            return usage();
        }
    }
    if ((capture.frame > 0) != (capture.output != NULL)) {
        fprintf(stderr, "player: --capture-frame and --capture-output go together\n");
        return usage();
    }
    if (argi >= argc) {
        return usage();
    }
    return run_root_mode(argv[argi], capture.frame > 0 ? &capture : NULL);
}
