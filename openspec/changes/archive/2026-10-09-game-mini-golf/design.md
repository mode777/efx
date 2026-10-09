# Design

## Context

See `proposal.md` — Why. The physics surface: `createBody` (box/sphere,
dynamic, mass/friction/restitution, sensor), `createStaticMesh` for triangle
meshes, `step`, `raycast`, `overlap`; `EfxBody` exposes writable `velocity`
and `applyImpulse`. 3D: `setCamera3D`, `makePlane`/`makeCube`, materials
with procedural `createImageData` textures, blob-shadow billboard pattern.

## Goals / Non-Goals

**Goals:** impulse dynamics as game feel — aim, charge, strike, roll, sink —
with a data-authored 9-hole course and no assets beyond the font.

**Non-Goals:** moving obstacles, audio (capstone's bank), orbit camera,
persistence, engine changes.

## Decisions

### D1 — Holes are data; geometry is generated from the data

Each hole: `{ walls[], ramps[], tee, cup, par }`. Walls become static box
bodies + rendered boxes; ramps become static triangle-mesh bodies (vertex
arrays like the physics showcase's ramp) + rendered meshes; every hole's
wall layout fully encloses the course so the ball cannot leave — a design
guarantee, no out-of-bounds handling needed.

### D2 — Ball: dynamic sphere tuned for roll, not bounce

High mass relative to stroke impulse cap, moderate friction, low restitution
(walls slightly livelier than the ground). Stop detection: `|velocity| <
epsilon` for N consecutive frames → aiming state.

### D3 — Aiming: mouse raycast to the ground plane; charge = ping-pong meter

On the ground plane through the cursor: build a ray from the 3D camera and
cursor (`raycast` from camera position along the cursor direction) and take
the plane hit; the aim direction is ball → hit, clamped to a reasonable
pitch band (no straight-up strokes). Hold LMB to charge a 0→1→0 ping-pong
meter (skill timing); release applies `impulse = dir * power * cap` via
`applyImpulse`. Aim line + meter rendered as thin quads-in-3D/unlit lines
(2D overlay line for the meter).

- **Rejected — drag-back-to-set-power (slingshot)**: needs a world-space
  drag plane and fights the follow camera; the ping-pong meter is one
  timing skill and renders as plain 2D.
- **Rejected — orbit camera**: an orbiting camera makes the cursor raycast's
  meaning change with the camera; a fixed elevated follow keeps aim
  unambiguous.

### D4 — Camera: fixed elevated angle behind the ball, lerped follow

Position = ball + fixed offset (raised, pulled back along −Z), lerped each
frame; `setCamera3D(pos, ball)`. No player camera control — the mouse is the
aim device.

### D5 — Cup: sensor sphere + speed gate; velocity zero on capture

The cup is a `sensor: true` sphere body. `overlap` (or the ball's contacts)
detects entry; capture requires ball speed below a threshold (a fast ball
rolls over — the lip-out). On capture the ball's `velocity` is zeroed, a
sink pause plays, and the next hole loads. Stroke limit per hole (e.g. 2×par
+ 2) auto-advances to avoid softlocks.

### D6 — Look: one directional + one point light, procedural grass, blob shadow

Grass = `createImageData` checker/noise tile; tee, cup ring, and walls tinted
neon. Blob shadow (procedural radial, `facing: 'plane'`, small lift) under
the ball sells height on ramps. Clear color sky — no skybox asset (keeps the
sample font-only).

### D7 — State machine: ATTRACT → AIM → CHARGE → ROLLING → SUNK → SCORECARD

Attract strokes with scripted aim/charge (aim at the cup with an error term,
fire at ~70% meter). First mouse/key input takes over at the next AIM.
SCORECARD after hole 9 shows strokes vs par per hole; any input restarts.

### D8 — Resources: created per hole, destroyed on hole change

Static bodies/meshes per hole are `destroy()`ed when the hole advances
(dynamic-count native resources, ADR 0011/0012 discipline); the ball body and
fonts/texture persist for the whole round.

## Risks / Trade-offs

- [Raycast through cursor needs camera-consistent math] → the cursor → ray
  construction is derived from `setCamera3D`'s view parameters exactly once
  and unit-tested by aim-line sanity in play.
- [Tuned ball feel differs between desktop and web] → physics steps are
  engine-side and deterministic; only `dt` clamping is script-side.
- [Ramp triangle meshes jitter the rolling sphere] → keep ramps shallow and
  the iteration count at the showcase's proven setting.
