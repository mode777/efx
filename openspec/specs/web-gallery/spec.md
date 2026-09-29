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
as the visual focus.

#### Scenario: Consistent retro presentation

- **WHEN** the gallery is displayed
- **THEN** the catalog, application area, and editor share the retro styled
  shell and the running application remains the visual focus

### Requirement: Static site build

The gallery SHALL build to a static bundle that includes the Emscripten web
player, the sample catalog, and the editor, and that is suitable for serving
from GitHub Pages.

#### Scenario: Built bundle is self-contained

- **WHEN** the gallery is built for deployment
- **THEN** the output contains the web player module and data, the sample
  catalog, and the editor assets needed to serve the gallery statically

### Requirement: Sample asset packs

The gallery build SHALL support an asset pack on any catalog sample, golden
or curated. When a sample ships an asset pack, the build SHALL copy that
pack into the built site and the runner SHALL mount it as the sample's
resource root before the sample's script is evaluated, so the sample's
synchronous `load*` calls resolve against it. The same pack SHALL run the
sample under the player's resource-root contract (directory or zip), so a
sample stays portable between the gallery and the player.

#### Scenario: Curated sample asset pack is mounted before the script runs

- **WHEN** a curated sample ships an asset pack and is selected in the gallery
- **THEN** the runner mounts that pack as the resource root before evaluating
  the sample, and the sample's top-level `load*` calls succeed

#### Scenario: Asset pack is portable to the player

- **WHEN** a curated sample's asset pack is used as the player's resource root
- **THEN** the sample runs with the same loaded resources as in the gallery

#### Scenario: Missing or undecodable asset surfaces

- **WHEN** a sample's script loads a resource its pack does not provide, or
  one that cannot be decoded
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
