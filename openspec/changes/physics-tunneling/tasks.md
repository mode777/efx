# Tasks

## 1. Core sub-stepping

- [x] 1.1 Add `EFX_PHYS_MAX_SUBSTEP` (1/60 s) to the solver-tuning block in
  `src/physics/world.h` with a comment tying it to the documented recommended
  cadence, and verify `src/physics` still compiles standalone with
  `cc -std=c11 -Wall -Wextra -Werror -c src/physics/world.c -Isrc`
- [x] 1.2 Rework `efx_physics_step` in `src/physics/world.c` to clamp `dt` as
  today, then simulate `n = max(1, ceilf(dt / EFX_PHYS_MAX_SUBSTEP))` equal
  substeps of `dt / n` (integrate gravity and `force * inv_mass * h`, update
  position/AABB, generate contacts, solve at `h`), clearing the accumulated
  force once after the loop; verify `efx_physics_tests` still reports 27 cases,
  0 failures
- [x] 1.3 Confirm a `step(1/60)` takes exactly one substep and is
  arithmetic-identical to the pre-change path (same code order for `n = 1`);
  verify by rebuilding `efx_physics_tests` and diffing its output against the
  pre-change binary

## 2. Headless tests

- [x] 2.1 Add a `tests/physics/tests_world.c` case dropping a 0.8 m dynamic
  box from 4 m onto a zero-thickness static mesh plane at `dt = 0.1` and verify
  it settles with its surface on the plane instead of tunneling
- [x] 2.2 Add a case asserting a body with a large script-set downward velocity
  is caught by a thin mesh plane at the maximum `dt`, and verify it settles
  (documents the bounded-speed limitation)
- [x] 2.3 Add a case that accumulates `applyForce`, takes one large-`dt` step,
  and verifies the velocity change reflects the force over the whole `dt` and
  the force is consumed exactly once
- [x] 2.4 Verify the existing settle/determinism/step-`dt` cases still pass
  unchanged (`efx_physics_tests` all green)

## 3. Portable script coverage

- [x] 3.1 Extend `tests/scripts/physics_smoke.js` with a rounded assertion that
  a body over a thin mesh floor settles after a large-`dt` step sequence, and
  verify it passes under the native ctest runner

## 4. Documentation and ADR

- [x] 4.1 Add `docs/decisions/00NN-physics-substepping.md` (per
  `docs/decisions/TEMPLATE.md`) recording the bounded-substep decision and the
  amendment to ADR 0040's "no fixed-step accumulator" clause, and add its row
  to `docs/decisions/README.md`
- [x] 4.2 Note in `gallery/src/api/efx.d.ts` on `efx.physics.step` that a
  large `dt` is internally subdivided into bounded substeps, then regenerate
  the reference with `npm --prefix gallery run docs:markdown` and verify
  `npm --prefix gallery run docs:check` passes
- [x] 4.3 Update the F12 entry in `AGENTS.md` (current state + roadmap) to
  record the sub-stepping hardening and the new ADR

## 5. Verification

- [ ] 5.1 Run the pre-CI server suites via
  `python3 tools/verify_remote.py all <branch>` and confirm native ctest
  (incl. `efx_physics_tests` and the smoke suite), Emscripten ctest, and the
  cross-runtime compare `12_physics` are green
- [ ] 5.2 Dispatch `gh workflow run ci.yml --ref <branch>` and confirm the
  Linux -> Windows -> macOS four-target gate is green; capture the run id in
  `AGENTS.md`
