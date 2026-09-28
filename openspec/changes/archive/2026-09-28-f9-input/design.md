# Design

## Context

The player opens a window through Sokol but `src/platform/platform.c` builds
`sapp_desc` with no `event_cb`, so no input is captured today. The pinned Sokol
snapshot (`vendor/sokol`, master `2e75443`) exposes `sapp_event`
(`sokol_app.h:1616`) but no query helpers (`sapp_key_down`/`sapp_mouse_x` are
absent from its public API), so the engine must own key/button state itself.
The renderer's 2D coordinate space is the framebuffer: `platform.c` calls
`efx_render_set_viewport(sapp_width(), sapp_height())`, and Sokol reports
`mouse_x`/`mouse_y` in framebuffer pixels, so input coordinates align with
`drawQuad` and the 2D camera frame without conversion. Two script bindings
exist and must stay identical: desktop quickjs (`src/api` + `src/runtime`) and
the web bridge (`src/web`, ADR 0022). Lifecycle hooks already establish the
callback/unsubscribe pattern (ADR 0016) and the host state that retains
`JSValue` callback lists (`src/runtime/runtime_internal.h`). The exploration
session settled the product shape: namespaced query + event APIs, Löve-model,
plain data, no resources, no visual gate. See `proposal.md` — Why.

## Goals / Non-Goals

**Goals:**
- One C-owned input state model shared by both bindings and all four targets,
  fed from the single Sokol event callback.
- Deterministic, frame-staged delivery so callbacks run inside a frame and
  queries and events never disagree.
- A script surface that is pure plain data, adds no resources, and follows the
  existing namespace/naming/error conventions.
- A deterministic injection seam that lets the whole layer be unit-tested and
  integration-tested with no window and no golden images.

**Non-Goals:** gamepad/touch/pen; sub-frame dispatch; mouse lock/cursor
control; input mapping/actions; IME/text-editing semantics; exposing backend
keycodes or host event types.

## Decisions

**D1 — A pure-C input core behind the module wall, fed by the platform.**
New `src/input/` owns a fixed state table (keys, buttons, pointer, deltas) and
a per-frame event queue, with a plain C API (`efx_input_*`). Only
`src/platform/platform.c` includes Sokol; its `event_cb` translates
`sapp_event` into core calls. `src/api`, `src/web/bridge.c`, and the tests all
consume the same core. This mirrors the `render` seam (the platform implements
the backend; callers use the engine interface) and keeps Sokol/GLFW enums
behind the wall (ADR 0003).
*Rejected: exposing Sokol query helpers* — the pinned snapshot has none, and
even where present they leak vendor enums and backend quirks.
*Rejected: JS/DOM listeners on the web* — would reimplement input against
`document`, break the single-C-surface rule (ADR 0022), and violate the
zero-host-dependency rule.

**D2 — Frame-staged dispatch, not immediate dispatch from `event_cb`.**
The callback updates level state and appends to a queue; once per frame, before
the update hooks, the engine drains the queue into the script callbacks in
arrival order, and edges are cleared at frame end. This is the Löve/raylib game
loop shape (poll events, then update) and matches the engine's
immediate-API/deferred-processing philosophy (the display list, ADR 0019). It
is deterministic, re-entrancy-safe (a callback may draw), and identical on
desktop and web.
*Rejected: calling JS directly from `event_cb`* — non-deterministic relative to
the frame, lets a callback record draws outside a frame, and on the web fires
outside `requestAnimationFrame`, diverging from desktop.

**D3 — Namespaced Löve-model API with string key names and event objects.**
`efx.keyboard`, `efx.mouse`, `efx.window` are sub-namespaces of the single
`efx` object (ADR 0004). Namespacing removes the `Key`/`Button` prefixes:
`isDown`/`isPressed`/`isReleased` (raylib-style predicates) and
`onDown`/`onUp`/`onChar`/`onMove`/`onWheel` (Löve-style callbacks), each
returning an unsubscribe function (ADR 0016). Key and button identifiers are
engine-owned lowercase strings (`'space'`, `'lshift'`, `'left'`, `'f1'`,
`'left'`/`'right'`/`'middle'`) mapped by the core onto platform keycodes; a
lookup table in `src/input/` is the single source for both bindings. Event
callbacks receive one plain object (`{ key, repeat, mods }`, `{ char }`,
`{ button, x, y, mods }`, `{ x, y, dx, dy }`, `{ dx, dy }`) — never a DOM/host
event.
*Rejected: flat top-level functions* (`efx.isKeyDown`) — the namespace keeps
the growing API organized and matches the requested shape.
*Rejected: numeric `efx.Key.A` constants* — string literals are the project's
enum convention (`'alpha'`, `'repeat'`) and type as a `.d.ts` union for
autocomplete; numeric codes are also less readable in samples.
*Rejected: a single generic `onInput({ type })`* — typed handlers are more
discoverable and match the separate lifecycle-hook functions.
*Rejected: Löve positional callback arguments* — the project passes one
options object once a call exceeds ~3 values, and objects extend without
signature churn.

**D4 — Coordinates are surface (framebuffer) pixels; expose size and DPI
scale.** `efx.mouse.position`/`x`/`y` and `efx.window.size`/`width`/`height`
report framebuffer pixels (top-left, y down) — the same space as `drawQuad`
and the 2D frame — so hit-testing needs no conversion. On high-DPI displays the
surface is larger than the logical window; `efx.window.dpiScale` reports the
surface-to-logical ratio so scripts can derive logical units. This is the
Löve/raylib split (screen-space input; a conversion helper is the script's or a
future high-level layer's job).
*Rejected: reporting the active 2D camera frame coordinates* — couples input
to camera state and call order, and has no meaning in 3D.
*Rejected: reporting logical/CSS pixels* — would disagree with the drawing
space and reintroduce a scale conversion on every hit-test.

**D5 — A deterministic simulation seam, no visual gate.** The input core
exposes an injection entry point (`efx_input_inject_*` or an equivalent
synthetic-event function) used only by the harness; it is not reachable from
scripts. Unit tests drive the real quickjs runtime and web bridge with
synthetic events, asserting level/edge semantics, callback ordering, key-name
validation, and focus clearing. A script-level harness provides integration
coverage. No golden image is added (input has no rendering surface).
*Rejected: a player test flag (`--input-script`) driving goldens* — input needs
no pixels, so the machinery would only add player surface area.
*Rejected: real DOM events through the browser harness* — end-to-end but
timing-dependent and far heavier than the value returned.

**D6 — The web build reuses the Sokol event path; defaults are suppressed at
the platform layer.** Sokol already installs DOM listeners on Emscripten and
calls `event_cb`, so the C core is fed identically on the web. The platform
layer, not the script layer, is responsible for suppressing the browser default
action for game keys (space/arrows scrolling the page), keeping the behavior
platform-specific and out of the spec surface.
*Rejected: web-only JS key handling* — see D1; it would diverge from desktop.

**D7 — Focus loss clears held state without synthetic up events.** On
`SAPP_EVENTTYPE_UNFOCUSED` the core clears all held keys/buttons so nothing
sticks. It does not emit up events, keeping the model simple and predictable;
scripts that need blur-aware behavior can react to a future focus event.
*Rejected: synthesizing up events on blur* — more state machine and surprising
callbacks for a v1 that only needs stuck-key avoidance.

**D8 — Fixed-size state, no resources, callbacks retained like hooks.**
Key/button state is a fixed table (no allocation); event callbacks reuse the
existing `efx_hook_list` machinery on desktop and the `entry.js` hook arrays on
web. Input adds no native-backed class, no `destroy()`, and no slot bank, so
the resource taxonomy and fixed-limits table are unchanged (ADR 0011/0013).

**D9 — Sequencing as an orthogonal milestone.** F9's only dependencies are the
F1–F2 window/frame loop and dual bindings, both green. The roadmap delta
declares F9 orthogonal to F3–F8 so it may land now without waiting for F7/F8.

## Risks / Trade-offs

- [Web keyboard focus / iframe embedding] → the gallery embeds the player in an
  iframe; game keys only reach the canvas when it has focus. Mitigation:
  suppress defaults at the platform layer, document the focus requirement, and
  cover the web path in the browser harness; a click-to-focus affordance is a
  gallery concern, not an API change.
- [Framebuffer vs logical pixels on high-DPI] → scripts that assume logical
  pixels may mis-scale. Mitigation: `dpiScale` is exposed and the coordinate
  rule is stated in the reference; hit-testing against drawing is correct by
  construction.
- [Event ordering relative to update hooks] → a callback that mutates state an
  update hook reads could surprise. Mitigation: the ordering (callbacks, then
  update, then render) is normative in the `input` spec.
- [Key-name set drift from the platform keycodes] → a missing mapping silently
  drops a key. Mitigation: one shared lookup table, plus a unit test that every
  documented name resolves and unknown names throw.
- [Sub-frame latency is not delivered] → acceptable now; the frame-staged model
  can add an opt-in immediate-dispatch mode later without changing the query
  surface.

## Migration Plan

Additive: no existing API changes and no behavior change when no input is
used. The only doc change is the roadmap gaining F9 and the reference gaining a
provisional input section. Rollback is deleting the new module and binding
entries; no data or format migration exists.

## Open Questions

- The exact canonical key-name list (Löve-style vs a smaller curated set) —
  settled during implementation and pinned by the unit test; no spec change.
- Whether the simulation seam is a narrow per-event injector or a small queue
  API — an internal choice, invisible to scripts.
- Whether a future change adds a pure-JS `screenToFrame`/`screenToWorld`
  helper (F8-layer candidate) — deferred.
