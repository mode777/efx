# Proposal

## Why

EmotionFX can render but cannot be interacted with: the player opens a window
and the platform layer never installs a Sokol event callback, so scripts have no
way to read the keyboard or mouse. `vision.md` now names input as a product
capability and `docs/js-api.md` records it as an open question. This change
adds the input layer — a query API for polling state and an event API with
callbacks — as a new roadmap milestone, **F9 (input)**, an orthogonal milestone
that depends only on F2's window/frame loop and dual script bindings and may
proceed independently of F3–F8.

## What Changes

- Milestone **F9 — Input (keyboard + mouse)** is added to the roadmap as an
  orthogonal milestone (predecessor gate: F2; independent of F3–F8), verified
  by headless unit tests and a simulation harness rather than a golden-image
  gate (input logic has no visual surface).
- New C-owned input core (`src/input/`): a fixed key/button state table plus a
  per-frame event queue, fed by the platform's Sokol event callback and
  consumed by both script bindings. Input state is C-owned and **frame-staged**:
  level state updates on arrival, edges (`pressed`/`released`) are valid for one
  frame, and event callbacks are drained once per frame before update hooks.
- New script namespaces on the single `efx` object (Löve-model):
  - `efx.keyboard` — `isDown(key)`, `isPressed(key)`, `isReleased(key)`;
    `onDown(cb)`, `onUp(cb)`, `onChar(cb)` (text input), each returning an
    unsubscribe function.
  - `efx.mouse` — `isDown(button)`, `isPressed(button)`, `isReleased(button)`;
    `onDown(cb)`, `onUp(cb)`, `onMove(cb)`, `onWheel(cb)`; read-only
    `position`, `x`, `y`, `delta`, `wheel`.
  - `efx.window` — read-only `size`, `width`, `height`, `dpiScale`.
- Event objects are plain JS data (no DOM/host types) with keys/buttons named
  by engine-owned string constants; callbacks and queries share one state
  source, so the two APIs never disagree.
- High-DPI: mouse coordinates and `efx.window.size` are in the renderer's
  surface (framebuffer) pixel space — the same space `drawQuad` and the 2D
  frame use — so hit-testing needs no scaling; `dpiScale` is exposed for
  scripts that want logical units.
- Web focus/default handling is solved at the platform layer: the Sokol event
  callback is the single source on all four targets, and the web build
  suppresses the browser's default action for game keys.
- A deterministic **simulation seam** in the C input core lets the test
  harness inject synthetic key/mouse events; the seam is not exposed to
  scripts.

## Capabilities

### New Capabilities
- `input`: the engine input layer — C-owned state and frame-staged event
  delivery, keyboard/mouse query and event semantics, coordinate space and
  high-DPI policy, focus-loss behavior, and the deterministic simulation seam.

### Modified Capabilities
- `feature-roadmap`: adds F9 (input) as an orthogonal ninth milestone with an
  explicit F2 predecessor gate and a non-visual verification strategy.
- `js-api`: adds the `efx.keyboard` / `efx.mouse` / `efx.window` namespaces and
  their query/event contract; extends the reference document's milestone range
  from F1–F8 to F1–F9 and its vision-traceability catalog with input.

## Impact

- Code: new `src/input/` core (pure C, no Sokol, no quickjs); `src/platform/
  platform.c` installs `sapp_desc.event_cb` and translates `sapp_event` into
  core calls; `src/api/api.c` + `src/runtime/runtime.c` expose the desktop
  binding and drain events before update; `src/web/bridge.c` + `src/web/
  entry.js` expose the web binding with identical semantics; the player and
  REPL frame dispatch gain the pre-update drain step.
- API/docs: `docs/js-api.md` gains an F9 input section (provisional until the
  gate passes); `gallery/src/api/efx.d.ts` gains the namespaced types and the
  key/button string unions; `AGENTS.md` roadmap table and current-state section
  gain F9; **new ADR `docs/decisions/0036`** (input state is C-owned and
  frame-staged; scripts get query functions plus unsubscribe-returning event
  callbacks).
- Dependencies: none new — Sokol already provides the event callback on all
  four targets.
- Verification: headless unit tests drive the input core through the
  simulation seam (query level/edge semantics, callback ordering, validation,
  focus clearing) on all four targets; a script-level simulation harness
  provides integration coverage; no golden-image test is added.

## Non-goals

- Gamepads, touch, pen, and other input devices — a future change.
- Sub-frame event latency (immediate dispatch from the platform callback) —
  frame-staged delivery only; low-latency dispatch is a future extension.
- Mouse-lock/pointer-capture, cursor visibility, and custom cursors —
  deferred until a concrete need appears.
- Key rebinding, input mapping/actions, or text-edit/IME semantics beyond the
  platform character stream.
- Exposing Sokol/GLFW keycodes or any host input types to scripts.
