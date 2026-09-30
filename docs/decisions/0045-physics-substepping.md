# 0045 — The physics step sub-divides a large `dt` into bounded substeps

Status: Accepted (2026-09, change `physics-tunneling`)

Supports: ADR 0001 (C11 core with a C ABI); ADR 0040 (the F12 physics core —
this ADR amends its "no fixed-step accumulator" clause); ADR 0015 (fixed-function
consumer API — no script-visible change); ADR 0022 (web bridge shares one core);
ADR 0020 (four-target gate).

## Context

The F12 dynamic-body pipeline integrated each body over the whole frame `dt`
and detected contacts only by discrete overlap at the post-integration pose.
There was no continuous collision detection for dynamic bodies and no
sub-stepping (ADR 0040 deliberately chose "no fixed-step accumulator"). A thin
or zero-thickness static triangle-mesh collider (the `createStaticMesh` path,
e.g. a `makePlane` ground) is therefore skipped whenever the body's translation
in one `dt` approaches its own extent.

This is frame-rate dependent, so it hid in the browser: `requestAnimationFrame`
keeps `dt` near 16 ms, while the native player can reach the engine's 0.1 s
clamp (first-frame D3D11/pipeline setup, window hitches, or a software/WARP
session at a low frame rate). Reproduced headlessly against the pre-change
core: a 0.8 m box dropped from 4 m onto a two-triangle plane rests for
`dt <= 1/15 s` and tunnels for `dt >= 0.08 s`; a 1 m-thick box floor never
tunnels. The gallery physics sample passes the raw frame `dt` to
`efx.physics.step`, so a native run at a low frame rate dropped its props
through a mesh floor.

## Decision

`efx_physics_step` (`src/physics/world.c`) clamps `dt` exactly as before, then
simulates the clamped value as `n = max(1, ceil(dt / EFX_PHYS_MAX_SUBSTEP))`
equal substeps of `dt / n`, each running the existing
integrate -> generate-contacts -> solve cycle. `EFX_PHYS_MAX_SUBSTEP`
(`src/physics/world.h`) is `1/60 s`, the documented recommended cadence, so:

- a `step(1/60)` is exactly one substep and reproduces the pre-change
  arithmetic bit-for-bit (verified by an A/B core run);
- at the 0.1 s clamp `n <= 6`, bounding cost;
- the per-substep translation is bounded, so a dynamic body cannot skip a thin
  static collider at ordinary gravity/dynamics speeds.

The accumulator is **per call and bounded**, not an engine-owned fixed-step
loop: `step` still owns the simulation, no state is added to
`efx_physics_world`, and nothing is carried between calls. An accumulated
`applyForce` is applied on every substep at `force * inv_mass * h` and cleared
once after the last substep; `contacts` report the final substep. The public C
surface and the script API are unchanged.

## Consequences

- A dynamic body whose *script-set* velocity exceeds the per-substep bound can
  still tunnel; full CCD remains out of scope and is recorded as a known
  limitation rather than solved here.
- Scripts that step with a `dt` larger than 1/60 now get a sub-divided
  simulation instead of one large step. This is intended (more accurate, no
  slow-motion tunneling) and only affects that range; the 1/60 path is
  byte-identical.
- Determinism is preserved for identical call sequences: substeps derive only
  from `dt` and are integer-counted, so the portable cross-runtime compare
  stays meaningful.
- The F12 design risk "script-owned step with variable `dt`" is closed for
  thin static geometry. Future CCD work must keep the 1/60 path unchanged or
  re-baseline the affected scenarios.

## Rejected alternatives

- **Swept dynamic-vs-static CCD.** Correct at any speed, but needs new
  multi-contact time-of-impact handling and leaves dynamic-vs-dynamic discrete;
  deferred rather than adopted for a bounding mitigation.
- **An engine-owned fixed-step loop / real-time accumulator in the world.**
  Rejected: it adds state to `efx_physics_world`, complicates `clear()` and
  determinism, and reverses ADR 0040's script-owned-step architecture.
- **Making every script pass a fixed `dt`.** Rejected: that is the footgun that
  produced the bug, and it does not protect library users.
- **Leaving the 0.1 s clamp as the only bound.** Rejected: the clamp is already
  inside the failure band for ordinary gravity speeds.
