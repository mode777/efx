# Spec Delta

## ADDED Requirements

### Requirement: Sky Steps game sample

The curated gallery catalog SHALL include a Sky Steps game sample — the fifth
rung of the game series, putting the character controller, sensors, and
skinned glTF animation inside a playable platformer — and it SHALL use only
the public `efx` API and standard ES6, with no browser or Node.js dependency.

The sample SHALL implement a complete collect-them-all platformer: a capsule
character moved with the character-controller API (step height, floor snap,
move-and-slide) with script-owned gravity and jump; a floating platform
course of static boxes and ramps over a void; star collectibles and
checkpoints detected as sensor volumes; a respawn at the last checkpoint when
the character falls below the course; and a finish sensor that ends the round
with a time result once every star is collected. The character SHALL be a
CC0 rigged glTF model drawn skinned with animation clips selected by speed
and airborne state, beneath a third-person follow camera controllable by
mouse drag, with a blob shadow under the character and a camera-locked unlit
skybox backdrop. Timer and collected-star text SHALL be displayed, and the
finish screen SHALL restart on player input. The sample SHALL self-play
(script-driven movement) from launch until the first player input.

The sample's shipped assets (font, rigged character, equirectangular sky
image) SHALL be CC0 under the existing curated sample asset-pack contract,
with provenance and recipes recorded in
`gallery/samples/curated/CREDITS.md`. The sample SHALL be runnable by the
player under the same resource-root contract as any other sample, and SHALL
be covered by the gallery smoke run.

#### Scenario: Catalog contains the game sample

- **WHEN** the curated gallery catalog is read
- **THEN** it contains the Sky Steps sample under a Games category, and
  selecting it runs the sample in the application area

#### Scenario: Complete platformer loop

- **WHEN** the sample runs
- **THEN** the character runs and jumps across the course with keyboard (and
  gamepad) input, stars are collected and counted in text, falling respawns
  at the last checkpoint, and reaching the finish with every star ends the
  round in a time-result screen that restarts on player input

#### Scenario: Character movement uses the controller surface

- **WHEN** the character moves and jumps
- **THEN** it moves through the character-controller API with script-owned
  gravity, snaps to floors, climbs step-height ledges, and slides along walls
  and ramps

#### Scenario: Skinned character under a follow camera

- **WHEN** the character moves, idles, or is airborne
- **THEN** the rigged model is drawn skinned with a clip chosen by speed and
  airborne state, the mouse-dragged follow camera orbits it, a flat blob
  shadow stays under it, and the skybox fills the background without
  occluding the course

#### Scenario: Self-play until first input

- **WHEN** the sample runs with no player input
- **THEN** script-driven movement plays visibly in the gallery embed, and the
  first player input takes control

#### Scenario: Assets, portability, and smoke

- **WHEN** the sample's directory and credits are read, its source is run by
  the `efx` player against its resource-root directory, or the gallery smoke
  run executes
- **THEN** it ships the standard CC0 font, the CC0 rigged character with its
  material recipe, and the CC0 sky image, each with a provenance row in
  `gallery/samples/curated/CREDITS.md`; it runs using only the public API and
  standard ES6; and the smoke fails on any console or page error from it
