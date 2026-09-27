# Spec Delta: player-runtime

## ADDED Requirements

### Requirement: Host-provided entry script on web

On Emscripten, when the embedding page supplies an entry-script source before
the player boots, the player SHALL execute that source in place of the
resource root's `main.js`; when no host source is supplied, the existing
resource-root `main.js` behavior SHALL be unchanged. The host channel SHALL be
consumed before the script is evaluated and MUST NOT be observable by the
entry script, which remains bound by the no-browser/host-dependency
restriction.

#### Scenario: Host source is preferred

- **WHEN** the embedding page supplies an entry-script source before the web
  player boots
- **THEN** the player evaluates and runs that source instead of loading
  `main.js` from the resource root

#### Scenario: Absent host source falls back to the resource root

- **WHEN** no host entry-script source is supplied
- **THEN** the player loads and executes `main.js` from the resource root as
  before

#### Scenario: Host channel is invisible to the script

- **WHEN** an entry script runs from a host-provided source
- **THEN** the script cannot observe the host channel during or after
  evaluation

#### Scenario: Missing script still fails

- **WHEN** neither a host source nor a resource-root `main.js` is available
- **THEN** the player reports a diagnostic and the run ends with a non-zero
  exit code
