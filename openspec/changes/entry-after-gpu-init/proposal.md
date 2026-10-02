# Proposal

## Why

Every run mode currently evaluates the entry script's top-level code **before**
the window and GPU context exist. ADR 0016 recorded this explicitly and
deferred the fix: "moving window/GL-context creation ahead of evaluation is
**not** required for this contract and stays deferred… can be revisited if a
future feature needs load-time GPU access." That future is here: top-level
access to engine-owned GPU resources (`efx.graphics.whiteTexture`) fails, and
the engine must carry a deferred pre-GPU upload queue to paper over the gap.
The entry script should run as late as possible — once the surface, pipelines,
input window, audio, and gamepad backends are all initialized — so that
top-level code is genuine initialization.

This is stage 1 of two. It moves evaluation; it does not yet remove the
deferred queue (stage 2, `collapse-pre-gpu-queue`).

## What Changes

- **Add a platform init callback.** `efx_frame_hooks` gains `on_init(ud)`,
  invoked at the end of `efx_init_cb` after `sg_setup`, `efx_pipeline_install`,
  the gamepad backend, the audio backend, and `efx_input_set_window` — the
  earliest point a rendering surface exists.
- **Desktop resource-root and capture modes:** keep the early "no `main.js`"
  failure (read the entry before opening the window), create the runtime and
  resource before `efx_platform_run`, and evaluate the entry plus pick hooks
  inside `on_init`. A failed evaluation records the exit code and stops the
  loop through the existing exit contract.
- **`--repl <root>`:** the optional root entry is evaluated in `on_init`; the
  console loop is unchanged.
- **Web (DOM):** invert the boot. `__efxBoot` still ensures the API and mounts
  the host asset root, but starts the sokol loop first; the C init callback
  calls back into JS to evaluate the entry. `__efxEvaluateEntry` no longer
  starts the loop itself.
- **Set `efx.window.size` before evaluation** so it is valid at load time.
- **Unchanged in this stage:** `--script` and the web Node harness have no
  rendering surface by design and keep their current behavior; the deferred
  upload queue still exists but is no longer reachable from any GPU run mode.
- **BREAKING (minor, observable):** in resource-root/capture/REPL runs, a
  top-level exception or `efx.quit(n)` now happens after the window opens
  instead of before it. `--script` behavior is unchanged.

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities
- `player-runtime`: the entry script's top-level code SHALL run after the
  rendering surface is initialized (resource-root, capture, REPL-with-root on
  desktop; DOM web), while `--script` and the web Node harness remain
  surface-less. Pins the new ordering and the `efx.window.size`-at-load
  guarantee.

## Impact

- **Code:** `src/platform/platform.h`/`platform.c` (init callback + input
  window), `src/player/player.c`, `src/player/repl.c`, `src/web/js/boot.js`,
  `src/web/bridge_core.c`, `src/runtime/*` (evaluation entry lifetime).
- **Behavior:** top-level GPU access works in GPU run modes; `--script` and
  web-Node unchanged. Golden images MUST stay byte-identical.
- **Docs/ADRs:** amend **ADR 0016** (adopt the reorder it deferred) rather than
  add a new ADR; update the `player-runtime` spec and the `AGENTS.md`
  current-state map if the run-mode description changes.
- **Milestone:** no roadmap milestone — this is a post-roadmap architecture
  change (F1–F14 are complete). It implements the deferred decision in
  ADR 0016.

## Non-goals

- Removing the deferred pre-GPU upload queue or changing `--script`/web-Node
  resource behavior (stage 2, `collapse-pre-gpu-queue`).
- Any script-facing API change, new namespace member, or changed error text.
- Re-baselining golden images; a pixel diff is a regression.
- Giving `--script` or the web Node harness a rendering surface.
