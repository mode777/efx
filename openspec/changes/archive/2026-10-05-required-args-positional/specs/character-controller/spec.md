# Spec Delta

## MODIFIED Requirements

### Requirement: Capsule character creation

`efx.physics.createCharacter(radius, height, opts?)` SHALL create a native-backed `Character`
whose collision volume is a vertical capsule, with the required capsule
dimensions positional: `radius` (positive) and `height` (total tip-to-tip
length including caps, `>= 2 * radius`). The trailing `opts` bag is optional
and SHALL accept `position` (default `[0, 0, 0]`), `up` (default `[0, 1, 0]`;
MUST be a non-zero vector), `floorMaxAngle` (degrees, default `45`),
`floorSnapLength` (default `0.1`), `stepHeight` (default `0.3`; `0` disables
step-up), `maxSlides` (positive integer, default `6`), `safeMargin` (default
`0.001`), and `layer` / `mask`. A non-positive `radius`, a height below
`2 * radius`, a non-positive `maxSlides`, a zero `up`, or a negative
`floorSnapLength` / `stepHeight` / `safeMargin` SHALL throw `RangeError`;
unknown bag fields or wrong types SHALL throw `TypeError`. The `Character`
SHALL expose read-only `position` and mutable `velocity`, a read-only
`onFloor` reflecting the last move, and an idempotent `destroy()` with
destroyed-use throwing. Like a `Body`, a live `Character` SHALL be held by the
world until `destroy()`, `efx.physics.clear()`, or runtime teardown; dropping
every script reference SHALL NOT remove it.

#### Scenario: Valid character is created

- **WHEN** `createCharacter(0.4, 1.8, { position: [0, 1, 0] })`
  is called
- **THEN** the character exists with its capsule centered at the position, and
  its non-overridden parameters take the documented defaults

#### Scenario: Invalid parameters throw

- **WHEN** a character is created with `height: 0.5` (below `2 * radius`),
  `maxSlides: 0`, `up: [0, 0, 0]`, or an unknown bag field
- **THEN** the numeric/range cases throw `RangeError` and the unknown field
  throws `TypeError`, and nothing is created

#### Scenario: Destroyed character is safe

- **WHEN** `destroy()` is called twice and the character is used after the
  first call
- **THEN** the second call is a no-op and the later use throws
