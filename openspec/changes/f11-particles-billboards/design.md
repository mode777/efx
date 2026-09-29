# Design

## Context

See `proposal.md` — Why. Current state that shapes this design:

- Rendering is a per-frame display list played back in record order
  (`src/render/render.c`). There are two record families: 2D quads (affine to
  the 2D frame, no depth) and 3D meshes (MVP from the recorded 3D camera,
  depth test + write, lit Phong).
- Consecutive 2D quad records sharing texture and blend collapse into one draw
  call (`efx_render_runs`); mesh records break runs.
- The 2D quad pipeline is `compare = ALWAYS, write = false`; the mesh pipeline
  is `LESS_EQUAL, write = true`. There is no depth-test/no-write variant.
- The 3D camera is record-snapshotted; the vertex layout is shared
  (`pos, color, normal, uv`) and the canned-shader model forbids
  script-visible shaders (ADR 0015).
- F7 already simulates skinning on the CPU in C, deterministically, as the
  precedent for engine-owned CPU animation.

## Goals / Non-Goals

**Goals:**

- A world-space billboard primitive (`drawBillboard`) oriented by the engine
  from the recorded 3D camera, with `'view'`/`'y'` facing and a
  depth-test/no-write policy reusable by later transparent materials.
- A CPU, engine-owned, fully configurable particle system whose quads are
  drawn as `'view'`/`'y'` billboards or as fixed `'plane'` 3D quads, in 3D
  world space (or 2D screen space).
- Batched 2D sprite drawing (`drawSprites`) that reuses the existing quad
  records and run planner.
- Deterministic simulation for unit tests and goldens; no new dependencies.

**Non-Goals:**

- GPU/compute simulation, mesh particles, velocity-stretched/ribbon trails,
  collision, sub-emitters, soft particles.
- A general cross-record transparency sort for arbitrary meshes; only
  within-batch particle sorting ships.
- Per-particle render-mode switching within one system (the mode is a
  system-level property).

## Decisions

### D1 — CPU simulation in the C core

The particle pool, emitter, and integration live in `src/render/` as pure C,
driven by the engine frame step. A `ParticleSystem` is an engine-owned pool of
fixed-capacity slots.

- **Why**: matches F7's CPU-skinning precedent; deterministic (the unit/golden
  gate needs reproducible stepping); identical on all four targets including
  WASM; needs no compute support, float render targets, or per-particle GPU
  state; keeps one canned-shader set.
- **Alternatives rejected**: GPU/compute simulation (sokol exposes no compute
  path here; D3D11/Metal/WASM divergence; far harder to debug and to make
  deterministic for goldens); JS-side simulation in the prelude (per-particle
  JS/GC cost, and the engine already owns the loop and the display list).

### D2 — One oriented-quad render path on the 3D camera; a depth-test/no-write pipeline

Introduce a billboard/oriented-quad record whose vertices are built from a
basis `(right, up, normal)` derived at record time from the recorded 3D camera
and, for `'plane'`, a fixed normal:

- `'view'`: `right/up` = camera right/up (square to the view).
- `'y'`: `up` = world `+Y`, `right` = normalize(cross(worldUp, viewDir)).
- `'plane'`: a fixed orthonormal basis around the configured `normal`.

The GPU needs a quad-vertex pipeline variant with `compare = LESS_EQUAL,
write = false`, in both the normal and the RT y-flipped (winding-compensated)
forms, mirroring the existing mesh pipeline variants. The billboard record
carries the camera transform and a `depthTest` flag.

- **Why**: one basis routine serves `drawBillboard` and all particle render
  modes; the no-write policy is exactly what transparent materials will need
  later, so the architecture is laid once.
- **Alternatives rejected**: reusing the 2D quad pipeline (no depth, so
  particles would never be occluded); reusing the lit mesh path (particles are
  unlit and would need an emissive material per draw); adding a new
  script-visible shader (forbidden).

### D3 — `drawSprites` is a C loop over existing quad records

`drawSprites(texture, sprites)` validates the array, then records one
`efx_quad_record` per entry, exactly as `drawQuad` would, with validation
performed before any record is pushed so a bad entry records nothing.

- **Why**: same-texture/same-blend quads already coalesce into one draw in
  `efx_render_runs`, so no new record type or run-planner change is needed;
  the only wins over a JS loop are avoiding per-sprite JS→C overhead and
  offering an array-shaped API.
- **Alternatives rejected**: a dedicated batched sprite record with a
  per-frame instance arena (extra plumbing for no draw-call benefit);
  doing nothing (the gallery already shows the JS-loop cost).

### D4 — A particle batch record expanded at playback, with within-batch sorting

`drawParticles(sys)` records one particle-batch record carrying the system
handle, the camera snapshot, and blend. At playback the engine iterates the
live particles and emits their vertices directly, so one system is one draw.
For a world-space system it uses the billboard/oriented-quad basis; for a
screen-space system it emits 2D quads. Alpha batches sort particles
back-to-front by view distance within the batch; additive/subtractive batches
skip the sort.

- **Why**: avoids one record per particle and keeps particles contiguous for a
  single draw; sorting within the batch is correct for a single system and
  avoids disturbing the F2/F3 record-order contract (a global depth sort would
  change existing opaque/quad playback and goldens).
- **Alternatives rejected**: recording N billboard records per frame (fine for
  a few hundred but wasteful for the "main prize" scale); a global stable
  depth sort of all records (out of scope, high regression risk).

### D5 — `facing` belongs to the system; `drawBillboard` stays a billboard

`drawBillboard` exposes only camera-facing axes (`'view'`, `'y'`); the
`'plane'` mode is a property of the particle system, where "not always a
billboard" is the requirement. A water sheet is a world-space system with
`facing: 'plane', normal: [0,1,0]`.

- **Why**: keeps the primitive's name honest while still covering the
  water-plane use case; the two share the D2 basis routine so there is no
  duplicated renderer.
- **Alternatives rejected**: renaming the primitive to a generic `drawQuad3D`
  (the billboard-ness is the useful user-facing abstraction); exposing
  `'plane'` on `drawBillboard` (mixes concerns).

### D6 — `ParticleSystem` is a native-backed class; configurations are plain values

The class holds the engine pool and a snapshotted configuration; `destroy()` is
deterministic with a GC backstop (ADR 0011/0012). The system retains its
texture until destruction, mirroring F4b material-map retention. `set(opts)`
validates the whole partial object before applying anything.

- **Why**: the native-backed class shape is the established memory discipline;
  retaining the texture prevents a use-after-free when a script destroys a
  texture still drawn by a live system; atomic `set` avoids half-applied state.
- **Alternatives rejected**: a JS-managed system (the pool must be native);
  a slot bank of fixed systems (the count is not fixed by design).

### D7 — Engine auto-update with a manual override; deterministic RNG

Live systems are advanced once per frame by the engine with the frame `dt`
scaled by `speedScale`. `pause`/`stop`/`reset`/`emit` give control without an
explicit per-frame `update` call. An engine-owned RNG, seeded deterministically
at creation, supplies per-particle randomness.

- **Why**: the engine already owns the frame loop and the requirement is
  engine-owned-but-tweakable; matches the Godot model the brief endorsed. A
  deterministic seed makes unit tests exact and goldens stable.
- **Alternatives rejected**: an explicit Löve-style `sys.update(dt)` as the
  only advance path (more calls; easy to forget; the frame loop already has
  `dt`).

### D8 — Options-object vocabulary

All Löve-style setters are folded into one `opts` object at creation and into
`set(opts)` at runtime; there are no chainable `setX` methods. The vocabulary
mirrors Löve (lifetime, emission rate/lifetime, position, direction/spread,
speed, linear/radial/tangential acceleration, damping, sizes/colors arrays,
rotation/spin, insert mode, relative rotation) plus the 3D additions (space,
3D vectors and emission shapes, `facing`/`normal`, gravity, `blend`).

- **Why**: house convention (`configuration beyond ~3 values goes in a
  trailing option object`), and the brief explicitly asked to fold the Löve
  API into an options object.
- **Alternatives rejected**: a fluent setter API (un-idiomatic for this
  engine); separate `setX` methods (large surface, harder to snapshot
  atomically).

### D9 — Emission/force scope for v1

Emission shapes: `'point'`, `'box'`, `'sphere'`, `'sphereSurface'`, `'disc'`.
Forces: gravity, linear/radial/tangential acceleration, linear damping.
Display-over-lifetime: size and color arrays (`1..8` samples), initial
rotation and spin, optional flipbook quads, `relativeRotation`.

- **Why**: covers the canonical PS2 effects (fire, smoke, sparks, explosions,
  water sheets) with the Löve-style lifetime interpolation the brief endorsed;
  keeps the vocabulary bounded.
- **Alternatives rejected**: Godot-style min/max/curve for every property (a
  curve type/editor is a much larger surface); collision/trails/sub-emitters
  (proposal non-goals).

### D10 — Determinism, goldens, and the 5-attempt cap

Simulation unit tests step fixed `dt` sequences and compare against a CPU
reference. The golden scene uses a fixed capture frame with a deterministic
setup. Per the change brief, the golden is attempted with a hard cap of **5
capture/debug attempts**; if it still fails, the agent stops and reports back
rather than iterating further.

- **Why**: the milestone gate needs a golden (rendering milestone), but
  time-based effects can be golden-hostile; the cap bounds the risk.
- **Alternatives rejected**: skipping the golden (the roadmap gate requires
  one for rendering milestones); an unbounded golden chase.

### D11 — Visitor-facing showcases over the golden

A golden proves pixels; it does not show a visitor what the feature *does*.
The change therefore also ships one or more curated gallery showcases — one
world-space effects demo (`'view'`/`'y'` billboards, additive + alpha blends,
burst + continuous, lifetime size/color) and one oriented-plane demo
(`facing: 'plane'`, the water-sheet case) — with textures generated
procedurally via `createImageData` so no third-party assets or `CREDITS.md`
change are needed.

- **Why**: the gallery is the public face of the engine and already has a
  showcase per feature (text, modules, input, assets); particles are the most
  visual feature and deserve one. Procedural textures keep the change
  self-contained and portable.
- **Alternatives rejected**: golden-only (no visitor-facing demonstration);
  shipping a CC0 sprite atlas (extra provenance/asset-pack work with no
  feature gain, though the capability still permits it).

## Risks / Trade-offs

- **[CPU particle cost on the software renderer / WASM]** → cap the pool
  (65536 hard, small defaults), emit one draw per system, and keep the golden
  scene's particle count modest; GPU is a future backend behind the same API.
- **[Depth-test/no-write changes could disturb existing goldens]** → the new
  pipeline variant is used only by the new record types; existing quad/mesh
  playback and their pipelines are untouched.
- **[Within-batch sorting is not global transparency sorting]** → documented
  limitation; particles sort among themselves but not against transparent
  meshes. A global sort is deferred.
- **[Cross-platform float/RNG divergence]** → the simulation is deterministic
  on a given target; the cross-runtime script test uses coarse assertions
  (counts, monotonic motion), and the golden is captured per platform.
- **[Golden flakiness]** → bounded to 5 attempts per the brief; a failure is
  reported, not chased.
- **[`drawSprites` records N quads, not a compact batch]** → acceptable
  because the run planner already coalesces the draw; revisit only if
  record-storage profiling demands it.

## Migration Plan

Additive. No script, spec, or golden behavior changes for existing features;
rollback is deleting the change's new code paths. The new pipeline variant and
record types are inert unless the new API is called.

## Open Questions

None blocking. The particle hard cap (65536) and the default `normal`
(`[0,1,0]`) are settled in the specs.
