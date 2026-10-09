# web-gallery

## Purpose

The public sample-gallery site: a browsable catalog of engine samples that
run in the browser, with editable source, so visitors can see and try the
shipped API.

## Requirements

### Requirement: Sample catalog and selection

The gallery SHALL present a selectable catalog of samples, each sample being
a runnable script, and SHALL run the selected sample in the application area.
The catalog SHALL include one entry per committed golden scene plus the
curated showcase samples.

#### Scenario: Catalog lists and runs a selected sample

- **WHEN** the visitor selects a sample from the catalog
- **THEN** that sample's script runs in the application area

#### Scenario: Catalog tracks the golden scenes

- **WHEN** a golden scene is added under `tests/goldens/`
- **THEN** the catalog contains an entry for it without a hand edit to the
  gallery

### Requirement: Isolated engine hosting per run

Each sample run SHALL execute in an isolated engine environment: starting a
run SHALL begin from clean engine state, and when a run is replaced the
previous run's engine resources and rendering context SHALL be released so
repeated runs do not accumulate state or exhaust rendering contexts.

#### Scenario: Switching samples starts clean

- **WHEN** a sample that mutates engine state is running and the visitor
  selects a different sample
- **THEN** the new sample starts from clean engine state with no residue from
  the previous sample

#### Scenario: Repeated runs release their context

- **WHEN** the visitor runs many samples in sequence in one visit
- **THEN** each finished run's rendering context is released and the gallery
  continues to run new samples

### Requirement: Editable source with run control

The gallery SHALL display the selected sample's source in an in-browser code
editor that the visitor can edit, SHALL provide a control that executes the
editor's current content as the sample, and SHALL surface a sample's script
error to the visitor without breaking the gallery shell.

#### Scenario: Edits are executed on run

- **WHEN** the visitor edits the sample source and activates the run control
- **THEN** the application area runs the edited source

#### Scenario: Reset restores the original source

- **WHEN** the visitor activates the reset control
- **THEN** the editor returns the selected sample to its original source

#### Scenario: Script error is surfaced

- **WHEN** the running sample's script throws an error
- **THEN** the gallery reports the error to the visitor and the rest of the
  gallery remains usable

### Requirement: Samples use only the public API

Sample source SHALL use only the `efx` namespace and standard ES6, with no
browser or Node.js dependencies, so a sample remains runnable by the player
under the same dependency restriction as any game script.

#### Scenario: Sample is portable to the player

- **WHEN** a catalog sample's source is run by the player against the same
  resource-root contract
- **THEN** it runs without referencing any browser or Node.js API

### Requirement: API type document

The gallery SHALL make a TypeScript declaration of the public `efx` API
available to the editor for completion and type checking. This declaration
SHALL describe current API behavior and SHALL be updated in the same change
as any script-facing API change, alongside `docs/js-api.md`.

#### Scenario: Editor completes against the API

- **WHEN** the visitor edits a sample in the editor
- **THEN** the editor offers completion and type information for the public
  `efx` API from the type document

#### Scenario: Type document tracks the API

- **WHEN** the public API changes
- **THEN** the type document is updated in that same change so it never
  describes removed or missing API surface

### Requirement: Presentation style

The gallery shell SHALL present a PlayStation-2-era visual style — a dark
palette, glowing accents, and beveled panels — with the running application
as the visual focus. The published API reference under `/api` SHALL use the
same PlayStation-2-era palette so the reference and the gallery read as one
site.

#### Scenario: Consistent retro presentation

- **WHEN** the gallery is displayed
- **THEN** the catalog, application area, and editor share the retro styled
  shell and the running application remains the visual focus

#### Scenario: Reference shares the palette

- **WHEN** the published API reference is displayed
- **THEN** it uses the same PlayStation-2-era palette as the gallery shell

### Requirement: Static site build

The gallery SHALL build to a static bundle that includes the Emscripten web
player, the sample catalog, the editor, and the generated API reference under
`/api`, and that is suitable for serving from GitHub Pages.

#### Scenario: Built bundle is self-contained

- **WHEN** the gallery is built for deployment
- **THEN** the output contains the web player module and data, the sample
  catalog, the editor assets, and the API reference HTML under `api/` needed
  to serve the gallery statically

#### Scenario: Reference is reachable from the gallery

- **WHEN** a visitor is viewing the gallery
- **THEN** a link to the API reference under `/api` is available, and the
  reference links back to the gallery

### Requirement: Sample asset packs

The gallery build SHALL support resources on any catalog sample, golden or
curated. A curated sample SHALL be authored as a self-contained directory
under `gallery/samples/curated/<name>/` containing its `main.js` entry plus
any resources it loads, so that directory is itself the sample's resource
root and the player runs the sample by pointing at it. For a curated sample
whose directory contains resources besides `main.js`, the gallery build
SHALL derive a mountable pack from the directory's contents at the archive
root, copy it into the built site, and the runner SHALL mount it as the
sample's resource root before the sample's script is evaluated, so the
sample's synchronous `load*` calls resolve against it. A curated sample
whose directory contains only `main.js` SHALL run with no pack. A golden
scene SHALL continue to supply a committed asset pack when it needs one.
The same pack SHALL run the sample under the player's resource-root contract
(directory or zip), so a sample stays portable between the gallery and the
player.

#### Scenario: Curated sample directory is the resource root

- **WHEN** the player is launched against a curated sample's directory
- **THEN** the sample's `main.js` runs with the directory's resources
  resolvable by its synchronous `load*` calls

#### Scenario: Curated sample asset pack is mounted before the script runs

- **WHEN** a curated sample whose directory contains resources besides
  `main.js` is selected in the gallery
- **THEN** the build-derived pack for that sample is mounted as the resource
  root before evaluating the sample, and the sample's top-level `load*` calls
  succeed

#### Scenario: Asset-less curated sample runs with no pack

- **WHEN** a curated sample's directory contains only `main.js`
- **THEN** the runner evaluates the sample with no pack mounted and no
  resource-pack fetch

#### Scenario: Asset pack is portable to the player

- **WHEN** a curated sample's directory (or its derived pack) is used as the
  player's resource root
- **THEN** the sample runs with the same loaded resources as in the gallery

#### Scenario: Missing or undecodable asset surfaces

- **WHEN** a sample's script loads a resource its directory does not provide,
  or one that cannot be decoded
- **THEN** the failure surfaces through the sample error channel without
  breaking the gallery shell

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

A showcase SHALL prefer procedurally generated textures built with
`createImageData` so it needs no third-party assets, or otherwise ship a CC0
asset pack under the existing sample asset-pack contract. Each particle
showcase SHALL be covered by the gallery smoke run.

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
- **THEN** it runs using only the public API and standard ES6, with textures
  either generated with `createImageData` or loaded from its mounted asset
  pack

#### Scenario: Showcase is exercised by the smoke

- **WHEN** the gallery smoke run executes
- **THEN** at least one particle showcase is among the samples it drives, and
  the smoke fails on any console or page error from it

### Requirement: Physics showcase sample

The curated gallery catalog SHALL include at least one physics showcase sample
that demonstrates the `efx.physics` API to visitors: a character moved with
`moveAndSlide` over solid static geometry (walls, a slope, and a step),
`sensor` trigger volumes detected through `overlap` or `body.contacts`, simple
falling and pushable dynamic props, and at least one query (`raycast` or
`overlap`). The sample SHALL use only the public API and standard ES6, SHALL
use no browser or Node.js dependency, and SHALL prefer procedurally generated
geometry so it needs no third-party asset pack. The sample SHALL be runnable
by the player under the same resource-root contract as any other sample.

#### Scenario: Catalog contains a physics sample

- **WHEN** the curated gallery catalog is read
- **THEN** it contains a physics showcase sample, and selecting it runs the
  sample in the application area

#### Scenario: Sample exercises the physics surface

- **WHEN** the physics showcase sample runs
- **THEN** it moves a character with `moveAndSlide` over geometry, reports a
  floor and a wall, detects a sensor, and simulates at least one dynamic body

#### Scenario: Sample is portable

- **WHEN** the sample's source is run by the player against the same
  resource-root contract
- **THEN** it runs without referencing any browser or Node.js API

### Requirement: Interactive keyboard input is exercised by the smoke run

The gallery smoke run SHALL verify that keyboard input reaches a running
sample through the embed, not only that a sample boots. It SHALL move focus
into the application area (for example by clicking the runner canvas) and
send a key event, then assert that the running sample observed the key
through `efx.keyboard`. The smoke SHALL fail when keyboard input does not
reach the running sample.

#### Scenario: Keyboard reaches the sample after interaction

- **WHEN** the smoke run focuses the application area and sends a key-down to a sample that records `efx.keyboard` state
- **THEN** the sample reports the key as down and the smoke check passes

#### Scenario: Missing keyboard delivery fails the smoke

- **WHEN** the runner does not deliver keyboard input to the running sample after the smoke focuses and sends a key
- **THEN** the smoke reports a failure and exits non-zero

### Requirement: Dropped archive runs as a sample

The gallery runner SHALL accept a zip archive dropped onto the application
area as the active sample, loading it under the same resource-root contract as
a selected sample, and SHALL suppress the browser's default handling of the
dropped file so the page is not navigated away. A dropped file that is not a
usable sample archive SHALL surface through the sample error channel without
breaking the gallery shell.

#### Scenario: Dropped archive replaces the running sample

- **WHEN** the visitor drops a zip archive containing `main.js` onto the
  application area
- **THEN** that archive is mounted as the resource root and its entry script
  runs in place of the previously selected sample

#### Scenario: Browser default is suppressed

- **WHEN** the visitor drops a file onto the application area
- **THEN** the browser does not open or download the file, and the gallery
  shell remains displayed

#### Scenario: Unusable drop is surfaced

- **WHEN** the visitor drops a file that is not a usable sample archive
- **THEN** the gallery reports the failure through the sample error channel and
  the rest of the gallery remains usable

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
and SHALL prefer a procedurally generated shadow texture built with
`createImageData` so it needs no third-party asset.

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
  shadow texture generated via `createImageData` and the sky image loaded from
  its mounted asset pack

#### Scenario: Showcases are exercised by the smoke

- **WHEN** the gallery smoke run executes
- **THEN** both showcases are among the samples it drives, and the smoke fails
  on any console or page error from them

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

### Requirement: Glow Gauntlet game sample

The curated gallery catalog SHALL include a Glow Gauntlet game sample — the
third rung of the game series, the first where gamepad input and streamed
music carry the experience — and it SHALL use only the public `efx` API and
standard ES6, with no browser or Node.js dependency.

The sample SHALL implement a complete one-button dodger: an avatar that rises
while the single control is held and sinks while it is released, scrolling
gates with gaps that must be threaded, a score of gates passed with a
session-best display, and death on gate collision that shows a game-over
screen restarting on the same button. The single control SHALL map
identically to a keyboard key, a mouse button, and a gamepad face button.
Difficulty SHALL ramp gate speed and tighten gaps as the score grows, with a
floor on gap width. Death SHALL emit an additive particle burst and a
one-shot sting, and a looping music track SHALL stream through the audio
playback bank with mute and volume controls; on the web, audio SHALL unlock
on the first input per the existing audio behavior. The sample SHALL
self-play (script-driven button) from launch until the first player input.

The sample's shipped assets (font, music loop, sting) SHALL follow the
existing curated sample asset-pack contract with provenance recorded in
`gallery/samples/curated/CREDITS.md`; the sting SHALL be synthesized
deterministically by a committed generator script with a drift check. The
sample SHALL be runnable by the player under the same resource-root contract
as any other sample, and SHALL be covered by the gallery smoke run.

#### Scenario: Catalog contains the game sample

- **WHEN** the curated gallery catalog is read
- **THEN** it contains the Glow Gauntlet sample under a Games category, and
  selecting it runs the sample in the application area

#### Scenario: One-button loop across input modes

- **WHEN** the player holds and releases the keyboard key, the mouse button,
  or a gamepad face button
- **THEN** the avatar rises while held and sinks while released in all three
  modes, and death on a gate shows a game-over screen that restarts on the
  same button

#### Scenario: Difficulty ramps within a run

- **WHEN** the passed-gate score grows during a run
- **THEN** gates scroll faster and gaps tighten toward a floor, and the
  session-best score is displayed as text

#### Scenario: Music streams with controls

- **WHEN** the sample runs
- **THEN** a looping track streams through the audio playback bank, the mute
  and volume controls act on it, and death fires a one-shot sting through the
  effect bank

#### Scenario: Self-play until first input

- **WHEN** the sample runs with no player input
- **THEN** a script-driven button plays visibly in the gallery embed, and the
  first player input takes control

#### Scenario: Assets, portability, and smoke

- **WHEN** the sample's directory and credits are read, its source is run by
  the `efx` player against its resource-root directory, or the gallery smoke
  run executes
- **THEN** it ships the standard CC0 font, a credited music loop, and a
  deterministically generated sting with provenance rows in
  `gallery/samples/curated/CREDITS.md`; it runs using only the public API and
  standard ES6; and the smoke fails on any console or page error from it

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
