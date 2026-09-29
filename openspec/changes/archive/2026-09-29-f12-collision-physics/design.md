# Design

## Context

See `proposal.md` — Why. The design is shaped by the current engine state and
constraints:

- `src/` is a single C11 core static library (`platform`, `runtime`, `api`,
  `player`, `render`) plus `main.c` (ADR 0003); the only C++ TUs are the
  `src/math` GLM wrapper behind a plain C API (ADR 0005). A new subsystem is
  expected to be C11 and, where C++ is unwanted, to avoid `src/math`.
- There is **no scene graph**. Rendering is immediate-mode into a re-orderable
  display list, and drawn meshes do not persist across frames. Collision
  therefore cannot derive from draw calls; it owns its own persistent set of
  colliders.
- Native-backed classes follow ADR 0011/0013: opaque handle, deterministic
  idempotent `destroy()`, GC-finalizer backstop, display-list retention.
- CPU simulation in the C core is the established precedent for determinism
  and identical four-target behavior (F7 skinning, ADR 0035; F11 particles,
  ADR 0039).
- The script-owned frame loop is `update`/`render` hooks (F1); there is no
  engine `_physics_process`. Physics stepping must fit that without changing
  the frame architecture.
- The active `f11-particles-billboards` change also modifies the shared
  `js-api` class-list requirement and the `feature-roadmap` requirements; this
  change's deltas are written against the current main specs and will need a
  rebase/sync if F11 archives first (see Risks).

## Goals / Non-Goals

**Goals**

- A dependency-free, deterministic C collision + linear-dynamics + character
  core that is **testable in isolation**: a standalone headless test binary
  with millisecond iteration, direct narrowphase tests, scenario tests,
  determinism tests, and a stress/fuzz net.
- A minimal `efx.physics` script surface that makes the common game tasks
  (walk a character, query the world, drop/push props, detect triggers) short
  and hard to misuse.
- Identical behavior, validation, and errors on both the desktop and web
  bindings.

**Non-Goals** (design-level boundaries beyond the proposal's scope)

- No solver for rotation/angular contact, joints, or stacking stability beyond
  simple linear resting.
- No engine-driven simulation loop, no fixed-timestep accumulator, no
  interpolation.
- No physics authoring format or glTF physics extension; collision geometry is
  created from `Mesh`/primitive data at script level.
- No broadphase beyond what static triangle meshes and a modest dynamic body
  count need.

## Decisions

### D1 — Bespoke pure-C module, no GLM, own math

`src/physics/` is C11 and depends only on libc. It carries its own
`vec3`/`quat`/`mat3` primitives rather than linking `src/math` (C++/GLM).

- **Why**: the explicit testability goal. Linking GLM forces the physics test
  binary to be C++ and to pull the math TU, which slows iteration and couples
  the subsystem to an unrelated wall. A dependency-free module builds by
  itself (`cc src/physics/*.c tests/physics/*.c`) with no CMake or display.
- **Alternatives**: (a) vendor Jolt — rejected in the proposal on size (~10×
  wasm), a C++17 island, a community C binding, and cross-runtime determinism;
  (b) Bullet/reactphysics3d — same C++ island, and their character controllers
  are weaker than what we need; (c) link `src/math`/GLM — rejected for test
  isolation; (d) plain C math shared with `src/math` — the wrapper still links
  GLM, so no gain.

### D2 — Single engine-owned world, script-owned step

One world owns all colliders; `efx.physics.step(dt)` is called by the script,
and the engine never advances the simulation itself.

- **Why**: matches the immediate-mode ethos, avoids introducing a fixed-step
  frame concept (no `_physics_process`), and keeps the engine loop untouched.
  Tests and replays get full control of stepping, which is exactly what makes
  determinism testable.
- **Alternatives**: engine-owned fixed-step accumulator — rejected by the user
  because it changes the frame architecture; multiple world resources —
  rejected in favor of the fixed-limit "one world" model.

### D3 — Module layout and data model

```
src/physics/
  efx_phys_vec.h      vec3/quat/mat3 (own, C)
  shape.h/.c          shape descriptors; bounds/aabb
  narrow.h/.c         overlap, ray, sweep, contact generation
  broadphase.h/.c     static-mesh BVH; body list + AABB pass
  solver.h/.c         contact manifold, sequential impulses (linear)
  character.h/.c      capsule move_and_slide
  world.h/.c          registry, ids, step pipeline, queries
  physics.h           the public C surface
```

- World: intrusive arrays of colliders keyed by a stable monotonically
  increasing id; free-list reuse for dynamic arrays; no fixed cap.
- Collider: kind (static/dynamic/sensor), shape descriptor, position, layer,
  mask; dynamic adds inverse mass, velocity, friction, restitution.
- Static triangle meshes own a BVH built once at creation; bodies keep an AABB
  used by the body pass.
- Contacts live per dynamic body for the `contacts` report and are also the
  solver input.

### D4 — Narrowphase: analytic primitive pairs + mesh via closest features

Support sphere, AABB, vertical capsule, and triangle, with the pairs actually
needed: sphere/AABB/capsule against triangle (mesh), sphere/AABB/capsule against
each other, and ray against all.

- **Why**: specialized tests are simpler and more robust than a general GJK/EPA
  and cover the chosen shape set. Capsule is handled as a segment plus radius,
  which reduces capsule-vs-X to segment-vs-X plus a radius offset.
- **Alternatives**: GJK/EPA general convex — rejected as over-general and
  harder to make deterministic/robust for the fixed shape set; OBB — out of
  scope (linear-only keeps boxes axis-aligned).

### D5 — Swept character movement via conservative advancement

The character capsule is swept by iteratively advancing to the earliest time
of impact and projecting the remaining motion onto the contact plane, up to
`maxSlides`. Impact time comes from closest-feature distance and a
conservative bound on closing speed, with a `safeMargin` epsilon and a small
bounce-off offset.

- **Why**: continuous motion avoids tunneling and the "stick in wall" artifacts
  of recovery-only controllers (the Bullet `btKinematicCharacterController`
  failure mode); projecting per-plane implements the slide.
- **Alternatives**: discrete move + penetration recovery — rejected as jittery
  and tunnel-prone; sphere-stack character — noted as the fallback if
  capsule-vs-mesh edge cases prove intractable (see Risks).
- **Classification/snap/step**: floor/wall/ceiling from the contact normal
  against `up` with `floorMaxAngle`; floor snap is a down cast of
  `floorSnapLength` when not moving upward; step-up is up/forward/down casts of
  `stepHeight`, run only when blocked and grounded.

### D6 — Linear-only sequential-impulse solver

Contacts are resolved by a bounded loop of sequential impulses along the normal
and by a clamped friction impulse; restitution is applied above a small
relative-velocity threshold; remaining penetration is corrected with a slop
(Baumgarte or split-impulse position pass). No angular terms.

- **Why**: linear-only removes inertia tensors, contact torque, and the
  tumbling instability that makes small solvers flaky; with axis-aligned boxes
  and vertical capsules a single normal+friction impulse per contact is
  sufficient for PS2-era gameplay. It is also the most testable solver shape.
- **Alternatives**: full angular impulse solver — deferred by the user;
  positional-only (PBD) — rejected because `applyImpulse`/restitution are part
  of the requested impulse model.

### D7 — Broadphase: per-mesh BVH + body AABB pass

Static triangle meshes own a BVH for ray/sweep/overlap; dynamic bodies are few,
so they are paired against the static BVH by their AABB and against each other
by a linear AABB pass. Sensors participate in the same passes.

- **Why**: levels are triangle meshes and need an acceleration structure;
  dynamic counts are small enough that a second structure is unwarranted.
- **Alternatives**: uniform grid/hash for everything — fine for tile-ish levels
  but weaker for arbitrary imported meshes; dynamic AABB tree — deferred until
  body counts justify it.

### D8 — Determinism and the cross-runtime harness

Ordering is stable (creation id), iteration is bounded, and there is no RNG or
wall clock. Because floating point can differ across platforms/engines, the
portable script harness SHALL assert on **rounded** positions/velocities rather
than raw bits, so the desktop-vs-web comparison is meaningful.

- **Why**: the F10-style cross-runtime compare otherwise fails on harmless FP
  divergence; the physics gate explicitly requires "identical results", which
  we interpret as identical to the harness's documented precision.
- **Alternatives**: exact bit equality — rejected as unachievable across
  engines; per-platform golden values — rejected as brittle.

### D9 — Binding and resource integration

`src/physics` is compiled into `efx_core`; `src/api/api.c` and the web bridge
each register `efx.physics.*` and the `Body`/`Character` classes with identical
validation, classified native-backed (ADR 0011/0013). `body.transform` is built
in the binding from the body position as a column-major translation matrix so
`drawMesh` can consume it directly.

- **Why**: one core, two thin bindings, parity by construction (ADR 0022).
- **Alternatives**: a JS convenience wrapper for bodies — unnecessary; the
  surface is already thin.

### D10 — Test architecture

`tests/physics/` builds `efx_physics_tests` (headless, C-only) with:
direct narrowphase cases and invariants; scenario tests (settle, slide, slope,
step, sensor, push); determinism/replay; and a fuzz/stress net (no NaN, no
tunneling past tolerance, bounded time). Portable scripts under
`tests/scripts/` cover the JS surface through both runtimes; a `smoke_*` case
runs under ctest and a `web_*` case under the Emscripten ctest + cross-runtime
compare. A debug-wireframe golden scene is optional and may be deferred.

- **Why**: the user's explicit goal is expressive unit tests for fast
  iteration; separating pure-C tests from script tests keeps the fast loop fast.

## Risks / Trade-offs

- **Capsule-vs-triangle sweep edge cases (vertex/edge contact)** → Mitigate
  with closest-feature reduction, conservative advancement, a contact epsilon,
  an explicit edge/vertex test matrix, and a reserved internal sphere-stack
  fallback if specific cases remain intractable. This is the single largest
  schedule/robustness risk and gets dedicated tests first.
- **Resting jitter / sinking with linear-only contacts** → Mitigate with a
  penetration slop, a restitution threshold, enough solver iterations, and a
  "bodies come to rest" scenario per shape/plane combination.
- **Cross-platform FP divergence** → Mitigate by rounding in the portable
  harness (D8); keep exact determinism as a per-platform unit-test property.
- **F11 archives first and rewrites the shared `js-api` class list and
  `feature-roadmap` requirements** → Mitigate by re-reading the main specs and
  re-syncing this change's MODIFIED blocks against the post-F11 text before
  archiving (the delta currently reflects the pre-F11 baseline).
- **Performance on large imported meshes** → Mitigate with a bounded BVH build
  and documented soft guidance; profiles recorded in the core tests.
- **Script-owned step with variable `dt`** → Mitigate by documenting a fixed
  recommended `dt`, clamping non-positive/absurd values, and testing solver
  stability across the documented range.
- **No scene graph means scripts re-register level colliders per scene** →
  Mitigate with `clear()` on scene change and a documented lifecycle recipe.

## Migration Plan

Additive: new module and namespace, no changes to existing APIs or rendering.
Land in order: (1) pure-C core + headless tests (fast iteration), (2) bindings
and classes, (3) portable script harness and cross-runtime compare, (4) docs
(`docs/js-api.md`, `gallery/src/api/efx.d.ts`, `AGENTS.md`, ADR 0040), (5)
optional gallery sample. Rollback is deleting the additive module/namespace;
nothing existing depends on it.

## Open Questions

- Exact restitution threshold, penetration slop, and default iteration count —
  tunable constants to settle during apply without changing the specs.
- Contact manifold point count per pair (single point per feature vs a clipped
  4-point box face) — settle against stability tests; spec only requires
  correct reporting, not a point count.
- BVH leaf size and build budget for large meshes — settle by profiling.
- Whether the debug-wireframe golden scene ships in this change or a follow-up
  gallery/test tweak — spec does not require a golden image.
