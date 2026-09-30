# Spec Delta

## MODIFIED Requirements

### Requirement: Impulse-based linear dynamics

Dynamic bodies SHALL carry a positive `mass`, a `velocity`, per-body
`friction` and `restitution`, and SHALL be affected by the world `gravity`
(vector, default `[0, -9.81, 0]`). `applyImpulse(v)` SHALL change velocity
instantly by `v / mass`; `applyForce(v)` SHALL accumulate a force consumed by
the next `step`. `efx.physics.step(dt)` — which the **script** calls — SHALL
integrate each dynamic body, detect contacts, resolve them with a bounded
number of sequential impulses applied along the contact normal (with friction
impulses clamped by the friction coefficient and a restitution term suppressed
below a small relative-velocity threshold), and correct remaining penetration
with a slop so resting bodies do not sink or jitter. The solver SHALL be
**linear only**: dynamic bodies SHALL NOT rotate, so boxes remain axis-aligned
and capsules remain vertical. A non-positive `mass` on a dynamic body SHALL
throw `RangeError`. Nothing SHALL move until `step` is called (the engine SHALL
NOT advance the world itself).

To keep the simulation frame-rate robust, `step(dt)` SHALL NOT advance a
dynamic body by more than a bounded maximum translation in a single collision
sample: a `dt` larger than a fixed maximum substep SHALL be simulated as a
sequence of equal substeps no larger than that bound, each running the full
integrate/detect/resolve cycle. A `dt` at or below the bound SHALL be simulated
as a single substep, so the default recommended cadence is unchanged. The
per-body `contacts` list after `step` SHALL describe the final substep, and an
accumulated `applyForce` SHALL act over the whole `step` and be cleared exactly
once per `step`.

#### Scenario: Gravity makes a body fall and rest

- **WHEN** a dynamic sphere is created above a static plane and `step` is
  called repeatedly
- **THEN** it falls and comes to rest with its surface on the plane, without
  sinking through it or jittering beyond tolerance

#### Scenario: Thin static geometry is not skipped at a large step

- **WHEN** a dynamic body is dropped onto a zero-thickness static mesh plane
  and `step` is called with the maximum accepted `dt`
- **THEN** the body settles on the plane instead of passing through it

#### Scenario: A force acts across the whole step

- **WHEN** `applyForce` accumulates a force and a `step` with a `dt` larger
  than the maximum substep is taken
- **THEN** the resulting velocity change reflects the force acting over the
  entire `dt`, and the force is consumed exactly once

#### Scenario: Impulse changes velocity

- **WHEN** `applyImpulse([0, 4, 0])` is called on a stationary dynamic body of
  mass `2`, then `step` runs
- **THEN** the body's velocity gained `[0, 2, 0]` (before gravity and contacts)
  and its position advances accordingly

#### Scenario: Restitution and friction shape the response

- **WHEN** a dynamic sphere with high restitution hits a plane, and a sliding
  body with high friction crosses a plane
- **THEN** the first rebounds (its normal velocity reverses in proportion to
  restitution) and the second's tangential velocity is reduced by friction

#### Scenario: No movement without a step

- **WHEN** a dynamic body is given velocity or an impulse but `step` is never
  called
- **THEN** its position is unchanged

#### Scenario: Invalid mass is rejected

- **WHEN** a dynamic body is created with `mass: 0` or a negative mass
- **THEN** the call throws `RangeError`
