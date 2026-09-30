# character-controller Specification

## Purpose
Defines the kinematic capsule character controller: a script-driven body that
moves through the collision world with swept motion, slides along surfaces,
classifies floors, walls, and ceilings, snaps to slopes, and climbs small
steps.

## Requirements

### Requirement: Capsule character creation

`efx.physics.createCharacter(opts)` SHALL create a native-backed `Character`
whose collision volume is a vertical capsule. `opts` SHALL accept `radius`
(positive), `height` (total tip-to-tip length including caps, `>= 2 * radius`),
`position` (default `[0, 0, 0]`), `up` (default `[0, 1, 0]`; MUST be a non-zero
vector), `floorMaxAngle` (degrees, default `45`), `floorSnapLength` (default
`0.1`), `stepHeight` (default `0.3`; `0` disables step-up), `maxSlides`
(positive integer, default `6`), `safeMargin` (default `0.001`), and `layer` /
`mask`. A non-positive `radius`, a height below `2 * radius`, a non-positive
`maxSlides`, a zero `up`, or a negative `floorSnapLength` / `stepHeight` /
`safeMargin` SHALL throw `RangeError`; unknown fields or wrong types SHALL
throw `TypeError`. The `Character` SHALL expose read-only `position` and
mutable `velocity`, a read-only `onFloor` reflecting the last move, and an
idempotent `destroy()` with destroyed-use throwing. Like a `Body`, a live
`Character` SHALL be held by the world until `destroy()`, `efx.physics.clear()`,
or runtime teardown; dropping every script reference SHALL NOT remove it.

#### Scenario: Valid character is created

- **WHEN** `createCharacter({ radius: 0.4, height: 1.8, position: [0, 1, 0] })`
  is called
- **THEN** the character exists with its capsule centered at the position, and
  its non-overridden parameters take the documented defaults

#### Scenario: Invalid parameters throw

- **WHEN** a character is created with `height: 0.5` (below `2 * radius`),
  `maxSlides: 0`, `up: [0, 0, 0]`, or an unknown field
- **THEN** the numeric/range cases throw `RangeError` and the unknown field
  throws `TypeError`, and nothing is created

#### Scenario: Destroyed character is safe

- **WHEN** `destroy()` is called twice and the character is used after the
  first call
- **THEN** the second call is a no-op and the later use throws

### Requirement: Move and slide

`character.moveAndSlide(motion)` SHALL move the character by the given motion
vector against the world's solid static and kinematic geometry. The character
SHALL sweep its motion, stop at the first blocking hit, then continue the
remaining motion projected onto the blocking surface, repeating until no
further hit occurs or `maxSlides` is reached. The motion SHALL be applied to
the character's position and the character's `velocity` SHALL NOT be integrated
by this call (the script supplies the full motion). The method SHALL return a
plain JS object
`{ position, onFloor, onWall, onCeiling, floorNormal, collisions }`, where
`collisions` lists the blocking contacts encountered
`{ body, normal, point }`. Dynamic bodies and sensors SHALL NOT block the move.

#### Scenario: Motion without obstacles is applied in full

- **WHEN** `moveAndSlide([0, 0, 1])` is called with no geometry in the way
- **THEN** the character's position advances by the full motion and the result
  reports no floor, wall, or ceiling contact

#### Scenario: A wall produces a slide

- **WHEN** a character moves diagonally into a wall whose normal is
  perpendicular to part of the motion
- **THEN** the component along the wall is preserved, the component into the
  wall is removed, and the result reports `onWall` with the wall normal

#### Scenario: maxSlides bounds the iteration

- **WHEN** a character is driven into a corner that would require more
  redirections than `maxSlides`
- **THEN** the move stops after `maxSlides` redirections without exceeding the
  requested motion

#### Scenario: Dynamic bodies do not block

- **WHEN** `moveAndSlide` drives the character into a dynamic box
- **THEN** the character is not stopped by it (only solid static or kinematic
  geometry blocks)

### Requirement: Floor, wall, and ceiling classification

After a `moveAndSlide`, the controller SHALL classify each blocking contact by
comparing its normal to the character's `up` vector: a contact whose angle to
`up` is within `floorMaxAngle` is a **floor**, one whose angle to the opposite
of `up` is within `floorMaxAngle` is a **ceiling**, and every other contact is
a **wall**. The result SHALL report `onFloor`, `onWall`, and `onCeiling`
accordingly, and `floorNormal` SHALL be the floor contact normal when
`onFloor` is true.

#### Scenario: Standing on a floor

- **WHEN** a character descends onto a horizontal plane and `moveAndSlide`
  runs with a downward motion
- **THEN** the result reports `onFloor` true and `floorNormal` approximately
  `[0, 1, 0]`

#### Scenario: Hitting a steep slope is a wall

- **WHEN** a character is pushed into a slope steeper than `floorMaxAngle`
- **THEN** the contact is reported as a wall (not a floor)

#### Scenario: Hitting a ceiling

- **WHEN** a character moves upward into an overhang
- **THEN** the result reports `onCeiling` true

### Requirement: Floor snapping

When `floorSnapLength` is greater than zero and the character is moving along
the ground, `moveAndSlide` SHALL keep the character attached to downward
slopes by casting down up to `floorSnapLength` and moving the character to the
ground if one is found. Snapping SHALL NOT be applied when the motion has an
upward component that detaches the character (for example a jump), so the
character can leave the ground.

#### Scenario: Walking down a slope stays attached

- **WHEN** a grounded character moves down a gentle slope with a downward
  component shorter than `floorSnapLength`
- **THEN** the character remains on the slope surface and `onFloor` stays true
  across the move

#### Scenario: Jumping detaches

- **WHEN** a grounded character's motion has an upward component (a jump)
- **THEN** snapping does not pull it back to the ground and `onFloor` becomes
  false

### Requirement: Step-up

When `stepHeight` is greater than zero, `moveAndSlide` SHALL climb an obstacle
whose height is at most `stepHeight` by casting up by `stepHeight`, casting the
motion forward, and casting down to land on the obstacle's top, instead of
being blocked by it. When `stepHeight` is `0`, step-up SHALL be disabled and
such an obstacle SHALL block the move as a wall.

#### Scenario: A low ledge is climbed

- **WHEN** a grounded character walks into a ledge no taller than `stepHeight`
- **THEN** its position ends on top of the ledge and the move is not blocked

#### Scenario: A tall obstacle is not climbed

- **WHEN** a character walks into an obstacle taller than `stepHeight`
- **THEN** it is blocked, reported as a wall, and does not step up

#### Scenario: Step-up can be disabled

- **WHEN** a character is created with `stepHeight: 0` and walks into a low
  ledge
- **THEN** the ledge blocks the move as a wall

### Requirement: One-way push of dynamic bodies

During `efx.physics.step`, the world SHALL treat every character as an
immovable (infinite-mass) kinematic collider, so a dynamic body overlapping or
in contact with a character is pushed away from it and receives an impulse,
while the character is unaffected. `moveAndSlide` SHALL likewise not be
blocked by dynamic bodies, so characters push through them in the direction of
travel.

#### Scenario: Character pushes a dynamic crate

- **WHEN** a character moves into a dynamic crate and `step` runs
- **THEN** the crate is pushed away from the character (gaining velocity),
  and the character's position is unchanged by the crate

#### Scenario: Character is not pushed by dynamics

- **WHEN** a fast dynamic body collides with a stationary character and `step`
  runs
- **THEN** the character does not move
