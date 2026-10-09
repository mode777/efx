# Spec Delta

## ADDED Requirements

### Requirement: Bloom Breakout game sample

The curated gallery catalog SHALL include a Bloom Breakout game sample — the
second rung of the game series, composing the 2D layer with particles,
one-shot audio, and post effects as gameplay feedback — and it SHALL use only
the public `efx` API and standard ES6, with no browser or Node.js dependency.

The sample SHALL implement a complete Breakout loop: a player paddle
(mouse and keyboard), a ball, brick layouts with multi-hit rows, lives, and
score — all rendered as 2D quads with score and lives as text. Clearing a
layout SHALL advance to the next of at least three layouts; losing all lives
SHALL show a game-over screen; both end states SHALL restart on player input.
Brick destruction SHALL emit an additive particle burst, the frame SHALL
carry a bloom post-effect chain so glowing elements bloom, and paddle, brick,
wall, and life-loss events SHALL fire one-shot sound effects. The sample
SHALL self-play (script-driven paddle) from launch until the first player
input.

The sample's shipped assets (font and short WAV effects) SHALL follow the
existing curated sample asset-pack contract: the font SHALL be the standard
CC0 Kenney font, the effect WAVs SHALL be synthesized deterministically by a
committed generator script with a drift check, and provenance SHALL be
recorded in `gallery/samples/curated/CREDITS.md`. The sample SHALL be
runnable by the player under the same resource-root contract as any other
sample, and SHALL be covered by the gallery smoke run.

#### Scenario: Catalog contains the game sample

- **WHEN** the curated gallery catalog is read
- **THEN** it contains the Bloom Breakout sample under a Games category, and
  selecting it runs the sample in the application area

#### Scenario: Complete game loop

- **WHEN** the sample runs
- **THEN** it plays rally-lives-layout cycles: bricks are destroyed by the
  ball, clearing a layout advances to the next, losing all lives shows a game
  over, clearing the final layout shows a win screen, and both end states
  restart on player input

#### Scenario: Destruction feedback composes particles, post, and audio

- **WHEN** the ball destroys a brick during play
- **THEN** an additive particle burst emits at the brick, the frame's bloom
  chain glows the burst and neon bricks, and a one-shot sound effect fires

#### Scenario: Self-play until first input

- **WHEN** the sample runs with no player input
- **THEN** a script-driven paddle plays visibly in the gallery embed, and the
  first player input hands the paddle to the player

#### Scenario: Assets follow the pack and credits contract

- **WHEN** the sample's directory and credits are read
- **THEN** it ships the standard CC0 font plus deterministically generated
  effect WAVs, each with a provenance row in
  `gallery/samples/curated/CREDITS.md`, and regenerating the WAVs with the
  committed generator reproduces them byte-for-byte

#### Scenario: Sample is portable and smoke-covered

- **WHEN** the sample's source is run by the `efx` player against its
  resource-root directory, or the gallery smoke run executes
- **THEN** it runs using only the public API and standard ES6, and the smoke
  fails on any console or page error from it
