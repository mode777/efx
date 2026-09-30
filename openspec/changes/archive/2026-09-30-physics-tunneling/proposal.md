# Proposal

## Why

The physics showcase falls through the floor on a native Windows build while
the same sample runs correctly on the web. The F12 core (milestone **F12
collision + character + impulse dynamics**) integrates each dynamic body over
the whole frame `dt` and detects contacts only by discrete overlap at the
post-integration pose — there is no continuous collision detection for dynamic
bodies and no bounded sub-stepping (ADR 0040 explicitly chooses "no fixed-step
accumulator"). A **thin or zero-thickness static triangle-mesh collider** (the
F12 `createStaticMesh` path, e.g. a `makePlane` ground or an imported level
floor) is therefore missed whenever the body's per-step translation approaches
its own extent. The sample passes the raw render frame `dt` to
`efx.physics.step(dt)`: the browser's `requestAnimationFrame` cadence keeps
`dt` near 16 ms so the collision is sampled, while the native Windows player
can reach the engine's 0.1 s clamp (first-frame pipeline/D3D11 setup, window
hitches, or a software/WARP session at a low frame rate), at which point the
body skips the floor entirely. The failure is frame-rate dependent, not
Windows-specific, but Windows is where the large `dt` occurs in practice.

Reproduced headlessly against the current core: a dynamic 0.8 m box dropped
from 4 m onto a two-triangle plane rests for `dt <= 1/15 s` but tunnels
(`y` far below the plane) for `dt >= 0.08 s`; the same body over a 1 m-thick
box floor never tunnels at any tested `dt`. This is the reported symptom.

## What Changes

- **Bounded sub-stepping in `efx.physics.step(dt)`** (in `src/physics/`, the
  pure-C core): the already-clamped `dt` is subdivided into equal substeps so
  that no substep exceeds a fixed maximum (`EFX_PHYS_MAX_SUBSTEP = 1/60 s`,
  i.e. `n = ceil(dt / MAX_SUBSTEP)`, bounded by the existing 0.1 s clamp to
  <= 6 substeps). Each substep runs the existing integrate -> generate
  contacts -> solve pipeline; the final substep's state and contacts are the
  ones reported after `step`. A `step(1/60)` is exactly one substep, so the
  default guidance and all existing 1/60 scenarios are byte-identical.
- **Force semantics preserved across substeps**: an accumulated `applyForce`
  acts over the whole `step` (applied per substep at `force * inv_mass *
  substep_dt`, cleared only after the last substep) instead of for a single
  substep.
- **Headless tests** (`tests/physics/`): a thin/zero-thickness plane at large
  `dt` no longer tunnels; a body with a large script-set velocity is caught by
  a thin floor at the 0.1 s clamp; force-across-substeps integration is
  correct; the existing determinism scenarios are unchanged at 1/60.
- **Docs**: extend the `efx.physics.step` description in the type document
  `gallery/src/api/efx.d.ts` (the step is frame-rate robust and may internally
  subdivide a large `dt`) and regenerate `docs/api/`. A new ADR records the
  sub-stepping decision and its effect on ADR 0040's "no fixed-step
  accumulator" clause. The gallery sample needs no change.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `collision-physics`: the "Impulse-based linear dynamics" requirement gains
  that `step(dt)` SHALL subdivide a large `dt` into bounded substeps so dynamic
  bodies cannot skip thin static geometry, plus a scenario asserting a body
  settles on a zero-thickness mesh plane even at the maximum clamped `dt`.
  Contact-reporting and determism semantics are unchanged.

## Impact

- `src/physics/world.c` (`efx_physics_step`), `src/physics/world.h` (the new
  `EFX_PHYS_MAX_SUBSTEP` tuning constant); no change to `solver.c`,
  `narrow.c`, `broadphase.c`, `character.c`, or the public C surface in
  `physics.h`.
- No script-facing API change: `efx.physics.step(dt)` keeps its signature,
  return value, and error contract; only its internal sampling granularity
  changes for `dt > 1/60`.
- Tests: `tests/physics/tests_world.c` (new thin-collider / large-`dt` /
  force cases); `tests/scripts/physics_smoke.js` may gain a rounded
  large-`dt` assertion. No golden images (F12 has none).
- Docs: `gallery/src/api/efx.d.ts` + regenerated `docs/api/`; a new
  `docs/decisions/NNNN-physics-substepping.md` and its index row. No `js-api`
  spec delta (no surface change).

## Non-goals

- Continuous collision detection / swept dynamic bodies (CCD); the fix bounds
  per-substep travel rather than solving the general fast-body case.
- Dynamic-body rotation, dynamic-vs-dynamic CCD, or a persistent engine-owned
  fixed-step loop / interpolation.
- Changing the `efx.physics.step` signature, validation, or the 0.1 s clamp.
- Changing the character controller (already solved by conservative-advancement
  sweeps) or the query sweeps.
