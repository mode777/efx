# Spec Delta

## ADDED Requirements

### Requirement: Bot Arena game sample

The curated gallery catalog SHALL include a Bot Arena game sample — the
capstone of the game series, composing 3D rendering, lighting, post effects,
particles, physics queries, text, keyboard/mouse/gamepad input, and audio in
one wave-based twin-stick arena — and it SHALL use only the public `efx` API
and standard ES6, with no browser or Node.js dependency.

The sample SHALL implement a complete twin-stick loop: a player moved with
the character-controller API and aimed with the mouse or the gamepad's second
stick; enemy waves spawned by an escalating wave state machine; enemy bots as
dynamic bodies that seek the player and take knockback impulses when hit;
visible projectiles with hit feedback; player health with a game-over screen
that restarts on input; and score, wave, and health displayed as text. Enemy
shots SHALL respect wall cover through a physics line-of-sight query. The
fixed bank of point lights SHALL be spent on the loudest recent muzzle flashes
and explosions within the engine's fixed light budget, the frame SHALL carry
a bloom post-effect chain, and enemy deaths and hits SHALL emit particle
bursts with hit flashes and screen shake. Sound effects SHALL fire through
the one-shot effect bank and a looping music track SHALL stream through the
playback bank with mute and volume controls; on the web, audio SHALL unlock
on the first input. The arena, its walls and cover, and all actors SHALL be
procedural primitives with procedurally generated textures. The sample SHALL
self-play (script-driven twin-stick input) from launch until the first
player input.

The sample's shipped assets (font and a synthesized SFX/music pack) SHALL
follow the existing curated sample asset-pack contract with provenance
recorded in `gallery/samples/curated/CREDITS.md`; effect WAVs SHALL be
synthesized deterministically by a committed generator script with a drift
check. The sample SHALL be runnable by the player under the same
resource-root contract as any other sample, and SHALL be covered by the
gallery smoke run.

#### Scenario: Catalog contains the game sample

- **WHEN** the curated gallery catalog is read
- **THEN** it contains the Bot Arena sample under a Games category, and
  selecting it runs the sample in the application area

#### Scenario: Complete wave loop

- **WHEN** the sample runs
- **THEN** waves of enemies spawn, escalate, and attack; the player's health
  depletes on hits; losing all health shows a game-over screen that restarts
  on player input; and score and wave are displayed as text

#### Scenario: Twin-stick input across keyboard/mouse and gamepad

- **WHEN** the player uses WASD with mouse aim and fire, or both gamepad
  sticks
- **THEN** movement and aiming behave equivalently across both control
  schemes

#### Scenario: Enemies are physical and knock back

- **WHEN** a shot hits an enemy bot
- **THEN** the bot — a dynamic body — visibly recoils from an applied
  impulse, and enemy shots respect wall cover through a line-of-sight query

#### Scenario: Light budget spent on combat

- **WHEN** shots are fired and enemies explode
- **THEN** the loudest recent muzzle flashes and explosions hold the
  available point-light slots within the engine's fixed budget, and the
  bloom chain glows them

#### Scenario: Self-play until first input

- **WHEN** the sample runs with no player input
- **THEN** script-driven twin-stick input plays visibly in the gallery embed,
  and the first player input takes control

#### Scenario: Assets, portability, and smoke

- **WHEN** the sample's directory and credits are read, its source is run by
  the `efx` player against its resource-root directory, or the gallery smoke
  run executes
- **THEN** it ships the standard CC0 font plus deterministically generated
  effect WAVs and a credited music loop with provenance rows in
  `gallery/samples/curated/CREDITS.md`; it runs using only the public API and
  standard ES6; and the smoke fails on any console or page error from it
