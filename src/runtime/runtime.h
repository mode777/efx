#ifndef EFX_RUNTIME_H
#define EFX_RUNTIME_H

#include "quickjs.h"

#define EFX_HOOK_OK 0
#define EFX_HOOK_QUIT 1
#define EFX_HOOK_ERROR 2

typedef struct efx_runtime efx_runtime;

/* F6a resource root (borrowed; owned by the caller, e.g. the player) */
struct efx_resource;

efx_runtime *efx_runtime_new(char *const *args, int arg_count);
void efx_runtime_destroy(efx_runtime *rt);

void efx_runtime_set_resource(efx_runtime *rt, struct efx_resource *resource);

int efx_runtime_eval_file(efx_runtime *rt, const char *path);
int efx_runtime_eval_string(efx_runtime *rt, const char *name, const char *code);
/* F10: evaluate a script as a CommonJS entry module through the shared module
 * runtime. When `source` is NULL the entry at `path` is loaded through the
 * resource provider. The entry module's exported `update`/`render` (or its
 * module-local declarations) are captured for efx_runtime_pick_hooks. Returns
 * 0 on success (including a `efx.quit` request) and 1 on a fatal error. */
int efx_runtime_run_entry(efx_runtime *rt, const char *path, const char *source);
/* F6d: evaluate one REPL line in the persistent global context. Prints the
 * completion value to stdout when it is not `undefined`, prints a thrown
 * exception to stderr, and never sets the fatal error flag (a throw is
 * recovered). Returns 0 on success, 1 when the line threw. An `efx.quit`
 * request is not an error. */
int efx_runtime_eval_repl_line(efx_runtime *rt, const char *line);
void efx_runtime_pick_hooks(efx_runtime *rt, int *has_update, int *has_render);
int efx_runtime_call_hook(efx_runtime *rt, int update_not_render, double dt);
/* F9: drain the frame's staged input events into the registered input
 * callbacks, in arrival order (called before the update hooks). Returns
 * EFX_HOOK_OK / EFX_HOOK_QUIT / EFX_HOOK_ERROR. */
int efx_runtime_dispatch_input(efx_runtime *rt);
/* F13: build the plain gamepad pad-view object for a slot (shared by the
 * desktop binding's `get` and the connect/disconnect dispatch). */
JSValue efx_runtime_gamepad_view(efx_runtime *rt, int slot);

int efx_runtime_quit_requested(const efx_runtime *rt);
int efx_runtime_quit_code(const efx_runtime *rt);
int efx_runtime_in_error(const efx_runtime *rt);
void efx_runtime_collect(efx_runtime *rt); /* frame-end GC (finalizers) */

/* R22 spike-only (throwaway branch): context accessor for the timing harness */
JSContext *efx_runtime_context(efx_runtime *rt);

#endif
