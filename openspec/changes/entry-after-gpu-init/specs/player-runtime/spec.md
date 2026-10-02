# Spec Delta

## MODIFIED Requirements

### Requirement: Resource root loading
Given a path to a resource root directory, the player SHALL treat that
directory as the root of all game resources and SHALL load `main.js` from it as
the entry script. Code at the top level of `main.js` SHALL run once, after the
rendering surface is initialized and before the frame loop starts. The
rendering surface — window, GPU context, and engine pipelines — SHALL be ready
before top-level code executes, so top-level code may create or sample
engine-owned GPU resources such as `efx.graphics.whiteTexture`.

#### Scenario: Valid resource root
- **WHEN** the player is launched with a directory containing `main.js`
- **THEN** the entry script is loaded and executed

#### Scenario: Missing entry script
- **WHEN** the player is launched with a directory that does not contain
  `main.js`
- **THEN** the player prints a diagnostic and exits with a non-zero code

#### Scenario: Top-level GPU resource access
- **WHEN** the entry script reads `efx.graphics.whiteTexture` or creates a
  texture at the top level
- **THEN** the resource is available and the script runs without error

## ADDED Requirements

### Requirement: Rendering surface initialized before the entry script
In every run mode that has a rendering surface, the player SHALL initialize
that surface before evaluating the entry script's top-level code. This covers
desktop resource-root and capture modes, the optional entry script of
`--repl <root>`, and the DOM web player. The `--script` mode and the web Node
harness have no rendering surface by design; their top-level code SHALL run
without one, and this change SHALL NOT alter their behavior. In surface-bearing
modes, an uncaught top-level exception or a top-level `efx.quit` SHALL still
surface through the standard error/exit contract, even though the window has
already been created.

#### Scenario: Desktop surface before top-level code
- **WHEN** a resource-root or capture run's top-level code accesses a GPU
  resource
- **THEN** the window, GPU context, and engine pipelines already exist

#### Scenario: REPL root entry after surface init
- **WHEN** `--repl <root>` is given a root with `main.js`
- **THEN** that entry script runs after the window and GPU context exist, before
  console input is accepted

#### Scenario: DOM web surface before top-level code
- **WHEN** the DOM web player evaluates its entry script
- **THEN** WebGL is initialized and the engine pipelines are installed first

#### Scenario: Surface-less modes unchanged
- **WHEN** a `--script` run or the web Node harness evaluates its script
- **THEN** no rendering surface is created and the run behaves as before

#### Scenario: Top-level failure still exits non-zero
- **WHEN** a surface-bearing run's top-level code throws an uncaught exception
  or calls `efx.quit(3)`
- **THEN** the frame loop stops and the player exits with the contract's code
