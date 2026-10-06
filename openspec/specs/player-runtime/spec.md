# player-runtime

## Purpose

Defines the player executable's observable behavior: loading a resource root,
picking up the `main.js` entry script and its lifecycle hooks, the window and
frame loop, the `--script` run mode, and the exit-code contract.

## Requirements

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

### Requirement: Lifecycle hooks
The player SHALL drive per-frame callbacks through explicit hook registration.
`efx.registerUpdateHook(fn)` and `efx.registerRenderHook(fn)` SHALL stack hooks
in registration order; each frame SHALL run all update hooks, each receiving
`dt` (seconds since the previous frame), before all render hooks, and each
registration SHALL return an unsubscribe function that removes it and is
idempotent. The global `update` and `render` functions defined by the entry
script SHALL be registered as load-time sugar in load order once the script
finishes evaluating, after any hooks registered during evaluation; a script
that defines neither global hooks nor registrations SHALL run the frame loop
without error. An uncaught exception in any hook SHALL stop the frame loop and
exit with a non-zero code.

#### Scenario: Hooks invoked per frame
- **WHEN** the entry script registers update and render hooks (or defines
  global `update`/`render`)
- **THEN** all update hooks run before all render hooks once per frame, each
  update hook receiving `dt`

#### Scenario: Absent hook tolerated
- **WHEN** the entry script defines neither `update`/`render` nor any
  registration
- **THEN** the player runs the frame loop without error

#### Scenario: Unsubscribe removes a hook
- **WHEN** a script calls the unsubscribe function returned by a registration
- **THEN** that hook no longer runs on later frames, and calling unsubscribe
  again has no effect

#### Scenario: Global hooks remain load-time sugar
- **WHEN** the entry script defines global `update`/`render` and registers no
  explicit hooks
- **THEN** the player registers them after evaluation in load order, so F1
  scripts keep working unchanged

#### Scenario: Hook exception stops the loop
- **WHEN** any registered hook throws an uncaught exception
- **THEN** the frame loop stops, the error is printed, and the player exits
  non-zero

### Requirement: Window and frame loop
In resource-root mode the player SHALL open a platform window with a
renderer-defined clear color and SHALL run the frame loop until the window is
closed or the script requests termination.

#### Scenario: Window closed by user
- **WHEN** the user closes the window during the frame loop
- **THEN** the player shuts down cleanly and exits with code 0

#### Scenario: Script requests termination
- **WHEN** the entry script calls the engine's quit function with an exit code
- **THEN** the player stops the frame loop and exits with that code

### Requirement: Script run mode
The player SHALL support a `--script <path>` run mode that executes a single
script file without opening a window and exits as soon as the script finishes.
This mode is the vehicle for automated smoke tests.

#### Scenario: Headless script execution
- **WHEN** the player is launched with `--script` and a valid script path
- **THEN** no window is opened, the script runs to completion, and the player
  exits

### Requirement: Exit-code contract
The player SHALL exit 0 when the run completes successfully and a non-zero code
when it fails. Failure cases MUST include: missing resource root or entry
script, script file not found in `--script` mode, and an uncaught script
error. Diagnostics for failures SHALL be printed to stderr.

#### Scenario: Script exit code propagation
- **WHEN** a `--script` run ends by requesting exit code 3
- **THEN** the player process exits with code 3

#### Scenario: Script not found
- **WHEN** the player is launched with `--script` and a path that does not exist
- **THEN** the player prints a diagnostic to stderr and exits with a non-zero
  code

#### Scenario: Uncaught script error
- **WHEN** a script run ends with an uncaught script exception
- **THEN** the player prints the error to stderr and exits with a non-zero code

### Requirement: Web entry script and native lifecycle
On Emscripten, in resource-root mode, the player SHALL load `main.js` from
the resource root, execute it with the browser's native JS engine, and
wire the same global `update`/`render` lifecycle hooks (and explicit hook
registration) once per frame, `update` before `render`, matching the
desktop contract. An uncaught error in the entry script or a hook SHALL
stop the frame loop and surface through the same non-zero exit-code
contract (message on the console/error channel). A missing `main.js`
SHALL produce a diagnostic and the error exit code.

#### Scenario: Browser executes the entry script
- **WHEN** the web player is launched with a resource root whose `main.js`
  defines `update` and `render`
- **THEN** the browser engine executes the entry script once before the
  frame loop and calls the hooks once per rendered frame in order

#### Scenario: Web hook error surfaces
- **WHEN** the entry script or a hook throws an uncaught exception
- **THEN** the frame loop stops, the error is reported on the error
  channel, and the run ends with the failure exit code

#### Scenario: Missing entry script on web
- **WHEN** the web player is launched with a resource root without
  `main.js`
- **THEN** a diagnostic is reported and the run ends with a non-zero exit
  code

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

### Requirement: Resource root launch accepts a directory or an archive

In resource-root mode the player SHALL accept either a directory or a zip
archive as the resource root, read `main.js` from that root as the entry
script, and fail with a diagnostic and a non-zero exit when the root is
missing, unreadable, or lacks `main.js`. The engine-level behavior of reading
resources from a directory or archive is specified by the `resource-loading`
capability.

#### Scenario: Launch with a directory root
- **WHEN** the player is launched with a directory containing `main.js`
- **THEN** the entry script runs as before

#### Scenario: Launch with an archive root
- **WHEN** the player is launched with a zip archive containing `main.js`
- **THEN** the entry script runs and resource loads resolve inside the archive

#### Scenario: Bad root
- **WHEN** the player is launched with a missing root, a corrupt archive, or a
  root without `main.js`
- **THEN** it prints a diagnostic and exits non-zero

### Requirement: Script run mode resource root

In `--script <file>` mode the player SHALL use the script file's directory as
the resource root, so that `load*` calls in the script resolve relative to it.
The player SHALL also accept an explicit `--root <directory|archive>` override
for this mode; when present it takes precedence over the script's directory. A
missing or unreadable script file SHALL still fail with a diagnostic and a
non-zero exit.

#### Scenario: Script loads adjacent resources
- **WHEN** a `--script` run loads a resource that sits next to the script file
- **THEN** the load succeeds without any additional root argument

#### Scenario: Explicit root override
- **WHEN** a `--script` run passes `--root` naming a directory or archive
- **THEN** `load*` calls resolve against that root instead of the script's
  directory

#### Scenario: Missing script still fails
- **WHEN** `--script` names a path that does not exist
- **THEN** the player prints a diagnostic and exits non-zero

### Requirement: Web resource boot before the entry script

On Emscripten the player SHALL complete any host-provided asset-root mount
before evaluating the entry script, so that top-level and hook `load*` calls
are synchronous. When no asset root is supplied the existing preloaded-root
behavior SHALL be unchanged, and the Node harness paths SHALL continue to use
a filesystem root without fetching. The host asset channel and its
invisibility to scripts are defined by the `resource-loading` capability.

#### Scenario: Web boots after mount
- **WHEN** the web player has a host-provided asset root
- **THEN** `main.js` is evaluated only after the asset root is available

#### Scenario: Node harness unchanged
- **WHEN** the web player runs under Node with a filesystem resource root and
  no asset-root URL
- **THEN** no fetch occurs and resource loads read the filesystem as before

### Requirement: Interactive console run mode launch

In addition to resource-root and `--script` modes, the desktop player SHALL
support `--repl [<root>]`, which opens the normal window and frame loop and
drives the REPL on stdin. When a root is supplied but is missing or unreadable,
the player SHALL print a diagnostic and exit non-zero; with no root it SHALL
start with the bare namespace. The run SHALL follow the standard exit-code
contract. The mode is desktop (embedded-runtime) only.

#### Scenario: Launch with a root
- **WHEN** the player is launched as `--repl <root>`
- **THEN** it opens the window, sets the root, and accepts console input

#### Scenario: Launch bare
- **WHEN** the player is launched as `--repl`
- **THEN** it opens the window and accepts console input with no resource root

#### Scenario: Bad root
- **WHEN** `--repl` names a missing or unreadable root
- **THEN** the player prints a diagnostic and exits non-zero

### Requirement: Dropped resource root loads the game

The player SHALL treat a resource root dropped onto its window (desktop) or
canvas (web) as a request to load and run that game, with the same entry-script
and resource-resolution semantics as a root supplied on the command line. On
desktop the dropped root SHALL be accepted whether it is a zip archive or a
directory; on web the dropped root SHALL be a zip archive, because browsers do
not expose a dropped directory as a filesystem root. The dropped game's
`main.js` SHALL be read from the dropped root and its `load*` calls SHALL
resolve inside it. Dropping is a host-level action: it SHALL expose no
script-facing API, no loading hook, and no asynchronous `load*`, and game
scripts SHALL remain bound by the no-browser/host-dependency restriction. A
drop that is not a usable resource root SHALL surface a diagnostic and SHALL
NOT replace a game that is currently running.

#### Scenario: Dropped archive on desktop

- **WHEN** the user drops a zip archive containing `main.js` onto the desktop
  player window
- **THEN** the archive's entry script is loaded and run, and its resource loads
  resolve inside the archive

#### Scenario: Dropped directory on desktop

- **WHEN** the user drops a directory containing `main.js` onto the desktop
  player window
- **THEN** the directory's entry script is loaded and run, and its resource
  loads resolve inside the directory

#### Scenario: Dropped archive on web

- **WHEN** the user drops a zip archive containing `main.js` onto the web
  player's canvas
- **THEN** the archive is mounted as the resource root before the entry script
  runs, and the entry script's top-level `load*` calls succeed synchronously

#### Scenario: Unusable drop is rejected

- **WHEN** the user drops a root that is not a readable zip archive or
  directory, or one without `main.js`
- **THEN** the player reports a diagnostic and a game that was already running
  continues to run

#### Scenario: No script-visible drop API

- **WHEN** a game runs and a root is dropped onto the player
- **THEN** the running or newly loaded script observes no drop callback, no
  host channel, and no asynchronous resource API

### Requirement: In-place game swap

On desktop, the player SHALL be able to replace the active game with a newly
supplied resource root in place, without restarting the process or recreating
the platform window and rendering context. A swap SHALL release the previous
game's script session and engine state — script context and registered hooks,
GPU resources, input, physics, particles, and audio — before the new game
loads, so the new game starts from clean engine state, and SHALL then read the
new root's `main.js` and run its entry. Repeated swaps SHALL NOT accumulate
state or exhaust rendering resources. A swap to a root that is missing,
unreadable, or lacks `main.js` SHALL surface a diagnostic and SHALL leave the
current game running.

#### Scenario: Swap keeps the window and context

- **WHEN** a game is running and a new valid resource root is supplied
- **THEN** the platform window and rendering context are not recreated, and
  the new game's entry script runs in the same window

#### Scenario: Swap starts clean

- **WHEN** a game that mutated engine state is swapped for another game
- **THEN** the new game starts from clean engine state with no residue from the
  previous game

#### Scenario: Repeated swaps release resources

- **WHEN** many games are swapped in sequence in one run
- **THEN** each replaced game's resources are released and later games continue
  to run without accumulating state or exhausting rendering resources

#### Scenario: Unusable swap leaves the current game running

- **WHEN** a swap is requested with a root that is missing, unreadable, or
  lacks `main.js`
- **THEN** the player reports a diagnostic and the game that was running
  continues to run

### Requirement: Resources without a rendering surface
In run modes that have no rendering surface — the desktop `--script` mode and
the web Node harness — the player SHALL still allow scripts to create and query
engine resources. Texture, mesh, render-target, font, and particle-system
creation SHALL succeed; a created resource SHALL report its size and SHALL be
accepted by recorded draws, while nothing is uploaded to a GPU or rendered.
Engine-owned resources, including `efx.graphics.whiteTexture`, SHALL be
available in these modes exactly as in surface-bearing modes. No rendering
surface SHALL be initialized in these modes.

#### Scenario: Create and query a texture without a surface
- **WHEN** a `--script` run creates a texture from an ImageData and reads its
  `width` and `height`
- **THEN** the call succeeds and the values match the ImageData

#### Scenario: Engine-owned resource is available without a surface
- **WHEN** a `--script` run reads `efx.graphics.whiteTexture`
- **THEN** it receives a valid 1×1 Texture usable in a recorded draw

#### Scenario: Draws are recorded but not rendered
- **WHEN** a `--script` run draws a quad or mesh
- **THEN** the draw is accepted and recorded with no rendering surface present

#### Scenario: No surface is created
- **WHEN** the `--script` mode or the web Node harness runs
- **THEN** no window, GPU context, or engine pipelines are initialized

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
