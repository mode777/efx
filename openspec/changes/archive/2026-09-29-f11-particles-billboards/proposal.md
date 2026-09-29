# Proposal

## Why

EmotionFX targets PS2-era graphics but has no particle system, no world-space
sprite/billboard, and no batched 2D sprite draw. Particles are the single most
visible "oldschool" effect (fire, smoke, sparks, magic, explosions) and the
existing gallery demo has to fake them in screen space with per-particle
`drawQuad`. A CPU particle system plus the billboard primitive it needs makes
that class of effect a first-class, engine-owned feature and unlocks
independently useful sprites (impostors, 3D markers, health bars) and 2D
sprite/tile batching.

This is delivered as a new **orthogonal milestone F11** (predecessors F2 and
F3; F6a for textures loaded from the resource root). It does not depend on
F4/F5/F7/F8 and may land immediately.

## What Changes

- **New primitive `efx.drawBillboard(pos, opts)`** `[C]` — one textured quad
  placed at a world position and auto-faced by the engine using the recorded
  3D camera. A billboard stays a billboard, with a configurable facing
  **axis**: `'view'` (full camera-facing, default) or `'y'` (up pinned to
  world +Y, yaw toward the camera). Options: `size`, `color` tint,
  `sourceRect` (atlas frames), `rotation`, and `depthTest` (default true).
  Depth-tested but **depth-write disabled** so overlapping draws do not
  occlude each other. Scripts never read or track the camera.
- **New primitive `efx.drawSprites(texture, sprites)` `[C]`** — batched **2D**
  sprite drawing (never 3D): the current `drawQuad` option set over an array.
  Implemented as a C-side loop that records ordinary quad records, so
  consecutive same-texture/same-blend sprites collapse into one draw through
  the existing run planner. Serves 2D particle systems and tile/UI workloads.
- **New native-backed resource `ParticleSystem` `[C]`** —
  `efx.createParticleSystem(opts)` returns a CPU-simulated, engine-owned,
  fully tweakable emitter; `sys.emit(n)` for bursts;
  `sys.start()/stop()/pause()/reset()`; `sys.set(opts)` for runtime
  reconfiguration (value-snapshotted); `sys.speedScale`; read-only `sys.count`;
  `sys.destroy()` with GC finalizer backstop. Particles are simulated in true
  **3D world space** by default (`space: 'world'`: 3D position, direction +
  spread cone, 3D gravity/acceleration, 3D emission shapes) or in 2D screen
  space (`space: 'screen'`).
- **A particle system picks how its quads are drawn** — a per-system render
  **`facing`**: `'view'` (full camera-facing billboard, default), `'y'`
  (vertical-axis billboard), or `'plane'` (**true 3D planes**: a fixed world
  orientation given by a `normal`, e.g. `[0,1,0]` for waves on a water
  plane). A `plane` system simulates and moves its particles in world space
  but never turns them toward the camera. Rendering is
  `efx.drawParticles(sys)`, which records one batch and expands the live
  particles at playback (billboards/oriented planes for world space, 2D quads
  for screen space).
- **Simulation is CPU, in the C core** — matches the F7 CPU-skinning
  precedent, stays deterministic for unit/golden tests, needs no compute or
  render-to-float-texture support, and behaves identically on all four targets
  including WASM. The single canned-shader model (ADR 0015) is unchanged.
- **Depth policy groundwork** — billboards introduce the engine's first
  depth-test/no-write path; alpha particles are sorted back-to-front **within
  their own batch** (additive particles need no sort). A general
  transparent-material sort across records is architected but not built here.
- **Particle showcase samples** — in addition to the golden scene, one or
  more curated gallery samples demonstrate the particle features to visitors:
  world-space effects (camera-facing `'view'`/`'y'` billboards with additive
  and alpha blending, bursts plus continuous emission, lifetime-interpolated
  size/color, atlas frames) and fixed `'plane'` oriented quads (e.g. a water
  surface). Samples use only the public API and prefer procedurally generated
  textures (`createImageData`) so no third-party assets are required.
- **Documentation** — `docs/js-api.md` and `gallery/src/api/efx.d.ts` gain the
  three new entries and the `ParticleSystem` class; the roadmap is extended
  with F11 in `openspec/specs/feature-roadmap` and the AGENTS.md roadmap table.
- **No breaking changes.** Existing 2D/3D drawing, tests, and goldens are
  untouched.

### Non-goals

- **GPU/compute particles** — deferred; the CPU system is the v1 and the API
  leaves room for a GPU backend later.
- **Particle collision, physics, sub-emitters, trails/ribbons, soft particles,
  mesh particles, and velocity-stretched billboards.**
- **A curve/gradient editor** (Godot-style curves) — v1 uses Löve-style
  lifetime-interpolated arrays for size/color (bounded count).
- **Global transparency sorting across arbitrary records** (transparent
  materials) — only within-batch particle sorting lands now.
- **New shaders on the consumer API** — never; any billboard orientation is
  engine-owned.

## Capabilities

### New Capabilities

- `billboards`: world-space, camera-facing textured quad drawing
  (`drawBillboard`) — `'view'` and `'y'` facing axes, size/rotation/tint/atlas
  options, engine auto-facing, and the depth-test/no-write render policy.
- `particles`: engine-owned CPU particle systems — 3D world-space (and 2D
  screen-space) emission and simulation, a per-system quad render mode
  (`facing`: `'view'` / `'y'` / `'plane'` with an orientation `normal`),
  configuration (emission, forces, lifetime-interpolated size/color,
  rotation/spin, flipbook quads, insert mode), lifecycle and burst emission,
  runtime reconfiguration, and batched rendering through `drawParticles(sys)`.

### Modified Capabilities

- `2d-layer`: adds the batched `drawSprites(texture, sprites)` 2D draw
  requirement on top of the existing display-list/quad contract.
- `js-api`: adds the `drawBillboard`, `drawSprites`, `createParticleSystem`,
  and `drawParticles` entries; registers `ParticleSystem` as a native-backed
  class (dynamic-count, `destroy()`, GC backstop); and updates the normative
  reference and gallery type document.
- `feature-roadmap`: declares **F11 (particles + billboards)** as a new
  orthogonal milestone with predecessors F2 + F3 (and F6a for file textures),
  its scope, and its verification gate.
- `web-gallery`: adds the requirement that the curated catalog includes
  particle showcase samples that exercise the milestone's render modes and
  blending for visitors.

## Impact

- **Core (`src/render/`)**: new billboard and particle-batch record types and
  a particle-simulation module (pool, emitter, lifetime interpolation); a
  depth sort key for batch playback; the billboard vertex build (camera basis
  from the recorded 3D camera).
- **Platform (`src/platform/pipeline.c`)**: a quad-vertex billboard pipeline
  variant with depth test and no depth write (both GG and flipped RT winding),
  and playback expansion of the particle batch.
- **Bindings (`src/api/` desktop, `src/web/` bridge)**: the four new functions
  and the `ParticleSystem` class, identical on both bindings.
- **Docs**: `docs/js-api.md`, `gallery/src/api/efx.d.ts`, `AGENTS.md`, and a
  **new ADR `docs/decisions/0039-cpu-particle-billboard-pipeline.md`** (durable
  decisions: CPU simulation ownership, the depth-test/no-write policy,
  within-batch sorting, and the billboard primitive living on the 3D camera).
- **Tests**: headless simulation unit tests (deterministic stepping against a
  CPU reference), a portable script smoke case on all four targets, and a
  golden particle/billboard scene (capped at 5 capture/debug attempts per the
  change brief).
- **Gallery content**: one or more curated showcase samples under
  `gallery/samples/curated/` with manifest entries, covered by the gallery
  smoke; no new third-party assets (textures generated with `createImageData`)
  unless a CC0 pack is chosen.
- **Dependencies**: none new — textures come from the existing `createTexture`
  / resource layer; the pool, RNG, and math already exist in-core.
