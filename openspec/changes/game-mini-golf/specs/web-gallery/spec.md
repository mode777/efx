# Spec Delta

## ADDED Requirements

### Requirement: Mini Golf game sample

The curated gallery catalog SHALL include a Mini Golf game sample — the
fourth rung of the game series and the first 3D rung, built on the impulse
dynamics, sensor, and query surface — and it SHALL use only the public `efx`
API and standard ES6, with no browser or Node.js dependency.

The sample SHALL implement a complete nine-hole mini-golf loop authored as
data: each hole SHALL provide a tee, a cup detected as a sensor volume, wall
geometry, and a par rating; the ball SHALL be a dynamic body whose rolling
and bouncing come from friction and restitution; and each stroke SHALL apply
an impulse to the ball along an aim direction set by a mouse raycast onto the
course plane, with a hold-to-charge power meter whose release fires the
stroke. The stroke count SHALL be displayed as text with par, sinking the
cup SHALL advance to the next hole, and completing the ninth hole SHALL show
a scorecard comparing strokes to par that restarts on player input. Each
hole's perimeter SHALL contain the ball so it cannot leave the course. The
sample SHALL render under a 3D camera that follows the ball, with a
procedurally generated course texture and a blob shadow under the ball. The
sample SHALL self-play (script-driven aim and strokes) from launch until the
first player input.

The sample's only shipped asset SHALL be the standard CC0 font under the
existing curated sample asset-pack contract, with provenance recorded in
`gallery/samples/curated/CREDITS.md`. The sample SHALL be runnable by the
player under the same resource-root contract as any other sample, and SHALL
be covered by the gallery smoke run.

#### Scenario: Catalog contains the game sample

- **WHEN** the curated gallery catalog is read
- **THEN** it contains the Mini Golf sample under a Games category, and
  selecting it runs the sample in the application area

#### Scenario: Complete nine-hole round

- **WHEN** the sample runs
- **THEN** it plays stroke-cup-hole cycles across nine holes with stroke and
  par text, the ninth hole ends in a scorecard comparing strokes to par, and
  the scorecard restarts the round on player input

#### Scenario: Strokes exercise the physics surface

- **WHEN** the player aims with the mouse and holds to charge
- **THEN** the aim direction comes from a raycast through the cursor onto the
  course plane, the released power applies as an impulse to the dynamic
  ball, and the ball rolls and bounces off walls and slopes under friction
  and restitution until it stops for the next stroke

#### Scenario: Sinking the cup

- **WHEN** the slow-moving ball enters the cup's sensor volume
- **THEN** the hole completes and the round advances to the next hole

#### Scenario: Course is procedural and contained

- **WHEN** any hole is played
- **THEN** its geometry and grass texture are generated procedurally, its
  walls contain the ball within the course, and the only loaded asset is the
  shipped font

#### Scenario: Self-play, portability, and smoke

- **WHEN** the sample runs with no player input, its source is run by the
  `efx` player against its resource-root directory, or the gallery smoke run
  executes
- **THEN** script-driven strokes play visibly until the first player input;
  it runs using only the public API and standard ES6; and the smoke fails on
  any console or page error from it
