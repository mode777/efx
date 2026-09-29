# collision-physics Specification

## Purpose
Defines the engine's collision detection and linear impulse dynamics core: a
single deterministic world of static, dynamic, and sensor colliders built from
analytic shapes and triangle meshes, stepped by the script and reported through
a per-body contact list.

## Requirements

### Requirement: Collision shape set

`efx.physics` SHALL accept collision shapes as plain JS option objects of
exactly four types: `{ type: 'sphere', radius }`; `{ type: 'box', size }`, an
axis-aligned box whose `size` is its full `[x, y, z]` extent (matching
`makeCube`); `{ type: 'capsule', radius, height }`, a **vertical** capsule on
the world up axis whose `height` is the total tip-to-tip length including the
hemispherical caps and MUST satisfy `height >= 2 * radius`; and
`{ type: 'mesh', mesh }`, a triangle-mesh collider built from a live `Mesh`,
valid only for static bodies. A non-positive `radius` or `size` component, or a
capsule `height` below `2 * radius`, SHALL throw `RangeError`; an unknown shape
`type`, an unknown field, or a non-`Mesh` `mesh` value SHALL throw `TypeError`.
The same shape objects SHALL be accepted by bodies and by spatial queries.

#### Scenario: Analytic shapes are accepted

- **WHEN** `createBody` is called with a valid sphere, box, or capsule shape
- **THEN** the body is created and collides as that shape

#### Scenario: Box size is full extent

- **WHEN** a dynamic body with `{ type: 'box', size: [2, 1, 2] }` rests on a
  plane
- **THEN** it comes to rest with its bottom face on the plane, i.e. its center
  at half the extent above it

#### Scenario: Capsule height includes caps

- **WHEN** a capsule body with `radius: 0.4, height: 1.8` rests on a plane
- **THEN** it comes to rest with its lower cap touching the plane (center
  `0.9` above it), and the tip-to-tip extent is `1.8`

#### Scenario: Invalid shapes throw

- **WHEN** a shape has `radius: 0`, a capsule `height` of `0.5 * radius`, an
  unknown `type`, or a `mesh` value that is not a `Mesh`
- **THEN** the call throws `RangeError` for the numeric cases and `TypeError`
  for the type/value cases, and nothing is created

### Requirement: Collision world and bodies

There SHALL be a single engine-owned collision world. Scripts SHALL add and
remove **bodies** of exactly three kinds through `efx.physics`: static
(immovable level geometry or triggers), dynamic (impulse-simulated), and
sensor (see the Sensors requirement); sensor is a flag orthogonal to the
static/dynamic kind. Bodies SHALL be native-backed classes: each exposes a
`destroy()` that is idempotent, is reclaimed by a GC finalizer if never
destroyed, and throws when used after destruction. `efx.physics.clear()` SHALL
remove every body and reset the world to empty. Storage SHALL be dynamically
allocated (no hard body cap); the reference SHALL document recommended soft
limits rather than a fixed maximum. Every body SHALL carry a `layer` bitmask
and a `mask` bitmask; two colliders interact only when each one's `layer` is
included in the other's `mask`. Body and collider iteration after a step SHALL
be deterministic (stable creation order), so a given sequence of calls
produces the same result.

#### Scenario: Static, dynamic, and sensor bodies coexist

- **WHEN** a static box, a dynamic box, and a sensor sphere are created in one
  world
- **THEN** all three are present, the static one never moves, and the dynamic
  one collides against the static one

#### Scenario: Layer and mask filter interactions

- **WHEN** two dynamic bodies have mutually exclusive layers/masks and fall
  through each other's volume
- **THEN** they report no contacts and pass through, while a body whose mask
  includes the other's layer does report contacts

#### Scenario: Clear empties the world

- **WHEN** `efx.physics.clear()` is called after bodies were added
- **THEN** subsequent queries and steps observe an empty world, and using a
  previously returned body handle throws

#### Scenario: Destroyed body is safe

- **WHEN** `destroy()` is called on a body and then again, and the body is used
  in a query after the first call
- **THEN** the second `destroy()` is a no-op and the later use throws

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

#### Scenario: Gravity makes a body fall and rest

- **WHEN** a dynamic sphere is created above a static plane and `step` is
  called repeatedly
- **THEN** it falls and comes to rest with its surface on the plane, without
  sinking through it or jittering beyond tolerance

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

### Requirement: Contact reporting

After a `step`, each dynamic body SHALL expose a read-only `contacts` list,
valid until the next `step`, of plain JS objects
`{ body, sensor, normal, point, depth, impulse }`: `body` is the other
collider's handle (`null` when it is a static mesh), `sensor` marks an overlap
with a sensor, `normal` is the contact normal, `point` is the contact position,
`depth` is the penetration depth, and `impulse` is the accumulated normal
impulse applied. A body with no contacts SHALL report an empty list. The list
SHALL be ordered deterministically for a given scene and step sequence.

#### Scenario: Contact reports the other body

- **WHEN** a dynamic box lands on a static box and `step` runs
- **THEN** the dynamic body's `contacts` contains one entry whose `body` is the
  static body, with a downward-to-upward normal, a penetration depth, and a
  positive impulse

#### Scenario: Contacts reset each step

- **WHEN** a body separates and `step` runs again
- **THEN** its `contacts` is empty for that step

#### Scenario: Contacts are plain data

- **WHEN** a script reads `body.contacts`
- **THEN** it receives plain JS objects holding only engine-provided
  primitives, with no host or native object and no methods

### Requirement: Sensors

A collider declared with `sensor: true` SHALL never resolve collisions, never
block a character's `moveAndSlide`, and never receive an impulse, but it SHALL
be reported: it appears in `overlap` results, and a dynamic body overlapping it
SHALL report it in `contacts` with `sensor: true`. Sensors SHALL be excluded
from `raycast` results unless the query asks for them. Sensors MAY be static or
attached to a moving body.

#### Scenario: Sensor does not block but is reported

- **WHEN** a dynamic body falls through a sensor volume and `step` runs
- **THEN** the body keeps moving (no resolution) and reports the sensor in its
  `contacts`, while a solid collider in the same place would stop it

#### Scenario: Characters pass through sensors

- **WHEN** `moveAndSlide` drives a character through a sensor volume
- **THEN** the character is not blocked, and `overlap` at that position reports
  the sensor

### Requirement: Isolated deterministic core

The collision and dynamics core SHALL be implemented as a self-contained C
module with no dependency on the renderer, the platform layer, the script
runtime, or the C++ math library. It SHALL expose a pure C surface that a
standalone headless test binary can exercise directly — without a display, a
window, or the script runtime — and its results SHALL be deterministic for
identical inputs and step sequences (no wall-clock, RNG, or thread dependence).
Two worlds stepped in interleaved order SHALL each behave as if the other did
not exist.

#### Scenario: Core tests run without the engine

- **WHEN** the physics test binary is built and run headless
- **THEN** it exercises narrowphase tests, scenarios, and stepping without
  linking the renderer, platform layer, script runtime, or a display

#### Scenario: Determinism across identical runs

- **WHEN** the same scene and the same sequence of impulses and `step` calls
  are replayed twice
- **THEN** the resulting positions and contacts are identical

#### Scenario: Independent worlds do not interfere

- **WHEN** two worlds are created and their steps are interleaved
- **THEN** each world's final state equals the state it reaches when stepped
  alone
