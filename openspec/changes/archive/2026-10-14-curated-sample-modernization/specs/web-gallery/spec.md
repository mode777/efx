# Spec Delta: web-gallery

## MODIFIED Requirements

### Requirement: Particle system showcase samples

The curated gallery SHALL include at least one particle showcase sample that
demonstrates the particle system's defining features to visitors, and those
samples SHALL use only the public `efx` API and standard ES6. Together the
particle showcase(s) SHALL demonstrate:

- world-space emission and simulation (`space: 'world'`), including a burst
  (`emit`) and continuous emission;
- at least two quad render modes — camera-facing billboards (`facing: 'view'`
  or `'y'`) and fixed oriented planes (`facing: 'plane'`, e.g. a water
  surface);
- both additive and alpha (or subtractive) blending;
- lifetime-interpolated size and/or color, and at least one showcase MAY use
  atlas frames (`quads`).

A showcase SHALL use the shared radial glow texture asset (see the Shared
radial glow texture asset requirement) for its radial sprite textures rather
than building the falloff in script, and MAY generate any other texture with
`createImageData`; it SHALL ship no third-party assets beyond the curated
sample asset-pack contract. Each particle showcase SHALL be covered by the
gallery smoke run.

#### Scenario: Showcase demonstrates the render modes

- **WHEN** a visitor selects a particle showcase
- **THEN** it renders particles through the public `createParticleSystem` /
  `drawParticles` API and demonstrates camera-facing billboards and fixed
  oriented planes

#### Scenario: Showcase demonstrates blending and emission styles

- **WHEN** the particle showcase runs
- **THEN** it demonstrates an additive effect and an alpha/subtractive effect,
  and both a burst and continuously emitted particles

#### Scenario: Showcase is portable to the player

- **WHEN** a particle showcase's source is run by the `efx` player against the
  same resource-root contract
- **THEN** it runs using only the public API and standard ES6, with its radial
  texture loaded from its mounted asset pack and any other texture generated
  with `createImageData`

#### Scenario: Showcase is exercised by the smoke

- **WHEN** the gallery smoke run executes
- **THEN** at least one particle showcase is among the samples it drives, and
  the smoke fails on any console or page error from it

### Requirement: Skybox and blob-shadow showcase samples

The curated gallery catalog SHALL include a skybox showcase sample and a
blob-shadow showcase sample that demonstrate these effects to visitors using
only the public `efx` API and standard ES6, with no browser or Node.js
dependency.

The skybox showcase SHALL render an inward-facing primitive (`makeSphere` or
`makeCube` with `inverted: true`) carrying an `unlit` material whose `diffuse`
channel samples a single equirectangular sky image, drawn camera-locked and
with `depthWrite: false` before a small lit scene, so the sky reads as a
background that does not occlude the scene. The sky image SHALL be shipped as
a CC0 asset under the existing curated sample asset-pack contract, with its
provenance and downscale recipe recorded in `gallery/samples/curated/CREDITS.md`.

The blob-shadow showcase SHALL draw a radial shadow as a `facing: 'plane'`
billboard placed a small distance above a ground plane, with a dark color and
alpha blending, following a moving character mesh under an orbiting 3D camera,
and SHALL use the shared radial glow texture asset (see the Shared radial glow
texture asset requirement) as its shadow texture, shipped under the curated
sample asset-pack contract.

Each showcase SHALL be runnable by the player under the same resource-root
contract as any other sample, and each SHALL be covered by the gallery smoke
run.

#### Scenario: Catalog contains both showcases

- **WHEN** the curated gallery catalog is read
- **THEN** it contains a skybox showcase and a blob-shadow showcase, and
  selecting either runs the sample in the application area

#### Scenario: Skybox showcase draws an unlit, non-occluding sky

- **WHEN** the skybox showcase runs
- **THEN** it draws an inverted unlit mesh with a sky texture and
  `depthWrite: false`, the sky fills the background, and scene geometry is
  drawn over it

#### Scenario: Blob shadow stays flat under an orbiting camera

- **WHEN** the blob-shadow showcase runs and the camera orbits the scene
- **THEN** the shadow remains a flat decal on the ground plane and follows the
  moving character, independent of the camera's facing

#### Scenario: Showcases are portable to the player

- **WHEN** either showcase's source is run by the `efx` player against the
  same resource-root contract
- **THEN** it runs using only the public API and standard ES6, with the blob
  shadow texture and the sky image loaded from its mounted asset pack

#### Scenario: Showcases are exercised by the smoke

- **WHEN** the gallery smoke run executes
- **THEN** both showcases are among the samples it drives, and the smoke fails
  on any console or page error from them

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
procedurally generated course texture (via `efx.noise`) and a blob shadow
drawn with the shared radial glow texture asset. The sample SHALL self-play
(script-driven aim and strokes) from launch until the first player input.

The sample's shipped assets SHALL be the standard CC0 font and the shared
radial glow image under the existing curated sample asset-pack contract, with
provenance recorded in `gallery/samples/curated/CREDITS.md`. The sample SHALL
be runnable by the player under the same resource-root contract as any other
sample, and SHALL be covered by the gallery smoke run.

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
  walls contain the ball within the course, and the only loaded assets are the
  shipped font and radial glow image

#### Scenario: Self-play, portability, and smoke

- **WHEN** the sample runs with no player input, its source is run by the
  `efx` player against its resource-root directory, or the gallery smoke run
  executes
- **THEN** script-driven strokes play visibly until the first player input;
  it runs using only the public API and standard ES6; and the smoke fails on
  any console or page error from it

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
procedural primitives; glow and particle decals SHALL use the shared radial
glow texture asset. The sample SHALL self-play (script-driven twin-stick
input) from launch until the first player input.

The sample's shipped assets (font, the shared radial glow image, and a
synthesized SFX/music pack) SHALL follow the existing curated sample
asset-pack contract with provenance recorded in
`gallery/samples/curated/CREDITS.md`; effect WAVs SHALL be synthesized
deterministically by a committed generator script with a drift check. The
sample SHALL be runnable by the player under the same resource-root contract
as any other sample, and SHALL be covered by the gallery smoke run.

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
- **THEN** it ships the standard CC0 font plus the shared radial glow image
  and deterministically generated effect WAVs and a credited music loop with
  provenance rows in `gallery/samples/curated/CREDITS.md`; it runs using only
  the public API and standard ES6; and the smoke fails on any console or page
  error from it

## ADDED Requirements

### Requirement: Shared radial glow texture asset

Curated samples that need a radial glow or shadow texture SHALL load one
canonical pre-built image instead of building the falloff in script: a CC0
`radial.png` — white RGB with an alpha falloff over a normalized disc —
generated deterministically by a committed script under `gallery/scripts/`,
shipped at each consuming sample's resource root, loaded with `loadImage` +
`createTexture`, and tinted at draw time. Every consuming sample's directory
SHALL contain the file, and `gallery/samples/curated/CREDITS.md` SHALL record
it for that sample with its generator and license. No curated sample SHALL
hand-roll a radial-falloff pixel loop.

#### Scenario: Sample loads the shared radial image

- **WHEN** a curated sample that draws a radial glow or shadow is run by the
  player against its resource-root directory
- **THEN** it loads `radial.png` from its root and draws with it, with no
  script-side falloff loop

#### Scenario: Asset is recorded with provenance

- **WHEN** `gallery/samples/curated/CREDITS.md` is read
- **THEN** every sample directory that ships `radial.png` has a row naming the
  file, its in-repo generator script, and its CC0 license

#### Scenario: Generated bytes are reproducible

- **WHEN** the committed generator script is run
- **THEN** it regenerates the shipped `radial.png` byte-for-byte (a drift
  check), so the asset's provenance is reproducible from the repository
