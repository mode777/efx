# Spec Delta

## MODIFIED Requirements

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
