# js-runtime

## Purpose

Defines the embedded ES6 script runtime and the JavaScript/C boundary: script
execution with a bundled interpreter, the mechanism by which engine functions
implemented in C are callable from JS, the dependency restrictions on scripts,
and error propagation.

## Requirements

### Requirement: Bundled ES6 execution
The player MUST execute scripts with its own bundled ES6 interpreter
(quickjs-ng) on desktop platforms. On Emscripten the browser's native JS
engine MUST drive the compiled core directly through the native bridge —
no quickjs, no embedded interpreter, and no embedded-GC machinery ships in
the wasm. Game scripts MUST NOT depend on browser or Node.js APIs, neither
directly nor transitively, so the same script text runs on both runtimes.
The entry script SHALL be evaluated as a CommonJS module on both bindings,
and module loading SHALL use one shared synchronous runtime driven by the
resource provider, so desktop and web resolve and evaluate modules with
identical semantics (the `script-modules` capability pins the format and
resolution). A module resolution or evaluation failure SHALL surface through
the standard error/exit contract on both bindings.

#### Scenario: Portable script runs identically everywhere
- **WHEN** a smoke script that uses only standard ES6 features and
  engine-exposed functions is run on all four targets
- **THEN** it produces the same observable output and exit code on each

#### Scenario: Engine-provided global namespace
- **WHEN** any script starts (desktop interpreter or browser engine)
- **THEN** the engine functions are available in a single well-known global
  namespace without any import or setup by the script

#### Scenario: No interpreter in the wasm
- **WHEN** the Emscripten player is built
- **THEN** quickjs object code is not linked and the runtime memory
  footprint contains no embedded interpreter

#### Scenario: Entry script is a module on both runtimes
- **WHEN** the entry `main.js` uses `require` to load a sibling module and
  calls an engine function from it
- **THEN** the module resolves and executes identically on desktop quickjs and
  on the browser bridge

#### Scenario: Module failure uses the exit contract
- **WHEN** a required module is missing or throws on both runtimes
- **THEN** each reports a diagnostic and exits non-zero through the same
  contract as a top-level script error

### Requirement: JS-to-C function calls
The engine SHALL expose its C-implemented functions to scripts through the
global namespace, such that a script call crosses into C, executes, and returns
a result or effect to the script. F1 SHALL expose at least: a log/print
function, a script-requested quit function taking an exit code, and a means to
read script arguments provided by the host.

#### Scenario: Log crossing
- **WHEN** a script calls the engine log function with a string
- **THEN** the string appears on the player's stdout

#### Scenario: Quit crossing
- **WHEN** a script calls the engine quit function with code 3
- **THEN** the C core terminates the run with exit code 3

#### Scenario: Host arguments readable
- **WHEN** the host launches a `--script` run with extra arguments
- **THEN** the script can read those arguments through the engine API

### Requirement: Error propagation to exit codes
An uncaught script exception MUST surface as a non-zero player exit code with
the error message on stderr, in both `--script` mode and the frame-loop hooks.
A stack or error trace SHALL be included in the diagnostic output.

#### Scenario: Exception in script mode
- **WHEN** a `--script` run throws an uncaught exception
- **THEN** the player exits non-zero and stderr contains the error message

#### Scenario: Exception in a frame hook
- **WHEN** `update` or `render` throws an uncaught exception during the frame
  loop
- **THEN** the player stops the loop, prints the error to stderr, and exits
  non-zero

### Requirement: Native bridge binding contract
On Emscripten the player SHALL expose every C-implemented engine function
through the native bridge with the same name, signature, semantics and
error behavior as the desktop binding, so that browser code and game
scripts use one API. Resource objects (`createImageData`, `createTexture`,
`efx.graphics.whiteTexture`) SHALL wrap native handles as JS objects with the
same `destroy()` semantics; the frame-end native release sweep SHALL continue
to run in C. The bridge SHALL NOT require the embedded interpreter, and
its presence MUST NOT reintroduce embedded-GC machinery into the web
build.

#### Scenario: Same call from browser code
- **WHEN** browser script code calls the bridge's `setClearColor` with a
  4-element color array and then `drawQuad` with a created texture
- **THEN** the core renders identically to the same sequence on desktop

#### Scenario: Resource lifecycle on web
- **WHEN** browser code creates a texture and calls `destroy()` on it
- **THEN** the native release follows the same deterministic/deferred
  contract as on desktop, with the browser GC managing only the JS side

#### Scenario: No embedded GC on web
- **WHEN** the web player runs its frame loop
- **THEN** no embedded-interpreter garbage collection executes (the
  segfault class is structurally absent)

### Requirement: Script clock

The runtime SHALL expose an engine-owned clock to scripts as
`efx.time.now`: the number of seconds elapsed since the script started,
advanced natively by the frame loop (never accumulated in script). The
value SHALL be snapshotted at frame start: every read within one frame —
update hooks, render hooks, input callbacks — SHALL observe the identical
value, and the value SHALL strictly increase frame over frame while frames
run. The accumulator SHALL apply an internal per-frame maximum-step clamp so
a stall (window drag, background tab, debugger pause) does not advance the
clock by more than the clamp; animation driven by `now` therefore cannot
teleport. The clock SHALL be monotonic within a run, SHALL be a plain
read-only number (assignment SHALL have no effect), and SHALL behave
identically on the desktop and web runtimes. The `efx.time` sub-namespace
SHALL carry no other members in this change.

#### Scenario: Snapshot per frame

- **WHEN** an update hook and the render hook of the same frame both read
  `efx.time.now`, and the next frame reads it again
- **THEN** the two same-frame reads are identical, and the next frame's
  value is greater

#### Scenario: Stall does not teleport the clock

- **WHEN** the frame loop stalls for a long interval (for example a
  backgrounded tab or a deliberate long sleep in the harness)
- **THEN** `now` advances by at most the internal clamp for that frame, and
  subsequent frames advance normally

#### Scenario: Read-only plain number

- **WHEN** a script reads `efx.time.now` in the first update hook and then
  assigns to it
- **THEN** the first read is approximately zero, the value remains a plain
  number on every read, and the assignment has no effect

#### Scenario: Identical across runtimes

- **WHEN** the same scripted sequence of frames runs on the desktop player
  and the web player
- **THEN** `now` follows the same start-at-zero, per-frame-snapshot,
  clamped-advance behavior on both
