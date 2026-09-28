# Spec Delta

## MODIFIED Requirements

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
