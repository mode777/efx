# Tasks

## 1. Pure-C core scaffolding and math

- [ ] 1.1 Create `src/physics/` with `efx_phys_vec.h` (own `vec3`/`quat`/`mat3`, C11, no GLM) and verify it compiles standalone with `cc -std=c11 -Wall -Wextra -Werror -c`
- [ ] 1.2 Add a standalone `efx_physics_tests` CMake target that links only `src/physics/*.c` and `tests/physics/*.c` (no renderer/platform/quickjs), build with `-DEFX_HEADLESS=ON`, and verify it runs with an empty test main
- [ ] 1.3 Add `tests/physics/` assertion/scenario helpers (world builder, `step_n`, vec3-near, contact lookup) and verify a trivial self-test passes

## 2. Shapes and narrowphase

- [ ] 2.1 Implement shape descriptors (sphere, AABB, vertical capsule as segment+radius, triangle) with AABB/bounds and verify descriptor unit tests
- [ ] 2.2 Implement sphere/AABB/capsule vs triangle overlap with closest-feature reduction and verify direct narrowphase tests for face, edge, and vertex cases
- [ ] 2.3 Implement sphere/sphere, sphere/AABB, sphere/capsule, AABB/AABB, and capsule/capsule overlap with normal+depth and verify pairwise tests including symmetric-normal invariants
- [ ] 2.4 Implement ray vs sphere/AABB/capsule/triangle with nearest-hit and verify distance/normal accuracy tests
- [ ] 2.5 Implement swept sphere and swept capsule (conservative advancement) vs triangle and against the analytic shapes, and verify no-tunneling tests for fast, thin, and corner impacts

## 3. World, registry, and broadphase

- [ ] 3.1 Implement the world registry: stable ids, dynamic arrays with free-list reuse, `clear`, add/remove, and verify create/remove/clear/id-reuse tests
- [ ] 3.2 Implement the static triangle-mesh BVH (build + query) over a `Mesh`'s triangles and verify build correctness and query-vs-brute-force equivalence on random meshes
- [ ] 3.3 Implement the dynamic body AABB pass pairing dynamic-vs-static (BVH) and dynamic-vs-dynamic (linear) respecting layer/mask, and verify filtering and determinism (stable order) tests
- [ ] 3.4 Add determinism tests: replay a scene/step sequence twice and assert identical positions and contacts; interleave two worlds and assert independence

## 4. Linear impulse dynamics

- [ ] 4.1 Implement body integration (gravity, accumulated force, velocity, position) with no rotation, and verify free-fall and `applyImpulse`/`applyForce` tests
- [ ] 4.2 Implement contact generation into per-body contact lists (normal, point, depth) and verify manifold/normal tests per shape pair
- [ ] 4.3 Implement the sequential-impulse solver (normal impulse, clamped friction, restitution threshold) and verify restitution/friction response tests
- [ ] 4.4 Implement penetration correction with slop (Baumgarte or split impulse) and verify resting/stack-free settle tests (ball and box on a plane do not sink or jitter beyond tolerance)
- [ ] 4.5 Implement `step(dt)` wiring the pipeline (integrate → broadphase → generate → solve → correct), clamp non-positive/absurd `dt`, and verify that nothing moves without a step and that results are stable across the documented `dt` range
- [ ] 4.6 Add a fuzz/stress test (hundreds of random bodies, fixed steps) asserting no NaN, no tunneling past tolerance, and bounded runtime

## 5. Sensors and contact reporting

- [ ] 5.1 Implement `sensor: true` colliders (no resolution, no impulse, present in the broadphase) and verify a dynamic body passes through while reporting the sensor
- [ ] 5.2 Expose the read-only per-body `contacts` list with `{ body, sensor, normal, point, depth, impulse }`, reset each step, deterministic order, and verify the reporting and reset tests

## 6. Physics queries

- [ ] 6.1 Implement `raycast(origin, direction, opts)` with `maxDistance` (required), `mask`, `all`, `sensors` and verify nearest, sorted-all, miss, and sensor-filter tests
- [ ] 6.2 Implement `overlap(shape, opts)` returning matching `Body`/`Character` handles (including sensors) and verify hit, mask, empty, and character tests
- [ ] 6.3 Implement `shapeCast(shape, from, motion, opts)` returning `{ point, normal, fraction, body }` and verify first-hit, miss, and sensor-filter tests

## 7. Character controller

- [ ] 7.1 Implement capsule `Character` creation and validation (radius, total height >= 2r, up, floorMaxAngle, floorSnapLength, stepHeight, maxSlides, safeMargin, layer/mask) and verify creation + validation tests
- [ ] 7.2 Implement `moveAndSlide` swept slide loop with `maxSlides`, projection, `safeMargin`, and the result object, and verify free-move, wall-slide, and maxSlides tests
- [ ] 7.3 Implement floor/wall/ceiling classification from `floorMaxAngle` and verify floor, steep-slope-is-wall, and ceiling tests
- [ ] 7.4 Implement floor snapping over `floorSnapLength` with upward-detachment and verify slope-attached and jump-detach tests
- [ ] 7.5 Implement step-up (up/forward/down casts within `stepHeight`, disabled at 0) and verify low-ledge-climbed and tall-obstacle-blocked tests
- [ ] 7.6 Implement one-way push: characters act as immovable colliders in `step` (push dynamic bodies) and never block on dynamic bodies/sensors in `moveAndSlide`, and verify both push and not-pushed tests

## 8. Bindings (desktop + web parity)

- [ ] 8.1 Register the `efx.physics` namespace (gravity, iterations, step, clear, createBody, createCharacter, createStaticMesh, raycast, overlap, shapeCast) in `src/api/api.c` with the documented validation/errors and verify a local script smoke via the player
- [ ] 8.2 Implement native-backed `Body` and `Character` JS classes (`destroy()` idempotent, GC finalizer backstop, read-only props, `Body.transform` translation matrix, `contacts`) and verify destroy/use-after-destroy/GC-reclaim tests
- [ ] 8.3 Mirror the namespace and classes in the `src/web/` bridge with identical names/semantics/errors and verify the web smoke path runs the same script
- [ ] 8.4 Verify desktop/web parity with a script that exercises bodies, queries, sensors, and a character and prints rounded deterministic output through both runtimes

## 9. Portable script tests and cross-runtime gate

- [ ] 9.1 Add `tests/scripts/physics_smoke.js` (settle, slide, step, sensor, push, queries with rounded assertions) and verify it passes under the native ctest runner
- [ ] 9.2 Add the `web_12_physics` Emscripten case and verify the Emscripten ctest + `tools/run_web_compare.mjs` cross-runtime comparison matches
- [ ] 9.3 Wire the physics tests into the headless ctest set and verify `ctest` passes with `-DEFX_HEADLESS=ON`

## 10. Documentation

- [ ] 10.1 Update `docs/js-api.md` with the `efx.physics` catalog, the `Body`/`Character` class entries (classification, read-only props, destroy), shapes, validation, and the F12 tags and verify it against the specs
- [ ] 10.2 Update `gallery/src/api/efx.d.ts` so every new call form type-checks and invalid shapes/fields are rejected, and verify the gallery type-test passes
- [ ] 10.3 Write `docs/decisions/0040-physics-core.md` (pure-C/no-GLM wall, linear-only impulses, script-owned step, single world, one-way kinematic push, sensors, determinism) and add its index row
- [ ] 10.4 Update the `AGENTS.md` roadmap table and current-state section for F12 and verify a session can place the milestone from `AGENTS.md` alone

## 11. Gallery showcase sample

- [ ] 11.1 Add a curated physics showcase sample (character `moveAndSlide` over wall/slope/step, a sensor trigger, falling/pushable props, a raycast) using only the public API and procedural geometry, and verify it runs in the gallery host and under the player

## 12. Verification gate

- [ ] 12.1 Re-read the main `js-api` and `feature-roadmap` specs and re-sync this change's MODIFIED blocks if `f11-particles-billboards` has archived, then verify `npx openspec validate --strict` passes
- [ ] 12.2 Run the Linux pipeline first (native suites incl. goldens + `efx_physics_tests`) via `python3 tools/verify_remote.py all <branch>` and fix any failures
- [ ] 12.3 Dispatch the four-target gate (`gh workflow run ci.yml --ref <branch>`) and confirm native Linux/Windows/macOS suites and the Emscripten ctest + cross-runtime compare are green
- [ ] 12.4 Merge to `main` and push once the gate is green, then archive the change and sync the specs
