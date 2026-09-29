# physics-queries Specification

## Purpose
Defines the engine's spatial queries against the collision world — raycasts,
overlap tests, and shape casts — used for picking, line-of-sight, triggers, and
projectiles independently of whether any dynamic simulation is running.

## Requirements

### Requirement: Raycast

`efx.physics.raycast(origin, direction, opts?)` SHALL cast a ray from a 3D
point along a direction (which the engine normalizes) and return the nearest
hit, or `null` when nothing is hit. `opts` SHALL accept `maxDistance` (a
positive finite number; required — the query SHALL throw `TypeError` when it is
missing or not a positive finite number), `mask` (a collision bitmask), `all`
(when `true`, return every hit sorted by distance instead of the nearest), and
`sensors` (when `true`, include sensors). A hit SHALL be a plain JS object
`{ point, normal, distance, body }`. The query SHALL be usable with static
geometry only, with no dynamic bodies present.

#### Scenario: Nearest hit is returned

- **WHEN** a ray is cast through two boxes at different distances with
  `maxDistance` beyond both
- **THEN** the result is the nearer box, with its surface `point`, outward
  `normal`, and the distance to the hit

#### Scenario: All hits are sorted

- **WHEN** the same ray is cast with `all: true`
- **THEN** an array of hits is returned in ascending distance order

#### Scenario: Miss returns null

- **WHEN** a ray is cast in a direction with no geometry, or all geometry is
  beyond `maxDistance`
- **THEN** the result is `null` (or an empty array with `all: true`)

#### Scenario: Sensors are excluded unless requested

- **WHEN** a ray crosses a sensor and a solid box at different distances
- **THEN** by default the sensor is ignored and only the solid box is reported,
  and with `sensors: true` the sensor is included and sorted by distance

#### Scenario: Missing maxDistance is rejected

- **WHEN** `raycast` is called without `maxDistance` or with a non-positive
  value
- **THEN** the call throws `TypeError`

### Requirement: Overlap

`efx.physics.overlap(shape, opts?)` SHALL return the live colliders that
intersect a shape placed at a given position. `opts` SHALL accept `position`
(default `[0, 0, 0]`) and `mask`. The result SHALL be an array of live
`Body` and/or `Character` handles (including sensors and dynamic bodies), and
SHALL be empty when nothing overlaps. The same shape objects accepted by bodies
SHALL be accepted here. Overlap SHALL be usable as a per-frame trigger poll
with no dynamic bodies present.

#### Scenario: Overlap reports intersecting colliders

- **WHEN** a query sphere is placed overlapping a static box and a sensor
- **THEN** the result contains both handles

#### Scenario: Overlap respects the mask

- **WHEN** a query shape overlaps bodies on several layers and a `mask` is
  given
- **THEN** only handles whose `layer` is in the mask are returned

#### Scenario: Overlap with nothing returns empty

- **WHEN** a query shape is placed in empty space
- **THEN** the result is an empty array

#### Scenario: Characters are reportable

- **WHEN** a query shape is placed overlapping a character
- **THEN** the character's handle is returned

### Requirement: Shape cast

`efx.physics.shapeCast(shape, from, motion, opts?)` SHALL sweep a shape along a
motion vector and return the first hit or `null`. `from` is the shape's start
position and `motion` is its linear displacement. `opts` SHALL accept `mask`
and `sensors` (default `false`). A hit SHALL be a plain JS object
`{ point, normal, fraction, body }`, where `fraction` is the normalized time of
impact in `[0, 1]`. The query SHALL be usable independently of dynamic
simulation.

#### Scenario: First blocking hit is returned

- **WHEN** a sphere is cast along a motion that crosses two boxes
- **THEN** the result reports the first box encountered with `fraction` in
  `[0, 1]`, its `point`, and its `normal`

#### Scenario: Miss returns null

- **WHEN** a shape is cast along a motion that reaches no collider
- **THEN** the result is `null`

#### Scenario: Sensors are excluded by default

- **WHEN** a shape is cast through a sensor and a solid collider
- **THEN** by default the sensor is ignored, and with `sensors: true` the
  earliest of the two is reported
