#ifndef EFX_REPL_H
#define EFX_REPL_H

/* F6d interactive console (desktop/quickjs only): opens the normal window
 * and frame loop while evaluating JavaScript read from stdin a line at a
 * time in the persistent script context. `resource` is the optional F6a
 * root (borrowed; owned by the caller) — NULL starts the bare namespace.
 * Returns the process exit code. */
struct efx_resource;
int efx_repl_run(struct efx_resource *resource);

#endif
