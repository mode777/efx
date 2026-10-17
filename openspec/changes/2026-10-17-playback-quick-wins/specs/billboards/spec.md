# Spec Delta

## ADDED Requirements

### Requirement: Billboard playback batching

The playback plan SHALL coalesce consecutive billboard records that share the
same resolved texture, the same resolved blend mode, and the same depth-test
setting into a single draw call, so that billboard-heavy frames do not pay one
draw call and one pipeline/binding apply per record. Batching SHALL be an
optimization only: the rendered output of a batched frame SHALL be identical
to the same frame played back with one draw per billboard, because coalesced
records share one state and draw in record order from the shared vertex
buffer. A run SHALL break at any intervening record of another kind (quad,
mesh, particle, target switch) and whenever any of the three shared
properties differs. Records that a run spans keep their individual record
order, camera snapshot, orientation, atlas region, rotation, and tint; only
the draw-call granularity changes.

#### Scenario: Consecutive same-state billboards coalesce

- **WHEN** a frame records several `drawBillboard` calls in a row with the
  same texture, the same resolved blend mode, and the same `depthTest`
  setting
- **THEN** playback issues one draw call for the whole consecutive group, and
  the rendered frame is identical to per-record playback

#### Scenario: Intervening record breaks the run

- **WHEN** two billboards sharing texture, blend, and depth-test setting have
  a `drawQuad`, `drawMesh`, particle, or target-switch record between them
- **THEN** each billboard side of the intervening record draws in its own
  run, and the rendered frame is unchanged relative to per-record playback

#### Scenario: Differing state breaks the run

- **WHEN** consecutive billboards differ in texture, resolved blend mode, or
  `depthTest` setting
- **THEN** a run boundary is inserted at the change, and each group draws
  with exactly the state resolved at its records' record time

#### Scenario: Batching preserves depth-test behavior

- **WHEN** a batched run contains billboards with `depthTest: true` placed
  behind opaque geometry, and another batched run contains billboards with
  `depthTest: false` at the same positions
- **THEN** the first run's billboards are occluded by the geometry and the
  second run's billboards draw over it, matching the per-record depth and
  blend behavior requirement
