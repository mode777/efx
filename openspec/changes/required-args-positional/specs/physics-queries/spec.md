# Spec Delta

## MODIFIED Requirements

### Requirement: Raycast

`efx.physics.raycast(origin, direction, maxDistance, opts?)` SHALL cast a ray from a 3D
point along a direction (which the engine normalizes) and return the nearest
hit, or `null` when nothing is hit. `maxDistance` is a required positional
positive finite number — the query SHALL throw `TypeError` when it is
missing or not a positive finite number. The trailing `opts` bag is optional
and SHALL accept `mask` (a collision bitmask), `all`
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

- **WHEN** the same ray is cast with `all: true` in the options bag
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

- **WHEN** `raycast` is called without the positional `maxDistance` or with a
  non-positive value
- **THEN** the call throws `TypeError`
