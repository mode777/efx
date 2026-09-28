#include "player/repl.h"
#include "runtime/runtime.h"
#include "platform/platform.h"
#include "render/render.h"
#include "resource/resource.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

#define EFX_REPL_LINE_MAX 4096

typedef struct {
    efx_runtime *rt;
    char line[EFX_REPL_LINE_MAX];
    size_t len;
    int interactive; /* stdin is a TTY: show a prompt and banner */
    int prompted;
    int eof;
    int done; /* .exit requested */
} efx_repl;

/* ------------------------------------------------------------------ input */

static int repl_is_tty(void) {
#ifdef _WIN32
    return _isatty(_fileno(stdin));
#else
    return isatty(fileno(stdin));
#endif
}

/* Non-blocking "is there input?" check (design D1). Pipes and redirected
 * files are reported readable so the piped CI path works on every target. */
static int repl_readable(void) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD type = GetFileType(h);
    if (type == FILE_TYPE_CHAR) {
        return _kbhit();
    }
    if (type == FILE_TYPE_PIPE) {
        DWORD avail = 0;
        if (PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL)) {
            return avail > 0;
        }
        return 1; /* let the read decide */
    }
    return 1; /* disk file: read returns data or 0 at EOF */
#else
    struct pollfd pfd;
    pfd.fd = 0;
    pfd.events = POLLIN;
    pfd.revents = 0;
    /* any event (readable, hangup, error) means a read will make progress;
       a hangup read returns 0 and is reported as EOF */
    return poll(&pfd, 1, 0) > 0;
#endif
}

static int repl_read(char *buf, int n) {
#ifdef _WIN32
    return (int)_read(_fileno(stdin), buf, (unsigned)n);
#else
    return (int)read(0, buf, (size_t)n);
#endif
}

/* --------------------------------------------------------------- commands */

static void repl_print_help(void) {
    fputs("REPL commands:\n"
          "  .help   show this help\n"
          "  .exit   leave the console\n",
          stdout);
    fflush(stdout);
}

/* returns 1 when `line` was a host command handled here */
static int repl_host_command(efx_repl *r, const char *line) {
    if (line[0] != '.') {
        return 0;
    }
    if (strcmp(line, ".help") == 0) {
        repl_print_help();
    } else if (strcmp(line, ".exit") == 0) {
        r->done = 1;
    } else {
        fprintf(stderr, "player: unknown repl command: %s\n", line);
        fflush(stderr);
    }
    return 1;
}

static void repl_handle_line(efx_repl *r, const char *line) {
    if (repl_host_command(r, line)) {
        return;
    }
    if (line[0] == '\0') {
        return;
    }
    efx_runtime_eval_repl_line(r->rt, line);
}

static void repl_consume(efx_repl *r) {
    char buf[256];
    while (!r->eof && !r->done && !efx_runtime_quit_requested(r->rt) &&
           !efx_runtime_in_error(r->rt)) {
        if (!repl_readable()) {
            return;
        }
        int n = repl_read(buf, (int)sizeof(buf));
        if (n <= 0) {
            r->eof = 1;
            break;
        }
        for (int i = 0; i < n; i++) {
            char ch = buf[i];
            if (ch == '\n') {
                r->line[r->len] = '\0';
                repl_handle_line(r, r->line);
                r->len = 0;
                r->prompted = 0;
                if (r->done || efx_runtime_quit_requested(r->rt) ||
                    efx_runtime_in_error(r->rt)) {
                    return;
                }
            } else if (ch != '\r') {
                if (r->len + 1 < EFX_REPL_LINE_MAX) {
                    r->line[r->len++] = ch;
                }
                /* an overlong line drops the excess up to the newline */
            }
        }
    }
    /* a trailing line without a final newline still counts (piped input) */
    if (r->eof && !r->done && r->len > 0) {
        r->line[r->len] = '\0';
        repl_handle_line(r, r->line);
        r->len = 0;
    }
}

/* ------------------------------------------------------------ frame loop */

static int repl_exit_code(const efx_repl *r) {
    if (efx_runtime_in_error(r->rt)) {
        return 1;
    }
    if (efx_runtime_quit_requested(r->rt)) {
        return efx_runtime_quit_code(r->rt);
    }
    return 0; /* `.exit` and EOF are clean */
}

/* record the intended code, then stop the frame loop (macOS's Cocoa loop
 * never returns, so the platform layer exits with the recorded code) */
static int repl_stop(efx_repl *r) {
    efx_platform_set_exit_code(repl_exit_code(r));
    return 1;
}

static int repl_on_frame(void *ud, double dt) {
    efx_repl *r = (efx_repl *)ud;
    efx_runtime *rt = r->rt;
    if (efx_runtime_quit_requested(rt) || efx_runtime_in_error(rt)) {
        return repl_stop(r);
    }

    if (!r->done && !r->eof) {
        if (r->interactive && !r->prompted) {
            fputs("> ", stdout);
            fflush(stdout);
            r->prompted = 1;
        }
    } else if (r->prompted) {
        fputc('\n', stdout);
        fflush(stdout);
        r->prompted = 0;
    }

    repl_consume(r);
    if (r->done || r->eof || efx_runtime_quit_requested(rt) ||
        efx_runtime_in_error(rt)) {
        return repl_stop(r);
    }

    /* F9: input callbacks run before the update hooks, in arrival order */
    int irc = efx_runtime_dispatch_input(rt);
    if (irc != EFX_HOOK_OK) {
        return repl_stop(r);
    }
    if (efx_runtime_quit_requested(rt) || efx_runtime_in_error(rt)) {
        return repl_stop(r);
    }

    int rc = efx_runtime_call_hook(rt, 1, dt);
    if (rc != EFX_HOOK_OK) {
        return repl_stop(r);
    }
    if (efx_runtime_quit_requested(rt) || efx_runtime_in_error(rt)) {
        return repl_stop(r);
    }
    rc = efx_runtime_call_hook(rt, 0, dt);
    if (rc != EFX_HOOK_OK) {
        return repl_stop(r);
    }
    efx_runtime_collect(rt);
    if (efx_runtime_quit_requested(rt) || efx_runtime_in_error(rt)) {
        return repl_stop(r);
    }
    return 0;
}

int efx_repl_run(struct efx_resource *resource) {
    efx_runtime *rt = efx_runtime_new(NULL, 0);
    if (!rt) {
        return 1;
    }
    if (resource) {
        efx_runtime_set_resource(rt, resource);
        /* run the root's entry script once, if present; a bare root (or no
           root) starts with the namespace only (design D4) */
        int eerr = EFX_RESOURCE_OK;
        char *code = efx_resource_read_text(resource, "main.js", &eerr);
        if (code) {
            int rc = efx_runtime_run_entry(rt, "main.js", code);
            efx_resource_free(code);
            if (rc == -1 || efx_runtime_in_error(rt)) {
                efx_runtime_destroy(rt);
                return 1;
            }
            if (efx_runtime_quit_requested(rt)) {
                int exit_code = efx_runtime_quit_code(rt);
                efx_runtime_destroy(rt);
                return exit_code;
            }
        }
    }

    int has_update = 0;
    int has_render = 0;
    efx_runtime_pick_hooks(rt, &has_update, &has_render);

    efx_repl r;
    memset(&r, 0, sizeof(r));
    r.rt = rt;
    r.interactive = repl_is_tty();
    if (r.interactive) {
        fprintf(stderr, "EmotionFX REPL - .help for commands, .exit to quit\n");
    }

    efx_platform_desc desc;
    memset(&desc, 0, sizeof(desc));
    efx_frame_hooks hooks;
    hooks.ud = &r;
    hooks.on_frame = repl_on_frame;
    efx_platform_run(&desc, hooks);

    int exit_code;
    if (efx_runtime_in_error(rt)) {
        exit_code = 1;
    } else if (efx_runtime_quit_requested(rt)) {
        exit_code = efx_runtime_quit_code(rt);
    } else {
        exit_code = 0;
    }
    efx_runtime_destroy(rt);
    efx_render_end_frame();
    efx_render_shutdown();
    efx_platform_shutdown();
    return exit_code;
}
