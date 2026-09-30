# Spec Delta

## MODIFIED Requirements

### Requirement: Downloadable per-target build artifacts

Every gate run SHALL package the built player for each supported target
into a named archive and publish it as a downloadable workflow artifact:
a native executable archive for Windows, Linux, and macOS, and a bundle
archive containing the Emscripten web player (HTML loader, JS glue, wasm,
and data) for Emscripten. Every gate run SHALL also build a curated-samples
archive from the curated sample directories — one `<name>/` folder per
curated sample containing that sample's `main.js` and resources — and
publish it as a downloadable workflow artifact. On a tag run, the same
archives SHALL additionally be attached as downloadable assets of the
GitHub Release for that tag.

#### Scenario: Every run publishes downloadable archives

- **WHEN** a gate run completes, whether triggered by a tag or manually
- **THEN** a downloadable workflow artifact exists for each of the four
  targets and for the curated samples

#### Scenario: Tag run attaches archives to the release

- **WHEN** the gate runs for a version tag
- **THEN** the four target archives and the curated-samples archive are
  attached as assets of the GitHub Release for that tag

#### Scenario: Curated-samples archive is player-runnable

- **WHEN** the curated-samples archive is extracted
- **THEN** each top-level `<name>/` folder contains a `main.js` and that
  sample's resources, and the player runs the sample from the folder
