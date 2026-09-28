# Spec Delta

## Purpose

Defines the engine's script module format and resolution: a synchronous,
resource-root-backed CommonJS runtime that lets authored code be split across
files, with module caching, `module.exports` interop, JSON modules, and a
restricted deterministic resolver shared identically by the desktop and web
runtimes.

## ADDED Requirements

### Requirement: CommonJS module format

The engine SHALL treat every script file under the resource root as a
CommonJS module. Evaluating a module SHALL expose `require`, `module`, and
`exports` to that module's scope, with `exports` initially aliasing
`module.exports`, and SHALL return the module's final `module.exports` as its
value. `require(path)` SHALL load a module synchronously and return its
`module.exports`; it SHALL NOT return a promise. The module format SHALL be
the same on every target, and no module API SHALL be a member of the `efx`
namespace or a free global outside a module's scope.

#### Scenario: Module returns its exports
- **WHEN** a module assigns a value to `module.exports` and another module
  requires it
- **THEN** the requiring module receives exactly that value

#### Scenario: Exports alias is honored
- **WHEN** a module assigns properties to `exports` without reassigning
  `module.exports`
- **THEN** those properties are present on the value returned to a requiring
  module

#### Scenario: Loading is synchronous
- **WHEN** a module calls `require(path)` at its top level
- **THEN** the dependency's exports are available immediately, with no promise
  and no loading hook

#### Scenario: Module scope is not global
- **WHEN** a script checks the global scope for `require`, `module`, or
  `exports`
- **THEN** those names are not present as free globals; they exist only inside
  a module's own scope

### Requirement: Module resolution and path rules

Module specifiers SHALL be resolved relative to the requiring module, using
forward-slash separators and the same resource-root rules as other resources.
A specifier SHALL be either relative to the requiring module (`./` or `../`)
or root-relative; resolution SHALL try the exact path and a deterministic
`.js` fallback, and SHALL load `.json` files as modules. A path that resolves
outside the resource root, a missing file, or an unsupported specifier form
SHALL fail with an error and SHALL NOT read anything outside the root. Bare
specifiers (including `node_modules` packages) and directory-index resolution
SHALL NOT be supported.

#### Scenario: Relative specifier resolves against the requiring module
- **WHEN** a module in `lib/` requires `./math.js`
- **THEN** `lib/math.js` is loaded

#### Scenario: Parent-relative specifier resolves
- **WHEN** a module in `lib/` requires `../shared/util.js`
- **THEN** `shared/util.js` relative to the root is loaded

#### Scenario: Deterministic extension fallback
- **WHEN** a module requires `./math` and only `math.js` exists
- **THEN** `math.js` is loaded

#### Scenario: Escape attempt rejected
- **WHEN** a specifier resolves outside the resource root
- **THEN** the require fails with an error and no data outside the root is read

#### Scenario: Missing module errors
- **WHEN** a module requires a path that does not exist in the root
- **THEN** the require throws and the run reports it through the standard
  error/exit-code contract

#### Scenario: Unsupported specifier rejected
- **WHEN** a module requires a bare package name or a directory without an
  index file
- **THEN** the require fails with an error rather than guessing a resolution

### Requirement: Module exports and interop semantics

The runtime SHALL implement the CommonJS/TypeScript interop contract so that
transpiled ES-module output loads correctly: a module whose `exports` carries
`__esModule` SHALL expose its `default` export to `require` interop helpers,
and the runtime SHALL support the named-import and star-import interop forms
(`__importDefault`, `__importStar`, `__exportStar`) emitted by the
TypeScript/Babel `commonjs` transform. An `__esModule` module SHALL be
distinguished from a plain object export.

#### Scenario: Default export is reachable
- **WHEN** a transpiled module sets `exports.__esModule` and `exports.default`
  and another module requires it through default-import interop
- **THEN** it receives the `default` value

#### Scenario: Named exports are reachable
- **WHEN** a transpiled module defines named exports and another module
  requires them
- **THEN** each named export is present on the returned namespace

#### Scenario: Star re-export forwards names
- **WHEN** a module re-exports another module's names
- **THEN** the forwarded names are present on the re-exporting module's
  exports

### Requirement: Module caching and circular requires

The runtime SHALL cache a module by its resolved path, so repeated `require`
calls for the same module return the same `module.exports` object and evaluate
the module body at most once. On a circular require, the runtime SHALL return
the requiring module's partially populated `exports` rather than failing or
recursing without bound.

#### Scenario: Repeated require returns the same object
- **WHEN** two modules require the same dependency
- **THEN** both receive the identical exports object and the dependency body
  ran once

#### Scenario: Circular require yields partial exports
- **WHEN** module A requires B and B requires A
- **THEN** the cycle resolves, each module receives the other's available
  exports, and no unbounded recursion occurs

### Requirement: JSON modules

A `.json` file required as a module SHALL be read as text and returned as the
parsed JSON value of its `module.exports`; a file whose contents are not valid
JSON SHALL fail with an error. JSON modules SHALL participate in the module
cache like any other module.

#### Scenario: JSON file returns parsed value
- **WHEN** a module requires `data/config.json`
- **THEN** it receives the parsed JSON value

#### Scenario: Invalid JSON errors
- **WHEN** a required `.json` file contains invalid JSON
- **THEN** the require throws an error

### Requirement: Entry module and lifecycle hooks

The entry script `main.js` SHALL be evaluated as a CommonJS module on every
target. The existing load-time sugar SHALL remain: global `update`/`render`
functions defined by the entry are registered after evaluation in load order,
after any explicitly registered hooks. In addition, the entry module's
`module.exports.update` and `module.exports.render` functions, when present,
SHALL be registered with the same ordering and semantics, so a module-shaped
entry needs no globals. A module-shaped entry and the global form SHALL NOT
both register the same hook twice.

#### Scenario: Global hooks still work
- **WHEN** `main.js` defines global `update` and `render` and exports nothing
- **THEN** both are registered after evaluation and run once per frame

#### Scenario: Exported hooks are registered
- **WHEN** `main.js` assigns `update` and `render` to `module.exports`
- **THEN** both are registered after evaluation with the same semantics as the
  global form

#### Scenario: No double registration
- **WHEN** the entry defines both a global `update` and an exported `update`
- **THEN** each hook runs once, not twice

#### Scenario: Required modules may register hooks
- **WHEN** a required module registers an update hook during evaluation
- **THEN** the hook runs once per frame in registration order

### Requirement: Module dependency restriction

Modules SHALL be bound by the same dependency restriction as all scripts: no
browser or Node.js APIs, directly or transitively. The runtime SHALL NOT
provide Node built-ins or Node globals (`fs`, `path`, `process`, `Buffer`,
`__dirname`, `global`, …), and on the web build the host-global shadowing
SHALL continue to apply to module evaluation. A module that references a
host/Node facility SHALL fail rather than silently receive it.

#### Scenario: No Node built-ins provided
- **WHEN** a module requires a Node built-in such as `fs`
- **THEN** the require fails; no Node built-in is available

#### Scenario: Host globals stay shadowed on web
- **WHEN** a web-hosted module references `window`, `document`, `process`, or
  `fetch`
- **THEN** those names are not provided, preserving the no-host-dependency
  rule

#### Scenario: Same module text on both runtimes
- **WHEN** a module that uses only the engine API and standard ES6 is run on
  desktop and on the web
- **THEN** it produces the same observable result on both

### Requirement: Module error reporting

A module evaluation error, a resolution failure, or an unsupported syntax
construct SHALL surface as a script exception through the standard error
contract: an uncaught module error SHALL stop the run, report a diagnostic
with the module's path, and exit non-zero, identically on desktop and web. The
module path SHALL appear in the diagnostic so a failing module is locatable.

#### Scenario: Module throw stops the run
- **WHEN** a required module throws during evaluation
- **THEN** the run stops, the diagnostic names the module path, and the player
  exits non-zero

#### Scenario: Resolution failure is diagnosable
- **WHEN** a module requires a missing file
- **THEN** the error names the missing specifier and the requiring module

#### Scenario: Unsupported syntax is rejected
- **WHEN** a module contains static `import`/`export`, dynamic `import()`, or
  `import.meta`
- **THEN** evaluation fails with an error rather than partially executing

#### Scenario: Parity across runtimes
- **WHEN** the same failing module is run on desktop and on the web
- **THEN** both report a failure and a non-zero exit with equivalent
  diagnostics
