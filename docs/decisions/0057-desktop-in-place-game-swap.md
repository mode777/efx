# 0057 — A dropped root swaps the desktop game in place

Status: Accepted (2026-10, change `in-place-game-swap`)

Supports: ADR 0056 (a dropped root is host-level; this replaces its desktop
restart mechanism); ADR 0011/0012 (resource lifecycle and memory discipline);
ADR 0016 (loading `main.js` is the implicit init); ADR 0007 (run modes; the
macOS loop never returns); ADR 0031 (the dir/zip provider); ADR 0020 (the
four-target gate). Supersedes ADR 0056's desktop-relaunch clause only; the
host-level drop contract is unchanged.

## Context

`drop-to-load-game` (ADR 0056) loaded a dropped root on desktop by spawning a
detached copy of the player and exiting the current run. That tears down and
reopens the window — a visible flash — and discards window state. The engine,
however, builds its runtime, GPU resources, input, physics, particles, and
audio once and tears them down only at exit, so a seamless swap needs a
lifecycle the engine did not have. On web there is nothing to preserve: the
gallery already runs one engine per run in a fresh iframe, and the standalone
player reloads.

## Decision

- **A stable player session owns the active game.** `src/player/player.c` holds
  one session (`efx_runtime`, `efx_resource`, current root, pending root) and
  the platform hooks point at it (`hooks.ud`), so replacing the runtime on a
  swap never leaves a dangling frame callback.
- **A desktop drop requests a swap; the swap runs at frame start.** The drop
  handler validates the root (reusing `efx_resource_probe_root` + the archive
  cap from ADR 0056) and records it as the session's pending root. At the top of
  the frame callback the player performs the swap between frames, so no GPU
  resource is in flight. A drop arriving mid-swap is coalesced.
- **The swap releases all game state in a fixed order:** destroy the old
  runtime (JS finalizers release GPU resources, then the physics world);
  `efx_render_end_frame()` + `efx_render_reset()` (release every registered
  texture/mesh/render target/particle and re-apply documented defaults, keeping
  the installed sink and the Sokol `sg` context/pipelines); `efx_pipeline_rebind()`
  (re-install the sink and recreate the engine white texture view);
  `efx_input_reset()`; `efx_audio_stop_all()`; close the old root / open the
  new one; build a fresh runtime and run its `main.js` entry. The new root is
  preflighted before any teardown, so an unusable root leaves the running game
  untouched.
- **The swap is host-level.** No script-facing swap API, loading hook, or async
  `load*`; the `efx` namespace is unchanged.
- **Web is unchanged.** The gallery keeps its fresh-iframe isolation and the
  web player keeps ADR 0056's reload.

## Consequences

- A dropped desktop game swaps in the same window and `sg` context; the
  process is not restarted and the window is not recreated.
- This supersedes the desktop relaunch clause of ADR 0056 (the drop contract
  itself — host-level, native zip/folder, web zip, validation, size cap — still
  stands).
- `efx_render_reset` and `efx_pipeline_rebind` split teardown from sink
  ownership: the render shutdown path still clears the sink and tells it to
  shut down, while the reset path keeps both. Future in-place resets must call
  the pair together.
- Repeating swaps must not leak GPU resources: the regression guard is a native
  smoke that swaps through several roots, each creating and drawing a
  script-owned texture.
- The swap runs inside the frame callback, so macOS's never-returning Cocoa
  loop is untouched.

## Rejected alternatives

- **Keep relaunching (ADR 0056):** the flash and window recreation are exactly
  what this change removes.
- **Perform the swap inside the event callback:** events can arrive between
  render recording and commit, so GPU resources may be in use; deferring to
  frame start is safe on every backend.
- **Call `efx_render_shutdown()` then `efx_pipeline_install()` mid-run:**
  `efx_pipeline_install` early-returns once installed, so the sink would stay
  cleared and the cached white view would dangle, producing missing geometry
  after a swap.
- **Tear down render before the runtime:** JS finalizers would then write
  releases into freed registries.
- **Swap on web / reuse one gallery iframe across samples:** conflicts with
  `web-gallery`'s per-run isolation and risks WebGL-context exhaustion for no
  visible gain.
