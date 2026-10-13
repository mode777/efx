# Proposal

## Why

A duplication survey of the curated demos (24 samples, ~4,200 lines; the six
`game-*` samples alone carry ~2,750) found ~1,000 lines of scaffolding
re-implemented per sample: surface→frame pointer mapping, hand-rolled camera
rays, per-sample clocks and `clamp()`, identity→translate→rotate→scale matrix
ladders, "teleport = destroy + recreate" physics workarounds, and audio
unlock/retry dances. Two sharp edges force more boilerplate: `drawQuad` /
`drawBillboard` throw on zero sizes (health bars at 0 crash), and mouse input
arrives in device pixels — the one space browsers deliberately keep away from
input and the engine's own 2D frame already abstracts away. The engine owns
the state behind every one of these patterns; exposing it once (and fixing the
two edges) makes games concise without adding a comfort library.

## What Changes

**Additions**

- `efx.time.now` — script-facing engine clock: seconds since script start,
  snapshotted at frame start, internal max-step clamp; native-implemented
  (new `efx.time` sub-namespace).
- `efx.math.mat4.transform(pos, yawDeg?, pitchDeg?, rollDeg?, scale?)` —
  one-call model matrix (translate → yaw/pitch/roll → scale), **native**
  implementation for hot-path performance (plain-array marshal, no
  intermediate matrices); validation stays in the shared prelude.
- `efx.math.clamp(v, lo, hi)` and `efx.math.lerp(a, b, t)` — pure-JS scalar
  helpers.
- `efx.graphics.screenRay(x, y)` — camera→world ray through a frame-space
  screen point; returns `{ origin, direction }`; requires an active 3D
  camera; composes with the existing `physics.raycast` for picking.
- `body.setPosition(p)` — teleport a live body (static, dynamic, sensor) or
  character: clears linear/angular velocity, wakes the solver. Deletes the
  recreate workaround in three demos.
- `efx.physics.debugDraw(opts?)` — render-time debug view of every collider
  as unlit translucent geometry recorded into the display list.
- `makePlane({ uvs })` — UV tiling option (span, default 1), aligning the
  plane with the other primitives' configurability.

**Tolerance / behavior fixes**

- `Color` acceptance: 3-component `[r, g, b]` arrays are accepted everywhere
  a `Color` is expected, with alpha defaulting to 1.
- **BREAKING** `drawQuad` / `drawBillboard`: a `size` component ≤ 0 is a
  silent no-op (no display-list record) instead of a thrown `RangeError`;
  all other size validation is unchanged.
- **BREAKING** mouse input units: pointer `position`/`x`/`y`/`delta` and
  mouse-event `{x, y, dx, dy}` are reported in **frame pixels** — the active
  2D frame's space (scale-only mapping; identity when no frame was ever
  set), matching `drawQuad`'s coordinate space exactly. `window.size` stays
  surface pixels. `wheel` is unchanged (scroll ticks are not positions).

**Removal**

- **BREAKING** `efx.audio.resume()` is removed. The engine unlocks audio
  itself on the first key-down/mouse-down (inside a real user gesture on
  web, where a script call from `update()` never is); desktop stays a
  no-op. Playback handles keep `resume()`.

Demo-facing work in the same change: strip the now-dead plumbing from the
curated samples that motivated each item (mapping helpers, unlock gates,
teleport workarounds, hand-rolled `clamp`/clock/matrix ladders).

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities

- `js-api`: namespace member lists change — `efx.graphics` gains `screenRay`;
  `efx.math` gains `mat4.transform`, `clamp`, `lerp` (and is no longer
  purely `[JS]`-implemented); `efx.physics` gains `debugDraw` and `Body`
  gains `setPosition`; `efx.audio` loses `resume()`; mouse position members
  are documented in frame pixels; the `efx` root gains the `time`
  sub-namespace; new color-acceptance rule.
- `3d-core`: `Script math layer` admits a native-backed hot composition
  helper and the new members; `Procedural primitives` gains the `makePlane`
  `uvs` option; new screen-to-world ray requirement for `screenRay`.
- `2d-layer`: `Quad drawing` degenerate-size semantics (≤ 0 → no-op).
- `billboards`: `World-space billboard drawing` degenerate-size semantics.
- `input`: `Mouse semantics and coordinate space` — pointer coordinates move
  from surface pixels to frame pixels.
- `audio`: `Web autoplay unlock` replaced by engine-managed unlock (no
  script-facing resume entry point).
- `collision-physics`: new body teleport + collider debug-drawing
  requirement (`setPosition`, `debugDraw`).
- `js-runtime`: new script clock requirement (`efx.time.now`).

## Impact

- **Code**: `src/api/` (`api_2d.c`, `api_3d.c`, `api_physics.c`,
  `api_input.c`, `api_audio.c`), native math composition (GLM-backed, next
  to the existing camera math), `src/runtime/` (frame clock),
  `src/audio/` + `src/platform/` (gesture-driven unlock hook already present
  as `efx_audio_request_resume`), `src/prelude/prelude.js` (+ `clamp`,
  `lerp`, `makePlane` `uvs`, color-tolerance in the wire validators;
  regenerate `prelude.h`), web bridge `src/web/js/audio.js` (drop `resume`).
- **Script API**: `gallery/src/api/efx.d.ts` + regenerated `docs/api/`
  (ADR 0048 flow); `Color` type widens to 3-or-4 component; gallery
  type-test additions.
- **Docs**: new ADR `docs/decisions/0060` covering the three cross-cutting
  convention changes (native hot math, frame-space input units,
  engine-managed audio unlock) + `docs/decisions/README.md` index;
  `docs/js-api.md` guideline touches where the layering/input rules are
  stated.
- **Samples**: curated demos drop the boilerplate this change obsoletes.
- **Verification**: unit tests (clock snapshot/clamp, transform math vs GLM,
  screenRay, setPosition wake/velocity semantics, color tolerance, degenerate
  size no-op), one new golden scene exercising a zero-size quad among valid
  draws (ADR 0020 harness), cross-runtime error-catalog updates, four-target
  gate.

## Non-goals

- No `sfx()` wrapper (audio unlock removal + existing mixer pan clamping
  make it empty).
- No gradient/texture-generation API — demos that need generated art will
  ship offline PNGs.
- No material shorthand — syntactic sugar for a library on top later.
- No `fadeTo` audio tween, no `drawRect`/2D primitives (separate future
  change), no screen-shake/banner/attract-mode conveniences (kit material).
- No world-space 2D mouse accessor for zoomed 2D cameras (scale-only frame
  mapping now; can layer later without breaking it).
- No kit/comfort library — the engine stays complete but concise.
