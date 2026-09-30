# Design

## Context

See `proposal.md — Why` for the reported failure. The relevant current-state
facts:

- `efx_physics_step` (`src/physics/world.c`) clamps `dt` to
  `EFX_PHYS_MAX_DT = 0.1 s` and then, in one pass per call, integrates every
  dynamic body over the full `dt` (`solver.c` resolves the resulting contacts;
  `narrow.c` detects them). Dynamic bodies have **no swept/continuous test**;
  the conservative-advancement sweeps in `narrow.c` are used only by the
  character controller and the query shape casts.
- `efx_world_generate_contacts` builds contacts from the **post-integration
  pose**, so a body only collides if it overlaps a collider at that instant.
  A zero-thickness triangle-mesh collider (the `createStaticMesh` path) is
  skipped once the body's translation in one `dt` exceeds the body's own
  extent along the motion.
- The world struct (`world.h`) holds no per-step accumulator; ADR 0040 chose
  "no fixed-step accumulator" deliberately for determinism and simplicity.
- The F12 design (`openspec/changes/archive/2026-09-29-f12-collision-physics`)
  already lists "script-owned step with variable `dt`" as a risk.

Measured headlessly against the current core (a 0.8 m dynamic box dropped onto
a two-triangle plane): rests for `dt <= 1/15 s`, tunnels for `dt >= 0.08 s`.
A 1 m-thick box floor never tunnels. The engine's 0.1 s clamp is therefore
already inside the failure band for ordinary gravity speeds.

## Goals / Non-Goals

**Goals:**

- Make `efx.physics.step(dt)` produce frame-rate-robust collision results:
  a body cannot skip a thin static collider because of a large frame `dt`.
- Keep the default 1/60 cadence bit-for-bit identical (no test/golden churn,
  no change to the documented recommended `dt`).
- Keep the change inside the pure-C core; no script-visible API change.

**Non-Goals:**

- General CCD for fast bodies: a body whose *script-set* velocity exceeds the
  per-substep bound can still tunnel. This is documented, not solved.
- Dynamic-body rotation, dynamic-vs-dynamic CCD, engine-owned fixed-step loop
  or interpolation.

## Decisions

### D1 — Subdivide the clamped `dt` into bounded equal substeps (not CCD)

Inside `efx_physics_step`, after the existing clamp, compute
`n = max(1, (int)ceilf(dt / EFX_PHYS_MAX_SUBSTEP))` and simulate `n` equal
substeps of `h = dt / n`, each running the existing
integrate -> `efx_world_generate_contacts` -> `efx_solver_solve(h)` cycle.

- **Why**: bounding the per-substep translation by `h * |v|` keeps the body
  overlapping a thin collider at some sample as long as `h * |v|` is below the
  body's extent along the motion. It reuses the entire existing pipeline
  unchanged, needs no new narrowphase/solver code, and is deterministic for a
  given call sequence.
- **Alternatives**: swept dynamic-vs-static CCD (correct at any speed, but
  requires new multi-contact TOI handling and dynamic-vs-dynamic is still
  discrete — deferred); an engine-owned fixed-step loop (rejected, violates
  ADR 0040's script-owned step and adds accumulator state to `world.h`);
  making users pass a fixed `dt` (the footgun that caused this bug).

### D2 — `EFX_PHYS_MAX_SUBSTEP = 1/60 s`

- **Why**: it is the documented recommended cadence. A `step(1/60)` therefore
  yields `n = 1` and reproduces today's arithmetic exactly, so the existing
  settle/determinism/portable scenarios and the cross-runtime compare are
  unaffected. At the 0.1 s clamp `n <= 6`, bounding work.
- **Alternatives**: `1/120` (more margin, but doubles work and changes 60 fps
  results/tests); a fixed distance-based bound (needs per-body velocity
  inspection and is harder to bound for mixed scenes).

### D3 — Equal substeps of `dt/n`, no persistent accumulator

Each `step` subdivides its own clamped `dt`; nothing is carried between calls.

- **Why**: no new field in `efx_physics_world`, no interaction with
  `clear()`, and a replay of identical `step` calls is identical by
  construction (the determinism requirement holds). Two scripts reaching the
  same wall-clock time with different call granularity were never guaranteed
  to match.
- **Alternatives**: a fixed-timestep accumulator with leftover time (metered
  real-time playback, but adds state and complicates determinism/`clear()`).

### D4 — Force and contact semantics

An `applyForce` accumulation is applied on every substep at
`force * inv_mass * h` and cleared **once** after the last substep, so it acts
over the whole `step`. `contacts` are (re)generated every substep; the final
substep's report is what the script observes after `step`, matching the
existing "valid until the next step" contract.

- **Why**: clearing per substep would silently scale forces by `1/n`; keeping
  the last report preserves the "after step" meaning.
- **Alternatives**: pre-applying force as an impulse before the loop (same
  result, but diverges from the existing per-step integration order and would
  change 1/60 arithmetic).

### D5 — No public surface change

`efx.physics.step(dt)` keeps its signature, return value, and error rules;
`physics.h` is untouched. The only new symbol is the internal tuning constant
in `world.h`.

- **Why**: this is a behavior hardening, not an API change; the `js-api` spec
  and the generated reference need only a descriptive note, not a delta.

### D6 — ADR

Add `docs/decisions/00NN-physics-substepping.md` (+ index row) recording the
bounded-substep decision and amending ADR 0040's "no fixed-step accumulator"
clause (the accumulator is per-call and bounded, not engine-owned). The F12
design risk "variable `dt`" is thereby closed.

## Risks / Trade-offs

- **Very fast script-set velocities still tunnel** → Documented limitation;
  the bound covers gravity/dynamics speeds. Full CCD is a future change.
- **Additional CPU in a low-frame-rate hitch** → `n <= 6` (0.1 s clamp / 1/60);
  bodies are few and the substep pipeline is the same cheap solver.
- **Subtle behavior change for scripts that step > 1/60 s** → Intended and
  documented; results become more accurate rather than slow-motion-tunneling.
  Existing 1/60 tests are unchanged by D2.
- **Determinism/cross-runtime** → Substeps are derived only from `dt` and
  integer arithmetic (`ceilf`, one division); identical on all four targets,
  so the portable compare stays meaningful.
- **Per-substep contact regeneration cost on large meshes** → The BVH query
  already bounds work; `n` is small.

## Migration Plan

Additive and internal: land `world.h`/`world.c`, the new tests, the type-doc
note + regenerated reference, and the ADR. Rollback is reverting those files;
no data or API migration is required. Verification follows the project gate:
`efx_physics_tests` headless first, then the portable `web_12_physics`
cross-runtime compare, then the Linux -> Windows -> macOS four-target gate.

## Open Questions

None.
