# Design

## Context

See `proposal.md` — Why. The current boot order and the surface-less run modes
are the constraints that shape this design.

Today every run mode evaluates the entry script before the rendering surface
exists:

- Desktop resource-root/capture (`src/player/player.c`): `efx_runtime_new` →
  read `main.js` → `efx_player_run_entry` (top-level) → `pick_hooks` →
  `efx_platform_run` → `sapp_run` → `efx_init_cb` (`sg_setup`,
  `efx_pipeline_install`, gamepad, audio) → frame loop.
- `--repl <root>` (`src/player/repl.c`): the same pattern for the optional root
  entry, then the console loop.
- Web DOM (`src/web/js/boot.js`): `__efxEnsureApi` → mount assets →
  `__efxEvaluateEntry` → `_efx_web_start_loop` → `efx_platform_run` → `sapp_run`
  → WebGL init → first frame.
- `--script` and the web Node harness: no surface at all (ADR 0007 "no sokol
  initialization at all"; `efx_web_start_loop` returns early when there is no
  DOM).

`sokol_app` exposes only `sapp_run(desc)` (no split init/frame), so the earliest
a GPU context can exist is the `init_cb`. On Emscripten `sapp_run` returns
immediately and `init_cb` fires on the first animation frame, still before any
user frame (`_sapp_frame` calls init then frame). ADR 0016 explicitly deferred
moving surface creation ahead of evaluation.

The render module currently accepts resource creation with no sink and queues
the upload (`flush_pending_uploads`, texture `pending`, mesh/target deferral).
That queue is what lets `--script` and web-Node create textures/meshes/fonts.
It stays in this stage; removing it is `collapse-pre-gpu-queue`.

## Goals / Non-Goals

**Goals:**
- Evaluate the entry script at the earliest point the surface exists, so
  top-level code is genuine initialization on desktop resource-root/capture,
  `--repl <root>`, and DOM web.
- Keep the surface-less modes (`--script`, web Node) behaviorally unchanged.
- Keep the existing exit-code/error contract intact for top-level failures.

**Non-Goals:**
- Removing the pre-GPU upload queue or changing surface-less resource behavior
  (stage 2).
- Any script-facing API, namespace, or error-text change.
- Changing when `main.js` is *read* (missing-entry failure stays early).

## Decisions

### D1 — Evaluate from a new platform init callback, not the first frame
Add `int (*on_init)(void *ud)` to `efx_frame_hooks`, invoked at the end of
`efx_init_cb` after `sg_setup`, `efx_pipeline_install`, the gamepad backend, the
audio backend, and `efx_input_set_window`. `init_cb` runs before the first
`begin_frame` on both native and Emscripten, so evaluation precedes any frame
and render-state defaults (`ensure_state`) are applied during evaluation exactly
as before.
*Alternatives:* evaluating lazily at the top of the first `on_frame` — rejected
because the platform `frame_cb` already called `efx_render_begin_frame`, so
top-level clear color/camera would apply one frame late; a global init-callback
setter instead of a hooks field — rejected as less consistent with the existing
`efx_frame_hooks` shape.

### D2 — Only evaluation moves; runtime and resource setup stay before `sapp_run`
`run_root_mode`/`efx_repl_run` keep creating the runtime, opening the resource,
and reading `main.js` before `efx_platform_run`. The `on_init` callback receives
the runtime and evaluates the already-read source, then picks hooks. This
preserves the early "missing `main.js`" failure (no window opens for a bad
root) and the existing error-before-window path for setup failures.
*Alternative:* create the runtime inside `init_cb` — rejected: loses early
failure and pushes more state into the callback.

### D3 — Invert the web boot; C calls back into JS after WebGL init
`__efxBoot` still ensures the API and mounts the host asset root. On DOM it then
calls `_efx_web_start_loop`; the C `init_cb` calls an `EM_JS` trampoline that
invokes `__efxEvaluateEntry`. `__efxEvaluateEntry` is split so it no longer
starts the loop. The Node branch keeps evaluating directly and running the
existing frame-guard loop.
*Alternatives:* initialize WebGL before `sapp_run` — rejected: sokol_app owns
canvas/context creation and offers no split init; keeping JS-driven evaluation
and adding an explicit `sg_setup` elsewhere — rejected as duplicating platform
setup.

### D4 — Surface-less modes unchanged; queue retained
`--script` and the web Node harness keep their current code path and the
deferred queue. They have no rendering surface by design (ADR 0007); this stage
does not change them. Their surface-less contract is pinned in the delta spec.

### D5 — Set `efx.window.size` before evaluation
`efx_init_cb` calls `efx_input_set_window(sapp_width(), sapp_height(),
sapp_dpi_scale())` before `on_init`, so window queries are valid at load time
(today they read 0 until the first frame).

### D6 — Top-level failure/quit in `on_init` uses the existing contract
A failed evaluation or a top-level `efx.quit(n)` records the exit code and
stops the loop via the existing player/platform stop path. `sapp_quit()` may be
called from `init_cb` (it sets `quit_ordered`; native and Emscripten loops honor
it). macOS needs the same non-returning handling its `frame_cb` already uses.

## Risks / Trade-offs

- [A top-level error or `efx.quit` now opens a window before exiting in
  surface-bearing modes] → Acceptable and documented as a minor breaking change;
  `--script` (the CI harness) is unaffected.
- [macOS's run loop never returns; its `_exit` handling lives in `frame_cb`] →
  Apply the same non-returning exit path when `on_init` fails.
- [Emscripten `sapp_run` returns immediately and `init_cb` runs on the first
  animation frame; the C→JS eval callback is a re-entrant call] → Keep
  `__efxEnsureApi`/asset mount before `_efx_web_start_loop`, and make the
  callback idempotent/guarded by `st.started` and `st.api`.
- [Web asset fetch is async and must complete before evaluation] → Keep the
  `__efxResolveAssets` ordering; `_efx_web_start_loop` is called only after the
  mount resolves.
- [Evaluation now happens after `efx_render_install_sink`, shifting the
  "install must not wipe script state" concern] → Defaults are applied lazily on
  the first API call during evaluation; verify with byte-identical goldens.
- [Golden images could shift] → A diff is a regression, never a re-baseline.
- [Unmanaged resources] → This change introduces no new resource type; existing
  GC-finalized opaque classes with `destroy()` (ADR 0011/0012) are unchanged.
