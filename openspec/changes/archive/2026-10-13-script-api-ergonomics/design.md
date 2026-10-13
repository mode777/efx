# Design

## Context

Eleven small API changes agreed during the demo-duplication survey (see
proposal.md — Why). They share implementation surface: the prelude wire
validators (ADR 0049), the C API modules, the platform input/audio hooks, the
type document + generated reference flow (ADR 0048), and the four-target
golden gate (ADR 0020). Two of them are convention changes (input units,
audio unlock ownership) that cut against text currently in the specs and
docs; one relaxes the math layer's implementation locus. Constraints that
shape the design: fixed-function only (ADR 0015), marshal-only natives with
shared-prelude validation (ADR 0049), plain-JS-data script math (ADR 0010),
no new unmanaged resources.

## Goals / Non-Goals

**Goals:**

- Each addition removes a documented per-sample workaround (mapping helpers,
  unlock gates, recreate-on-teleport, matrix ladders, hand-rolled clock).
- Zero new native-backed classes; zero growth in the fixed-limits table.
- Identical behavior and error text across quickjs desktop and the web
  bridge for every touched entry.

**Non-Goals:**

- No kit/comfort library, no material shorthand, no `sfx`, no `fadeTo`, no
  2D primitives (separate future change), no gradient texture API (demos
  ship offline PNGs), no world-space 2D mouse accessor for zoomed cameras.

## Decisions

### D1 — `mat4.transform` is native; rotation order pinned

`mat4.transform(pos, yawDeg?, pitchDeg?, rollDeg?, scale?)` composes
`T(pos) · Ry(yaw) · Rx(pitch) · Rz(roll) · S(scale)` in one native call,
returning a fresh 16-number array.

- *Why native*: the user directed it for performance; the survey shows it is
  the highest-frequency per-draw call (8+ samples). Native composition via
  GLM avoids three intermediate matrices and four array allocations per
  call. It stays within ADR 0049: marshal-only native (arrays in, array
  out), option validation in the shared prelude.
- *Why not pure JS*: ADR 0010's rule is about script-visible *data* (plain
  arrays, degrees), not implementation locus; the spec text is amended to
  scope "entirely `[JS]`" to the scalar and pre-existing helpers (see the
  3d-core delta). Losing the pure-JS property would matter if scripts could
  subclass/inject — they cannot.
- *Why yaw→pitch→roll about Y/X/Z*: matches the dominant usage (all per-frame
  demo rotations are yaw); pitch/roll ride along at zero cost in the same
  composition. Rejected: an `axis` parameter (nobody in the corpus needs a
  non-Y per-frame axis; compose manually for that rare case — one
  `rotate()` call); a quaternion-taking overload (scripts would need quat
  construction first — heavier than three angles); an options bag (hot
  path; positional args keep call sites flat).

### D2 — `screenRay` reads live camera state; frame-space coordinates

`screenRay(x, y)` returns `{ origin, direction }` (fresh plain arrays) from
the most recently set 3D camera. It is a query, not a record: no display-list
entry, no per-record snapshot semantics (contrast `drawMesh`, ADR 0019).
`x, y` use the same space mouse input reports (D3) so
`screenRay(...efx.mouse.position)` is the canonical pick. No 3D camera ever
set → `Error` (a state error, not an argument error); bad coordinates keep
the argument convention.

- Rejected: returning `[origin, direction]` as an array-of-arrays (named
  fields read better at call sites and survive future fields); requiring an
  explicit `{ frame }` opt (the default covers every caller in the corpus;
  the frame mapping is already defined by D3); exposing camera basis
  vectors as a separate API (the ray is the only thing demos build from the
  camera — the basis math was scaffolding for the ray).

### D3 — Frame-space mouse units: scale-only mapping at expose time

`api_input.c` maps surface→frame at expose time (event payloads and polled
getters read through one mapping function): `px × frame/surface` per axis,
using the most recent `setCamera2D` frame; identity when the frame was never
set (`frame_w == 0`), mirroring the renderer's own fallback
(`render_particles.c:663`). Deltas scale by the same ratio; `wheel` is
untouched; `window.size`/`dpiScale` stay surface pixels.

- *Why scale-only*: Godot's precedent — the viewport/stretch transform is a
  resolution transform, while camera pan/zoom/rotation is deliberately
  excluded from input events (`get_global_mouse_position()` is the separate,
  explicit world-space accessor). Scale-only is also state-stable: the same
  cursor means the same frame point regardless of last frame's camera.
  Rejected: folding the 2D camera inverse in (semantics shift under camera
  motion; wrong for HUD input; nothing in the corpus needs it); a
  `mouse.framePosition` getter next to the existing members (two spaces for
  one pointer is the confusion we are removing); leaving units as surface
  pixels with a converter helper (keeps the two-step dance alive; device
  pixels are the one space browsers never use for input — `clientX` is CSS
  px, the framebuffer lives behind `devicePixelRatio`).
- *Browser HD note*: this change makes the input/metrics split mirror the
  browser's: input in logical (frame) space, physical surface behind an
  explicit ratio (`window.dpiScale`).

### D4 — Engine-managed audio unlock; delete `efx.audio.resume()`

`efx_input_key_down` and `efx_input_mouse_down` fire
`efx_audio_request_resume()` once (first gesture; the function is already
idempotent and desktop registers no callback — `audio.h:55`). The web
binding's `audio.resume` wrapper (`web/js/audio.js:103`) and the desktop
binding (`api_audio.c`) are removed. Playback-handle `resume()` is
untouched (different verb; pause/resume of one playback).

- Why the input hook beats the script verb: sokol dispatches DOM events
  synchronously inside the browser gesture, so the engine-side call always
  lands in a valid activation; a script call from `update()` never does —
  which is exactly the flag/retry boilerplate this deletes.
- Rejected: keeping `resume()` as a documented no-op (dead surface,
  contradicts "concise"; its existence implies it does something); unlocking
  on `mousemove` (browsers do not grant activation for pure moves; hovering
  a page would unlock audio nobody asked to hear).
- Documented limitation: gamepad input does not grant browser activation;
  a gamepad-only web session waits for one key/click. Unfixable from script
  either way.

### D5 — Degenerate sizes are silent no-ops

`drawQuad`/`drawBillboard` with a finite `size` component ≤ 0 record nothing
and do not throw. All other size validation (non-numeric, infinite, wrong
arity) and all `sourceRect` validation are unchanged; a zero-extent
*source rect* still throws (that is a sampling contract, not a size).

- Why no-op beats throw: the failing case is legitimate data (a health bar
  at exactly 0), so every UI-writing sample carries a guard
  (gamepad-tester's `rect()` documents it in a comment). No-op preserves
  the real error cases.
- Rejected: clamping to a minimum size (hides authoring bugs behind a
  1-pixel sliver); warning-once (log noise in a hot path; the golden corpus
  must stay deterministic).

### D6 — `setPosition`: zero velocity, wake, all body kinds + character

Teleport semantics: instant placement, linear + angular velocity zeroed,
sleeping bodies woken. `Character.setPosition(p)` gets the same semantics —
without it, sky-steps' character recreate workaround (one of the three
motivating cases) survives.

- Rejected: a `{ keepVelocity }` option (conciseness; all three demo
  workarounds zero velocity, and a caller who wants momentum can reapply it
  with `applyImpulse`); swept teleport (a teleport is a placement decision,
  not motion — sweeping would defeat respawning inside geometry on
  purpose).

### D7 — `debugDraw`: unlit translucent solids through the existing pipeline

One `efx.physics.debugDraw({ color?, alpha? })` call during render enumerates
live bodies/characters and records unit-hull geometry scaled per shape
(box/sphere/capsule generators; static mesh bodies draw their collision
mesh) with an unlit, alpha-blended material (ADR 0058's `unlit` flag) into
the display list — sortable, depth-tested, and free when not called.

- Rejected: wireframe/line rendering (no line primitive exists in the
  fixed-function API; a line mode would need a new canned shader for a debug
  verb — ADR 0015 tension); a per-body `debug` flag (same output, more
  API surface); script-side readback of body shapes (would expose the
  retained shape data the API deliberately keeps native).
- Throws outside the render phase (mirrors draw-call placement rules).

### D8 — Color tolerance at the validation choke point

3-component acceptance is implemented once in the shared color parse (wire
validators + the C color parser), not per callsite, so every color-typed
argument gains it uniformly and both runtimes stay byte-identical
("Identical argument errors" requirement). The d.ts `Color` type widens to
the 3-or-4 union; docs/api regenerates.

### D9 — `efx.time.now`: native, frame-start snapshot, clamped advance

The runtime accumulates dt per frame with a max-step clamp (0.05s — the
value all six game samples converge on by hand) and snapshots `now` at
frame start; update hooks, render hooks, and input callbacks read one
number per frame. Native per user direction; a prelude
`registerUpdateHook`-based clock was rejected because hook ordering against
user hooks would be fragile and the runtime owns the loop anyway.

- Rejected: also exposing `efx.time.delta` (update already receives `dt`;
  a second source of the same number is fat).

### D10 — `uvs` on `makePlane` only

`uvs?: number | [number, number]` (positive, default 1) tiles the plane's
uv span. Rejected: the same option on `makeCube` (no demo needs per-face
tiling today; add with a need — the primitive spec stays small); an
`uvOffset` companion (scrolling uv can be a material/texture concern later).

### Resource exposure

No new unmanaged resources: `setPosition` is a method on the existing
GC-finalized `Body`/`Character` classes (explicit `destroy()` discipline
unchanged, ADR 0011/0012); `screenRay` and `mat4.transform` return plain
JS data; `debugDraw` is a namespace function with no state; `efx.time.now`
is a number. The native-backed class list and fixed-limits table are
unchanged.

## Risks / Trade-offs

- [Input unit change silently breaks out-of-repo scripts that assumed
  surface pixels] → In-repo migration covers all curated samples; the
  frame-less fallback is identity so 3D-only scripts are unaffected; the
  ADR and js-api.md document the units prominently; golden scenes read no
  input, so the gate is unaffected.
- [`mat4.transform` native results diverge between GLM versions/targets]
  → Results are IEEE-deterministic double/float math identical to the
  camera path; the cross-runtime test compares serialized matrices from
  both runtimes byte-for-byte (same harness as the error catalog).
- [Auto-unlock on first input fires for scripts that never use audio]
  → Unlock is a no-op without an audio device and lazily initializes
  nothing on desktop; on web the context resume is free when no audio was
  requested.
- [`debugDraw` hull geometry costs at large body counts] → Recording is
  O(bodies) with cached unit meshes; demos run tens of bodies; acceptable
  for a debug verb, documented as debug-only.
- [Time clamp hides long stalls from scripts that want real time]
  → The clamp is the point (animation stability); scripts needing wall
  time can sum raw `dt` themselves. Rejected exposing a configurable clamp
  (fat; revisit on a real need).

## Migration Plan

All breaking changes land in one release: mouse units, degenerate-size
no-ops, `audio.resume` removal. In-repo migration is part of this change
(curated samples). Rollback is a single revert — no data or asset-format
dependencies. Out-of-repo script impact is limited to input units (frame =
surface when no frame is set, so frame-less scripts are unaffected) and the
removed `resume()`.

## Open Questions

- None. Signature details (`transform` argument order, `debugDraw` defaults,
  `uvs` shapes) are pinned in the spec deltas.
