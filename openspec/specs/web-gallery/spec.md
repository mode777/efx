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
