# Spec Delta: collision-physics

## ADDED Requirements

### Requirement: Body teleport and collider debug drawing

Live bodies SHALL expose `setPosition(p)` — an instant teleport to the world
position `p` (`[x, y, z]`). Teleporting SHALL apply to every body kind
(static, dynamic, sensor) and to characters: the position changes
immediately with no interpolation, sweeping, or contact solving along the
way; linear and angular velocity SHALL be cleared to zero; a sleeping body
SHALL be woken. This replaces the destroy-and-recreate workaround for
respawns, resets, and course placement. A non-array, wrong-length, or
non-finite `p` SHALL throw `TypeError`/`RangeError` per the engine's
argument convention, and a destroyed body SHALL throw on use per the
resource rules.

`efx.physics.debugDraw(opts?)` SHALL record, during the render phase, an
unlit translucent visual for every live body and character in the world into
the display list, using the camera and render state in effect at the call —
boxes as box hulls, spheres as sphere hulls, capsules as capsule hulls, and
static mesh bodies as their collision mesh — so collider layout can be
inspected without duplicating shape constants as script-side render calls.
The optional bag SHALL accept `color` (tint `[r, g, b, a]`, default a
distinct debug green) and `alpha` (a finite number in 0..1, default a
translucent value that leaves the scene visible); unknown or wrongly-typed
fields SHALL throw `TypeError`, out-of-range values `RangeError`. The debug
records SHALL participate in the display list like any other draw (sortable,
respecting depth test and blend state), and `debugDraw` SHALL NOT be
recorded outside the render phase — calling it during update SHALL throw.
Bodies destroyed before the call SHALL NOT be drawn.

#### Scenario: Teleport clears velocity and wakes the body

- **WHEN** a dynamic body at rest (or asleep) is teleported with
  `setPosition([10, 5, 0])` and the simulation steps
- **THEN** the body's `position` reflects the new location immediately, its
  `velocity` is zero, and it falls under gravity from there rather than
  continuing any prior motion

#### Scenario: Teleport works on every body kind

- **WHEN** `setPosition` is called on a static body used as a relocatable
  trigger, a dynamic body, and a character
- **THEN** each is placed at the target position on the next query, with no
  new resources created and no handles invalidated

#### Scenario: Invalid teleport arguments throw

- **WHEN** `setPosition` is called with a non-array, a two-number array, or a
  `NaN` component, on both runtimes
- **THEN** the call throws the same error class with the identical message
  and the body is unmoved

#### Scenario: Debug draw records every live collider

- **WHEN** `efx.physics.debugDraw()` is called in the render phase with a
  static box, a dynamic box, a sensor sphere, and a character in the world
- **THEN** translucent hulls appear for all four at their current transforms,
  interleaved with the frame's other draws through the display list

#### Scenario: Debug draw options validate

- **WHEN** `debugDraw({ color: [1, 0, 0, 1], alpha: 0.5 })` versus
  `debugDraw({ bogus: 1 })` or `debugDraw({ alpha: 2 })` is called
- **THEN** the first renders with the given tint and alpha, the unknown
  field throws `TypeError`, and the out-of-range alpha throws `RangeError`

#### Scenario: Debug draw outside render throws

- **WHEN** `debugDraw` is called from an update hook
- **THEN** the call throws and nothing is recorded
