# 0040 — A bespoke pure-C collision, character, and linear-dynamics core

Status: Accepted (2026-09, change `f12-collision-physics`)

Supports: ADR 0001 (C11 core with a C ABI); ADR 0003 (module walls);
ADR 0005 (GLM behind a plain C API — deliberately *not* used here);
ADR 0011/0012 (native-backed classes, GC discipline); ADR 0015 (fixed-function
consumer API); ADR 0022 (web bridge shares one core); ADR 0035 (CPU skinning
precedent for engine-owned CPU simulation).

## Context

EmotionFX can render a 3D world but has no collision, no character movement,
and no dynamics, so the default PS2-era shape — a character walking a level,
props that fall and get pushed, raycasts for picking and line of sight — is
impossible to author. A general rigid-body engine (Jolt, Bullet) is both
off-identity and too heavy for a four-target, small-binary gate (~1.9 MB of
wasm against a ~194 KB player) and would drag a C++17 island and a community
binding into a C11 codebase. The engine already has CPU simulation precedents
(F7 skinning, ADR 0035; F11 particles, ADR 0039) and a script-owned frame loop
(`update`/`render` hooks, ADR 0016) with no engine `_physics_process`.

## Decision

- **A bespoke, dependency-free C11 core (`src/physics/`).** Sphere, axis-
  aligned box, vertical capsule, and triangle-mesh colliders; a median-split
  BVH per static mesh; an impulse-based **linear** solver; sensors; and a
  kinematic capsule character. It links only libc and carries its own tiny
  `vec3`/`quat`/`mat3` math — **no renderer, no platform layer, no script
  runtime, and no `src/math`/GLM** — so `efx_physics_tests` builds and runs
  headless in milliseconds, independent of a display or the engine.
- **One engine-owned world, stepped by the script.** A single world owns all
  colliders; `efx.physics.step(dt)` advances it and the engine never does.
  There is no fixed-step accumulator, no interpolation, and no engine-owned
  simulation loop. This keeps the immediate-mode frame architecture untouched
  and makes tests and replays fully deterministic: identical inputs and step
  sequences produce identical results (no wall clock, RNG, or threads), and
  two worlds stepped interleaved behave as if alone.
- **Linear-only impulses.** Dynamic bodies translate; they never rotate, so
  boxes stay axis-aligned and capsules stay vertical. Contacts are resolved
  with a bounded number of sequential normal + clamped-friction impulses; the
  restitution target is computed once from the pre-solve approach velocity
  (recomputing it per iteration would cancel the bounce); penetration is
  removed with a slop by a positional pass, with no energy-adding bias. This
  trades tumbling and stacking stability for the small, robust, testable
  solver a PS2-era game needs.
- **Narrowphase by closest features, sweeps by conservative advancement.**
  Specialized analytic pairs plus sphere/AABB/capsule-vs-triangle via
  closest-feature reduction; shape casts and character movement sweep by
  advancing to a conservative bound on the closing distance, so fast, thin, and
  corner impacts do not tunnel.
- **One-way kinematic push.** During `step` a character is an immovable
  (infinite-mass) collider, so a dynamic body in contact is pushed away from it
  (using the character's script-set `velocity`); `moveAndSlide` is never
  blocked by dynamic bodies or sensors, so characters push through in their
  direction of travel. Two-way character/dynamic interaction is out of scope.
- **Sensors are a flag, not a kind.** A `sensor: true` collider participates in
  the broadphase and is reported (`overlap`, and flagged in `contacts`) but
  never resolves, never blocks `moveAndSlide`, and never receives an impulse;
  sensors are excluded from `raycast` unless requested.
- **Determinism is asserted at a documented precision.** Exact bit equality is
  not achievable across engines, so the portable harness asserts on rounded
  positions/velocities rather than raw bits; per-platform exactness remains a
  C unit-test property.
- **One core, two thin bindings.** `src/physics` compiles into `efx_core`;
  `src/api` (quickjs, desktop) and `src/web` (the page engine, ADR 0022) each
  register `efx.physics` and the native-backed `Body`/`Character` classes with
  identical validation, errors, and semantics.

## Consequences

- The physics surface is deliberately small and engine-shaped: no angular
  dynamics, joints, CCD for fast dynamics, sleeping/islands, collision
  callbacks (scripts poll `body.contacts`), convex hulls/OBBs, 2D physics,
  GPU/multithreaded simulation, or script-visible collider debug draw.
- `src/physics` stays GLM-free on purpose; a future change that wants to share
  math must move the wrapper, not link GLM into the physics test binary.
- The static-mesh collider reads a live `Mesh`'s retained CPU bind copy, so the
  renderer keeps that copy for every mesh (tiny CPU cost) to feed collision.
- Physics has no fixed body cap (dynamic allocation with soft guidance), so it
  is not in the fixed-limits table; the existing fixed limits (4+1 lights,
  1 camera, 16 surfaces, 8 post effects, 65536 particles, 4096 target) are
  unchanged.
