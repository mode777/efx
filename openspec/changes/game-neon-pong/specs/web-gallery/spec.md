# Spec Delta

## ADDED Requirements

### Requirement: Neon Pong game sample

The curated gallery catalog SHALL include a Neon Pong game sample — the first
rung of a game series that demonstrates the engine through complete games
rather than per-feature demos — and it SHALL use only the public `efx` API and
standard ES6, with no browser or Node.js dependency.

The sample SHALL implement a complete Pong game loop: two paddles and a ball
rendered as 2D quads, score rendered as text, a serve delay between rallies,
a point scored when the ball passes a paddle, and a match won at a fixed
point target with a win screen that restarts on player input. The ball SHALL
bounce with an angle derived from where it strikes the paddle and SHALL gain
speed over a rally. The player paddle SHALL be controllable by keyboard and
by mouse; an imperfect AI paddle SHALL oppose the player, and a two-player
keyboard mode SHALL be available.

The sample SHALL self-play (both paddles AI-driven) from launch until the
first player input, so the gallery embed shows live gameplay without
interaction. The sample SHALL ship its font as a CC0 asset under the existing
curated sample asset-pack contract, with provenance recorded in
`gallery/samples/curated/CREDITS.md`. The sample SHALL be runnable by the
player under the same resource-root contract as any other sample, and SHALL
be covered by the gallery smoke run.

#### Scenario: Catalog contains the game sample

- **WHEN** the curated gallery catalog is read
- **THEN** it contains the Neon Pong sample under a Games category, and
  selecting it runs the sample in the application area

#### Scenario: Complete match loop

- **WHEN** the sample runs
- **THEN** it plays serve-rally-point cycles with visible score text, awards
  the match at the fixed point target, shows a win screen, and restarts a
  fresh match on player input from that screen

#### Scenario: Player controls the paddle

- **WHEN** the player moves the mouse or holds the keyboard up/down controls
  **THEN** the player paddle tracks the input, and the AI paddle opposes it
  with capped, imperfect tracking

#### Scenario: Self-play until first input

- **WHEN** the sample runs with no player input
- **THEN** both paddles are AI-driven and a live match is visible in the
  gallery embed, and the first player input hands the player paddle to the
  player

#### Scenario: Sample is portable to the player

- **WHEN** the sample's source is run by the `efx` player against its
  resource-root directory
- **THEN** it runs using only the public API and standard ES6, loading its
  font from its mounted asset pack

#### Scenario: Sample is exercised by the smoke

- **WHEN** the gallery smoke run executes
- **THEN** the Neon Pong sample is among the samples it drives, and the smoke
  fails on any console or page error from it
