# 0043 — Web pointer input must not suppress the focus default that keyboard delivery depends on

Status: Accepted (2026-09, change `web-keyboard-focus`)

Supports: ADR 0022 (the browser's native JS engine drives the web bridge);
ADR 0030 (the gallery embeds the web player per-run in an iframe);
ADR 0036 (F9 input is C-owned and frame-staged).

## Context

Sokol's web backend installs keydown/keyup/keypress listeners on the
embedding document's `window`, because an HTML canvas cannot itself hold
input focus. Keyboard therefore reaches the engine only while the player's
document is the focused frame. Sokol's web backend also consumes mouse events
by default (`sapp_desc.html5.bubble_mouse_events = false`), which Emscripten
turns into `event.preventDefault()` on `mousedown`. Preventing the
`mousedown` default cancels the browser's focus transfer, so clicking the
canvas inside an iframe never focuses the player's document and no key event
ever arrives. Mouse input still works because its listeners are attached to
the canvas. The F9 and gallery-showcase designs had assumed a working
"click to focus" affordance, and the gallery smoke sent no input, so the gap
was invisible to CI. Full record: `openspec/changes/web-keyboard-focus/`.

## Decision

- **On the web target, the platform layer sets
  `sapp_desc.html5.bubble_mouse_events = true`** in
  `src/platform/platform.c`. Mouse events are no longer consumed, so the
  browser's native focus-on-click moves focus into the player's document
  (gallery iframe or standalone page) and subsequent key events reach
  `efx.keyboard`.
- **Key and wheel default handling is unchanged.** `bubble_key_events` and
  `bubble_wheel_events` stay at their defaults, and game-key default
  suppression (`sapp_consume_event()` for space/arrows) still runs in the
  platform event callback, independently of mouse bubbling.
- **Focus is the host's concern.** The engine delivers keys once its document
  is focused; an embedding host that steals focus after a click can still
  interrupt delivery, and a focus cue remains a host-level affordance, not an
  engine API.
- **The gallery smoke asserts the path**: it focuses the runner by clicking
  the application area, sends a key-down, and requires the running sample to
  observe it (`tools/run_gallery_smoke.mjs`).

## Consequences

- Future web platform work MUST NOT re-enable mouse-event consumption: doing
  so silently kills keyboard input in iframe embeds with no script-visible
  symptom.
- Standalone web pages now see mouse events bubble to the host document. The
  canvas and runner document carry no text and wheel bubbling stays disabled,
  so the default actions are benign; embedders relying on consumed mouse
  events must adapt.
- Desktop targets and the C input core are untouched; no script-facing API,
  `js-api` delta, or golden image changes.
- The regression is guarded on the pre-CI verification server by
  `tools/verify_remote.py gallery` and in the four-target gate.

## Rejected alternatives

- **Focus the canvas from a runner/gallery `pointerdown` handler.** Works, but
  is host-local (ADR 0030), leaves every other web embed broken, and papers
  over the platform default rather than fixing it.
- **Focus the canvas/iframe on load.** Steals focus from the parent editor and
  is often blocked without a user gesture.
- **Move key listeners to the canvas and add `tabindex="0"`.** Crosses the
  vendor line, still requires the element to be focused, and does not address
  the iframe embedding case.
- **Leave the default and only document the click requirement.** The click is
  exactly what the consumed `mousedown` prevents, so the documented
  workaround does not work.
