# Proposal

## Why

EmotionFX can draw a 3D world but cannot tell whether anything touches
anything else: there is no collision detection, no way to move a character
against geometry, and no dynamics for props. The default shape of a PS2-era
title — a third-person or first-person character on a level — needs a capsule
that slides along walls, climbs small steps, stands on slopes, and snaps to
floors, plus raycasts for picking and line-of-sight and simple falling /
pushable crates. Godot's `move_and_slide`, Unity's `CharacterController`, and
LÖVE's bump.lua all establish that ergonomic target; raylib and MonoGame show
the opposite cautionary tale, shipping shape tests but no world, so every user
hand-rolls the hard part.

A general physics engine is both off-identity (PS2-era games used kinematic
characters and simple impulses, not rigid-body stacks) and too heavy for this
project's four-target, small-binary gate: Jolt alone is ~1.9 MB of wasm against
our current ~194 KB player. This change therefore delivers a **bespoke,
pure-C11, impulse-based** collision and character system, owned by the C core
and exposed through a minimal `efx.physics` surface.

## What Changes

- **New pure-C physics core (`src/physics/`)** with zero engine, platform, or
  GLM dependencies (its own tiny vec3/quat math), so it compiles and is unit-
  tested in isolation in milliseconds. It contains: analytic shapes (sphere,
  AABB, vertical capsule) plus triangle-mesh colliders with a BVH broadphase;
  a deterministic single world; an impulse-based **linear** solver (no
  rotation); sensors; and the kinematic capsule character controller.
- **New `efx.physics.*` script namespace** — world configuration
  (`gravity`, `iterations`), `clear()`, script-owned `step(dt)`,
  `createBody`, `createCharacter`, `createStaticMesh`, and the queries
  `raycast`, `overlap`, `shapeCast`. Bodies and characters are native-backed
  classes with `destroy()` and a GC finalizer backstop.
- **Impulse dynamics** — dynamic bodies with mass, velocity, friction, and
  restitution; gravity, `applyImpulse`/`applyForce`; per-body `layer`/`mask`;
  penetration correction with slop; a fixed number of sequential-impulse
  iterations. Rotation is out of scope, so dynamic boxes stay axis-aligned and
  capsules stay vertical.
- **Kinematic character controller** — a vertical capsule with
  `moveAndSlide(motion)`: swept motion, iterative slide, floor/wall/ceiling
  classification (`floorMaxAngle`), floor snapping (`floorSnapLength`),
  step-up (`stepHeight`), `maxSlides`, and `safeMargin`.
  Characters push dynamic bodies one-way (they are immovable colliders during
  `step`) but are never blocked by them in `moveAndSlide`.
- **Sensors** — `sensor: true` colliders that never resolve; they are reported
  by `overlap` and in `contacts` (flagged) and are excluded from `raycast`
  unless requested.
- **Deterministic, expressive tests** — the change's explicit goal is fast,
  isolated iteration: direct narrowphase unit tests, invariant tests,
  scenario/settle tests, determinism tests, and a fuzz/stress net, all
  headless (`tests/physics/`), plus a portable script harness on all four
  targets. The physics core MUST remain dependency-free so its test binary
  builds without a display or the engine.
- **Documentation** — `docs/js-api.md` and `gallery/src/api/efx.d.ts` gain the
  `efx.physics` entries and the `Body`/`Character` classes; a new ADR records
  the durable decisions; the roadmap gains F12.
- **No breaking changes.** Existing rendering, scripting, input, and goldens
  are untouched.

## Capabilities

### New Capabilities

- `collision-physics`: the collision world — shape set (sphere, AABB, vertical
  capsule, triangle mesh), broadphase and narrowphase, static/dynamic/sensor
  bodies, layers/masks, the deterministic linear impulse solver, `step`, the
  per-body contact list, and the zero-dependency, isolated-unit-test contract.
- `physics-queries`: spatial queries — `raycast`, `overlap`, and `shapeCast`
  against the shared world, usable with or without dynamic bodies.
- `character-controller`: the kinematic capsule `moveAndSlide` — swept slide,
  floor/wall/ceiling classification, slope limit, floor snap, step-up, safe
  margin, and one-way push of dynamic bodies.

### Modified Capabilities

- `js-api`: adds the `efx.physics` namespace and its functions, classifies
  `Body` and `Character` as native-backed classes (extending the exact class
  list), and updates `docs/js-api.md` and the gallery type document in the
  same change.
- `feature-roadmap`: declares **F12 (collision + character + impulse
  dynamics)** as a new orthogonal milestone (predecessors F3 and F6a/F6b) with
  its scope and verification gate.
- `web-gallery`: adds the requirement that the curated catalog includes a
  physics/character showcase sample demonstrating movement, collision,
  triggers, and props.

## Impact

- **Core (`src/physics/`)**: new module — `vec` (own math), `shape`,
  `narrow`, `broadphase`, `solver`, `character`, `world`.
- **Bindings (`src/api/` desktop, `src/web/` bridge)**: the `efx.physics`
  namespace, the `Body` and `Character` classes, and identical validation and
  errors on both bindings.
- **Docs**: `docs/js-api.md`, `gallery/src/api/efx.d.ts`, `AGENTS.md`, and a
  new ADR `docs/decisions/0040-physics-core.md` (durable decisions: bespoke
  pure-C core with no GLM; linear-only impulse solver; script-owned step;
  single world; one-way kinematic push; sensors; determinism/ordering).
- **Tests**: a headless `efx_physics_tests` suite (narrowphase, invariants,
  settle scenarios, determinism, fuzz), a portable script harness on the four
  targets, and an optional debug-wireframe scene.
- **Gallery content**: one curated physics/character showcase sample, using
  only the public API and procedurally generated geometry/textures (no new
  third-party assets).
- **Dependencies**: none new. Jolt Physics is recorded as the evaluated
  full-engine alternative and deliberately not vendored (size, C++17 island,
  cross-runtime determinism). No vendoring or dependency evaluation is
  required because no third-party code is added.

## Non-goals

- **Angular dynamics / rotation** — dynamic bodies translate only; no inertia
  tensors, torque, or tumbling.
- **Continuous collision detection (CCD)** for fast dynamic bodies — raycasts
  are the bullet mechanism.
- **Sleeping / island management**.
- **Collision event callbacks** — scripts poll `body.contacts`.
- **Two-way character ↔ dynamic interaction** — one-way push only.
- **Convex hull / OBB / cylinder / arbitrary convex shapes**.
- **2D physics** — the system is 3D-only in this milestone.
- **Full rigid-body constraints** — joints, stacking solvers, soft bodies,
  ragdolls, vehicles.
- **GPU or multi-threaded simulation** — CPU, single-threaded, deterministic.
- **A general debug rendering of colliders** — debug draw is deferred (an
  optional test-only wireframe scene may land with the goldens).

## Roadmap position

This change implements **F12**, a new **orthogonal** milestone. Its only
predecessors are F3 (3D camera, math, and meshes) and F6a/F6b (resource loading
and glTF import, for collision meshes loaded from the resource root). It does
not depend on F4/F5/F7/F8 and may land independently of them. An ADR is
required (new `docs/decisions/0040-physics-core.md`): the pure-C/no-GLM module
wall, the linear-only impulse model, script-owned stepping, the single-world
ownership model, and one-way kinematic pushing are durable cross-cutting
decisions future changes must respect.
