# Design

## Context

See `proposal.md` — Why. The desktop player's lifecycle is built around a
single game: `run_root_mode` opens the root, reads `main.js`, creates the
runtime, and calls `efx_platform_run` with `hooks.ud` pointing **directly at
the runtime**; the entry runs once from `on_init` after the surface exists, and
teardown (`efx_runtime_destroy` → `efx_render_shutdown` →
`efx_platform_shutdown`) happens only after the loop returns.

Facts that shape the swap:

- `efx_runtime_destroy` frees the QuickJS context, whose finalizers release GPU
  resources into the render registry, and then frees the physics world — so the
  runtime must be destroyed **before** render teardown.
- `efx_render_shutdown` destroys every texture/mesh/RTT/particle, frees the
  arenas, and `memset`s `R`, which clears `R.sink`. Its sink's `shutdown` is
  `NULL`, so the Sokol `sg` context and the canned pipelines survive.
- `efx_pipeline_install` early-returns when `P.installed` is set, so it cannot
  be used to re-install the sink after a reset.
- The engine-owned 1×1 white texture lives in the render registry
  (`R.white_handle`) and its view is cached in `P.white_view`; a render reset
  destroys the texture and leaves that cached view dangling.
- macOS's Cocoa loop never returns (ADR 0007) and exits the process from the
  frame callback, so the swap must run **inside** the frame callback, not on
  the post-loop return path.

Web is out of scope: the gallery already isolates each run in a fresh iframe
(`web-gallery` "Isolated engine hosting per run"), and the standalone web
player keeps `drop-to-load-game`'s reload.

## Goals / Non-Goals

**Goals:**

- Swap the active desktop game while the window, `sg` context, and pipelines
  stay alive.
- Guarantee clean state and no resource accumulation across repeated swaps.
- Keep the swap invisible to scripts (no new API) and reuse the drop validation
  from `drop-to-load-game`.

**Non-Goals:**

- Web in-place swap, or reusing a gallery iframe across samples.
- Preserving any state across a swap.
- A script-facing swap API.

## Decisions

### D1 — A session object owns the active game; hooks point at it

Introduce a player session (`efx_runtime *rt`, `efx_resource *res`, current
root, pending-swap root). `hooks.ud` becomes the session and `efx_player_frame`
reads `session->rt`. Replacing the runtime on swap never dangles `ud`.

*Alternative considered:* keep `ud = rt` and mutate the static `g_hooks` on
swap — rejected; it reaches into platform statics from the player and leaves
the same dangling-pointer hazard for any other `ud` consumer.

### D2 — The drop event records a request; the swap runs at frame start

The drop handler validates the candidate root (reusing `drop-to-load-game`'s
helper) and, on success, records it as the session's pending root. At the top
of `efx_player_frame` (previous frame has ended; `sg_commit` done), if a swap
is pending, the player performs it before dispatching hooks. A second drop
while a swap is in flight is ignored.

*Alternatives considered:* swapping inside `efx_event_cb` — rejected, events
can arrive between render recording and commit, so GPU resources may be in
use; re-entering `efx_platform_run` — rejected, recreates the window and
breaks the macOS never-returns assumption.

### D3 — Add a render reset that preserves the sink, plus a pipeline rebind

Add `efx_render_reset()`: the resource-teardown half of `efx_render_shutdown`
(destroy textures/meshes/RTTs/particles, free arenas, re-apply default state)
but it does **not** clear `R.sink` and does **not** call the sink's `shutdown`.
Add `efx_pipeline_rebind()`: re-install the render sink and re-derive
`P.white_view` from a freshly created engine white texture, without recreating
shaders or pipelines. `efx_render_shutdown` is refactored to share the teardown
and additionally clear the sink.

*Alternative considered:* call `efx_render_shutdown()` then
`efx_pipeline_install()` — rejected: the latter early-returns (`P.installed`)
so the sink would stay cleared and `P.white_view` would dangle, producing
missing 3D geometry after a swap.

### D4 — Fixed teardown/build order

Per swap: (1) `efx_runtime_destroy(old)` — JS finalizers release GPU resources;
(2) `efx_render_reset()`; (3) `efx_pipeline_rebind()`; (4) clear input and stop
all audio sources; (5) `efx_resource_close(old)` then `efx_resource_open(new)`;
(6) `efx_runtime_new`, `efx_runtime_set_resource`, read `main.js`,
`efx_runtime_run_entry`, `efx_runtime_pick_hooks`. The new entry runs from the
frame step because the surface already exists.

*Alternative considered:* render reset before runtime destroy — rejected; JS
finalizers would then write releases into freed registries.

### D5 — Validation before commitment

The candidate root is opened and its `main.js` read before any teardown; on
failure the swap is abandoned with a diagnostic and the current game keeps
running (spec: "Unusable swap leaves the current game running"). This is the
same validation `drop-to-load-game` uses.

### D6 — No new script-visible resources

The swap introduces no dynamic-count or fixed-bank script resources; nothing
new is exposed through GC-finalized classes or pre-allocated banks. The script
sandbox and the `efx` namespace are unchanged.

## Risks / Trade-offs

- **GPU resource leaks across swaps** → the swap destroys the runtime (running
  finalizers) then resets the render registry; a smoke that swaps repeatedly
  and asserts clean state / continued draws guards it.
- **Dangling pipeline views (white texture, absent-map fallbacks)** → the
  rebind step re-derives `P.white_view`; a post-swap 3D draw with an absent map
  guards it.
- **Audio from the previous game still playing** → stop all sources as part of
  the swap (step 4).
- **Physics world lifetime** → freed by `efx_runtime_destroy` after the JS
  context; a new runtime builds a fresh world.
- **A drop arriving mid-swap** → coalesced/ignored (D2).
- **macOS exit path** → the swap stays inside the frame callback, so the
  never-returns behavior is untouched.
- **Longer frame on the swap frame** → acceptable; the swap is a deliberate
  user action.

## Migration Plan

Additive; no data or API migration. Rollback restores the relaunch path from
`drop-to-load-game`. The four-target gate (ADR 0020) still applies.

## Open Questions

- Whether `efx_render_reset`/`efx_pipeline_rebind` should be internal headers
  or render API — an implementation detail with no spec impact (settle in
  tasks).
