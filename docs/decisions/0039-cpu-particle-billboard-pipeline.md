# 0039 — CPU particles and the billboard/oriented-quad pipeline

Status: Accepted (2026-09, change `f11-particles-billboards`)

Supports: ADR 0015 (fixed-function consumer API; engine-owned canned shaders);
ADR 0019 (display-list value-snapshot state, handle-referenced resources);
ADR 0011/0012 (native-backed classes, GC discipline); ADR 0017 (CPU skinning
precedent for engine-owned CPU animation).

## Context

`vision.md` targets PS2-era graphics and already has skinned CPU animation
(ADR 0035), but no particle system, no world-space sprite/billboard, and no
batched 2D sprite draw. The display list has two record families — 2D quads
(no depth) and 3D meshes (depth test + write, lit) — and no depth-test/
no-write path. GPU compute is unavailable through the engine's Sokol surface,
and the four-target gate includes WASM and software-rasterized goldens.

## Decision

- **Particles are CPU-simulated engine state.** `src/render/particles.c` owns a
  fixed-capacity pool and the emitter/integration step; `ParticleSystem` is a
  native-backed class (ADR 0011) with deterministic `destroy()` and a GC
  backstop. A system retains its `Texture`/`RenderTarget` until destroyed
  (same mechanism as F4b material maps, ADR 0027). The engine advances live
  systems once per frame by `dt × speedScale` with an engine-owned RNG seeded
  deterministically at creation.
- **One oriented-quad basis serves billboards and particles.** At record time
  the engine derives a `(right, up, normal)` basis from the recorded 3D
  camera: `'view'` uses the camera basis, `'y'` pins up to world +Y, and
  `'plane'` uses a fixed normal. `drawBillboard` exposes only the camera-facing
  axes (`'view'`/`'y'`); the particle system also exposes `'plane'` for true
  world-oriented quads (e.g. a water sheet). Screen-space systems draw 2D
  quads through the existing quad path.
- **A depth-test/no-write pipeline variant is added, not a shader.** Billboard
  and particle quads render through the existing quad vertex layout and canned
  shader with `compare = LESS_EQUAL, write_enabled = false` (normal and
  RT-flipped winding). This is the first no-depth-write path and is the
  intended basis for future transparent materials.
- **`drawSprites` reuses quad records.** It is a C-side validate-then-record
  loop emitting ordinary `efx_quad_record`s; the existing run planner coalesces
  same-texture/same-blend sprites into one draw. No batched sprite record.
- **`drawParticles` records one batch per system**, expanded at playback into
  billboards/oriented planes (world) or 2D quads (screen); opaque geometry
  occludes particles and particles do not occlude one another. Alpha batches
  sort back-to-front **within the batch**; additive/subtractive batches are
  order-independent. A global cross-record transparency sort is out of scope.
- **Visitor-facing showcases accompany the golden.** One or more curated
  gallery samples demonstrate the render modes and blending, using textures
  generated with `createImageData` so no third-party assets are required.

## Consequences

- F11 adds records, a pipeline variant, a registry, and two bindings; existing
  quad/mesh playback, pipelines, and goldens are untouched (the new variant is
  used only by the new records).
- Particles are deterministic on a target, which makes the simulation
  unit-testable and the golden stable; GPU/compute simulation is a future
  backend behind the same API, not a v1 concern.
- Within-batch sorting handles a single particle system but not ordering
  against transparent meshes; a global transparency sort would need a new
  cross-record ordering rule and is deferred.
- The engine now owns a second CPU animation system; its pool and step cost
  scale with live particles and are bounded by a documented per-system cap.

## Rejected alternatives

- **GPU/compute particles** — no compute path in this Sokol surface, hard to
  make deterministic for goldens, and divergent across D3D11/Metal/WASM.
- **JS-side particle simulation** — per-particle JS/GC cost and duplicated
  engine state; the frame loop and display list are already engine-owned.
- **Reusing the lit mesh path for billboards** — particles are unlit and would
  need an emissive material per draw; the mesh path also writes depth.
- **A dedicated batched sprite record with a per-frame instance arena** — no
  draw-call benefit over the existing run planner; extra plumbing.
- **A global depth sort of all records** — would change opaque/quad playback
  order and regress existing goldens; only within-batch sorting ships.
- **A `drawQuad3D`/`drawSprite3D` rename** — "billboard" is the useful
  user-facing abstraction; the oriented-plane mode belongs to the particle
  system, which the requirement is actually about.
- **Fluent per-property setters (Löve-style methods)** — folded into one
  options object at creation and `set(opts)` at runtime, per house convention.
