# Design

## Context

Sokol's pinned web backend installs keydown/keyup/keypress listeners on the
embedding document's `window` (the canvas cannot hold input focus) and
consumes mouse events by default: `_sapp_emsc_mouse_cb` returns
`!html5.bubble_mouse_events`, which Emscripten turns into
`event.preventDefault()`. Preventing the `mousedown` default cancels the
browser's focus transfer, so a click on the canvas inside an iframe never
focuses the player's document and no key event reaches the engine. Mouse
input still works because its listeners are attached to the canvas.

`src/platform/platform.c` builds `sapp_desc` without touching
`html5.bubble_mouse_events`, so the default (`false`) applies. The gallery
embeds `runner.html` in an iframe (ADR 0030) whose canvas is
`tabindex="-1"`. Verified on the verification server against the built
gallery: after clicking the canvas, the runner's `document.activeElement` is
`BODY` and `document.hasFocus()` is false, with the mousedown marked
`defaultPrevented`; calling `canvas.focus()` makes a subsequent key reach
`efx.keyboard`. See `proposal.md` — Why.

## Goals / Non-Goals

**Goals:**
- Restore the browser's pointer focus transfer on the web so a click on the
  canvas focuses the player's document and keyboard input reaches scripts.
- Preserve the existing game-key default suppression (space/arrows must not
  scroll or navigate the host page).
- Guard the behavior with an automated gallery-smoke check so the gap cannot
  regress silently.

**Non-Goals:**
- No change to the C input core, desktop targets, or the script-facing API.
- No gallery UI focus cue or overlay.
- No relocation of keyboard listeners or vendor/engine re-architecture.

## Decisions

**D1 — Set `html5.bubble_mouse_events = true` on the web target in
`platform.c`.** This is the smallest change at the layer that owns browser
default handling (F9 design D6) and fixes every web embed — the gallery
iframe and a standalone page alike — by letting the browser perform its
native focus-on-click. `bubble_key_events` and `bubble_wheel_events` stay at
their defaults, so key suppression and wheel handling are untouched.
*Rejected: focus the canvas from a runner/gallery pointerdown handler.*
Works, but is host-local (ADR 0030) and leaves every other web embed broken;
it also papers over the platform-level default rather than fixing it. It
remains a possible future enhancement for hosts that steal focus, not the
fix.
*Rejected: focus the canvas on load.* Steals focus from the parent editor and
is often blocked without a user gesture.
*Rejected: move key listeners to the canvas and add `tabindex="0"`.* Crosses
the vendor line, still requires focus, and does not address the iframe
embedding case.

**D2 — Leave game-key suppression as-is.** `efx_event_cb` suppresses the
default for game keys via `efx_web_game_key` + `sapp_consume_event`, which is
independent of mouse-event bubbling. No interaction is expected; the change
must be verified on the web build.

**D3 — Add the regression to the existing gallery smoke
(`tools/run_gallery_smoke.mjs`), not a new harness.** The smoke already
drives the built site and the runner iframe, so it is the cheapest place to
catch this. It will: load a tiny inline sample through the existing editor
channel that records `efx.keyboard` state into a global; click the center of
the application area (`page.mouse.click` over the iframe) to move focus;
send a key-down (`page.keyboard.down`); then poll the runner for the recorded
state and assert it observed the key. It should also assert the runner's
`document.hasFocus()` after the click, which isolates a focus failure from a
key-mapping failure. Keeping it in the gallery smoke means
`tools/verify_remote.py gallery` covers it before CI.

**D4 — Record the durable rule in a new ADR (0043).** The rule — on the web,
pointer input must not suppress the focus default that keyboard delivery
depends on — outlives this fix and constrains future platform work. The ADR
references ADR 0030 (gallery iframe embedding) and ADR 0036 (F9 input).

## Risks / Trade-offs

- [Mouse events now bubble on the web] → A drag on the canvas could select
  text or trigger host-page behavior. The canvas/document carry no text, and
  wheel bubbling stays disabled; the smoke run plus a manual web check
  confirm no console/page errors and no unexpected navigation.
- [The fix relies on the browser focusing the frame on mousedown] → A host
  that itself steals focus could still break delivery. Mitigation: the
  regression test; a gallery focus cue remains a deferred host concern.
- [Standalone web embedders that relied on consumed mouse events] → The
  behavior change is documented in the ADR; canvas default actions are
  benign and no engine API is affected.

## Migration Plan

Behavior-only and additive. Rollback is reverting the one `sapp_desc` flag.
No data or format migration, no golden re-baseline.
