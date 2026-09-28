# 0036 — Input is C-owned and frame-staged; scripts get queries plus unsubscribe callbacks

Status: Accepted (2026-09, change `f9-input`)

Supports: ADR 0003 (module walls), ADR 0004 (single `efx` namespace),
ADR 0016 (explicit stacking registration with unsubscribe), ADR 0022
(desktop quickjs / web bridge parity).

## Context

The player opened a window but installed no Sokol event callback, so scripts
had no way to read the keyboard or mouse. The pinned Sokol snapshot exposes
`sapp_event` but no query helpers, and the two script bindings (desktop
quickjs and the web bridge) must stay identical. Input also must be testable
without a window or a golden image.

## Decision

- **One pure-C input core (`src/input/`).** A fixed key/button state table,
  pointer/accumulator state, and a fixed per-frame event queue. No Sokol, no
  quickjs. Only `src/platform/platform.c` includes Sokol and translates
  `sapp_event` into `efx_input_*` arrival calls through the single
  `sapp_desc.event_cb`; both bindings and the tests consume the same core.
- **Frame-staged delivery.** Level state and queued events update on arrival;
  movement/wheel accumulate between frames and commit at
  `efx_input_begin_frame`. Before the update hooks, each binding drains the
  queue into the registered callbacks in arrival order, then clears it.
  Press/release edges and per-frame deltas are valid for exactly one frame
  and cleared at `efx_input_end_frame`. Focus loss clears held state without
  emitting synthetic up events.
- **Namespaced script surface, no resources.** `efx.keyboard`, `efx.mouse`,
  `efx.window` are members of the single `efx` object. Queries are
  `isDown`/`isPressed`/`isReleased`; events are `onDown`/`onUp`/`onChar` and
  `onDown`/`onUp`/`onMove`/`onWheel`, each returning an idempotent
  unsubscribe function built on the existing hook-list machinery. Event
  arguments are one plain object of engine primitives (`{ key, repeat, mods }`,
  `{ key, mods }`, `{ char }`, `{ button, x, y, mods }`, `{ x, y, dx, dy }`,
  `{ dx, dy }`), never a DOM/host event. Input adds no native-backed class,
  no `destroy()`, and no slot bank (ADR 0011/0013 unchanged).
- **Coordinates are surface (framebuffer) pixels,** top-left origin, y down —
  the same space as `drawQuad` and the 2D frame; `efx.window.dpiScale` is the
  surface-to-logical ratio. Key/button identifiers are engine-owned lowercase
  strings from one lookup table; unknown names throw `TypeError`.
- **Deterministic seam.** `efx_input_inject_*` synthesizes key/char/mouse
  events through the same arrival path; it is used only by tests and is not
  reachable from scripts.
- **Web defaults are suppressed in the platform layer** with
  `sapp_consume_event()` for game keys, so the behavior is platform-specific
  and out of the script contract.

## Consequences

- Input behaves identically on all four targets because the state model and
  dispatch order live in C; the bindings only marshal.
- Callbacks run inside a frame, in arrival order, before update hooks, so
  queries and events never disagree within a frame and a callback may record
  draws.
- Input has no visual surface, so its gate is headless unit tests plus a
  script-level harness (no golden image).
- Sub-frame (immediate) dispatch is not delivered; the frame-staged model can
  gain an opt-in mode later without changing the query surface.
- The numeric key ids use the platform's stable virtual-keycode values to
  keep the platform translation a direct cast; scripts only ever see the
  engine-owned name table, so the ids never leak.

## Rejected alternatives

- Exposing Sokol query helpers — the pinned snapshot has none, and they leak
  vendor enums and backend quirks.
- JS/DOM listeners on the web — would diverge from desktop, break the
  single-C-surface rule, and add host dependencies.
- Immediate dispatch from `event_cb` — timing is non-deterministic relative to
  the frame and on the web fires outside `requestAnimationFrame`.
- Synthesizing up events on blur — more state machine and surprising
  callbacks for a v1 that only needs stuck-key avoidance.
- A player test flag driving goldens — input needs no pixels, so the
  machinery would only add player surface area.
