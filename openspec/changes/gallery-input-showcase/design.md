# Design

## Context

F9 (input) is implemented: `efx.keyboard` / `efx.mouse` / `efx.window`,
frame-staged callbacks before update hooks, surface-pixel coordinates,
`dpiScale` (ADR 0036). The gallery hosts each sample in an isolated iframe
whose canvas must be focused for key events to reach the engine; a fresh
iframe is created per run (ADR 0030). This change adds one curated sample
and no code to the shell, the runner, or the engine.

## Goals / Non-Goals

**Goals:** a single sample that is (a) genuinely interactive and fun, (b)
self-playing so it reads as a demo before focus, (c) a legible tour of the
*whole* F9 surface (events, queries, properties, window, coordinate rule),
and (d) robust in the headless gallery smoke (no console/page errors, small
fixed work per frame).

**Non-Goals:** new API, engine/renderer change, asset pack, golden scene,
gallery UI change.

## Decisions

**D1 — One 2D scene with a fixed virtual frame.** The sample uses
`setCamera2D({ frame: [640, 480] })` and draws additive quads from
`efx.whiteTexture`; no meshes or lighting. This keeps the sample pure and
makes the coordinate demonstration explicit. Rejected: a 3D scene — more
state, no extra F9 coverage.

**D2 — Surface→frame mapping is shown, not hidden.** `efx.mouse.position`
is in surface pixels; the frame is 640×480. Pointer positions are mapped
`frame = pointer * frameSize / efx.window.size`. This is exactly the rule
scripts must apply for hit-testing, so the sample teaches it. A
divide-by-zero guard falls back to the identity when the window is
reporting zero.

**D3 — Both API styles, deliberately.**
- Event callbacks (`onMove`/`onDown`/`onWheel`) drive *discrete*
  contributions: paint at the pointer, emit a burst, change the brush.
- Queries (`isPressed` for one-shots, `isDown` for held state) drive
  *continuous* behavior: palette keys, clear, movement, modifiers.
This mirrors the docs' guidance and avoids putting per-frame polling in a
callback or one-shots in a query.

**D4 — Attract mode, no first-frame emptiness.** The sample keeps ~a
handful of autonomous emitters wandering when no pointer input has arrived
for a moment, and pulses a frame border as a "click to interact" hint. Any
input cancels attract mode. This is a sample-level concern; it is not an
engine feature. Rejected: an external gallery overlay — the runner is
reused unchanged.

**D5 — Fixed budget, no allocation spike.** Particles live in a
preallocated array with a hard cap (400); spawned particles overwrite the
oldest when full. `dt` is clamped, so a long frame cannot tunnel every
particle. This bounds per-frame `drawQuad` work for the smoke and keeps the
sample's memory flat.

**D6 — Deterministic-ish idle, no RNG dependency.** `Math.random` is
standard ES6 and allowed, but the demo seeds spawns from a tiny integer
hash of the frame count and index so the visual character is stable and the
code stays readable; randomness is visual only.

## Risks / Trade-offs

- [Iframe focus: keys do nothing until click] → attract mode keeps it
  alive; the hint frame invites the click; keyboard movement is a bonus,
  not the only way to interact.
- [Headless smoke never sends input] → the idle path must itself be
  error-free and non-empty; verified by the gallery smoke screenshot.
- [Particle count vs software renderer] → cap 400 additive quads; the
  existing glowing-post samples already draw comparable counts.

## Open Questions

None.
