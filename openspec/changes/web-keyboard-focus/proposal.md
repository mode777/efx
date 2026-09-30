# Proposal

## Why

Keyboard input is dead in the web gallery's interactive samples: after
selecting the input playground and clicking the canvas, key presses never
reach `efx.keyboard`, while mouse input works. The root cause is the
interaction between two pinned decisions, not the sample:

- Sokol's web backend installs key listeners on the embedding document's
  `window` (the canvas cannot hold input focus), so keys only arrive when the
  player's document is the focused frame.
- Sokol's web backend consumes mouse events by default
  (`html5.bubble_mouse_events = false`), which makes Emscripten call
  `preventDefault()` on `mousedown`. Preventing the `mousedown` default
  cancels the browser's focus transfer, so clicking the canvas in an iframe
  never focuses the player's document.

The F9 and gallery-showcase designs assumed "click to focus" was a working
gallery affordance. It is not: the engine swallows the very event that would
focus it. The gallery smoke harness never sent an input event, so the gap was
invisible to CI.

## What Changes

- The web build SHALL let native pointer focus work: the platform layer sets
  `html5.bubble_mouse_events = true` on the web target, so a click on the
  canvas moves focus into the player's document and keyboard input then
  reaches scripts. This fixes all web embeds (gallery iframe and standalone
  page), not just the showcase.
- Game-key default suppression (space/arrows scrolling or navigating the host
  page) is preserved; only the mouse-default consumption that blocks focus is
  removed.
- The gallery smoke harness (`tools/run_gallery_smoke.mjs`) SHALL gain a
  regression check: focus the runner by interacting with the application
  area, send a key event, and assert it reaches the running sample's
  `efx.keyboard` state.
- A new ADR records the durable rule (web pointer input must not suppress the
  focus default that keyboard delivery depends on) and links back to the
  gallery-embedding and F9 records.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `input`: add a requirement for web keyboard focus in an embed — a click on
  the canvas moves focus into the player's document and keyboard input then
  reaches scripts, while game-key default suppression is retained.
- `web-gallery`: add a requirement that the gallery smoke run exercises
  keyboard input through the embed and fails when it does not reach the
  running sample.

## Impact

- Code: `src/platform/platform.c` (web `sapp_desc`), `tools/run_gallery_smoke.mjs`
  (regression check). No engine input-core, renderer, or binding change.
- Script API: none. `docs/js-api.md` and the generated `docs/api/` reference
  are unchanged.
- Decisions: new `docs/decisions/0043-*.md` plus an index row; it references
  ADR 0030 (gallery iframe embedding) and ADR 0036 (F9 input).
- Platform behavior: on the web, mouse events now bubble to the host document
  instead of being consumed. Desktop targets are unchanged. No golden image
  changes.
- Milestone: F9 (input) follow-up fix; no roadmap milestone moves.

## Non-goals

- No change to desktop input behavior or to the C input core.
- No new script-facing API and no keyboard-listener relocation.
- No gallery UI change (no focus cue or overlay); the fix makes the existing
  click-to-focus path work.
- No golden-image re-baseline.
