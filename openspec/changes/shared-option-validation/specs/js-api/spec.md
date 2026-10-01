## MODIFIED Requirements

### Requirement: Two-layer API with strict layering
The script API SHALL consist of exactly two layers: low/mid-level functions
implemented in C/C++ and registered through the engine binding, and
high-level convenience functions implemented in pure ES6. High-level
functions MUST be implemented using only the public low/mid-level API and
standard ES6 built-ins — they MUST NOT use private bindings or host
facilities that are not part of the public API. Every API function in the
reference SHALL be tagged with its layer.

A low/mid-level function MAY validate and normalize its arguments in the
engine-bundled pure-ES6 layer before its C/C++ implementation runs. This
argument handling SHALL be written once and shared by every runtime, and it
SHALL reach the native implementation only through an engine-internal binding
object that is neither a member of the `efx` namespace nor a global. Such a
function remains a low/mid-level, C-implemented function: its observable
behavior, layer tag and reference entry are unchanged, and the shared argument
handling SHALL NOT be callable by scripts on its own.

#### Scenario: High-level function built on public API
- **WHEN** a high-level convenience function (e.g. a model or text drawer) is
  implemented
- **THEN** it calls only documented public API functions and standard ES6

#### Scenario: Layer tag present
- **WHEN** a function entry is read in the API reference
- **THEN** its entry marks it as either C-implemented or pure-JS

#### Scenario: Shared argument handling stays private
- **WHEN** a script enumerates the `efx` namespace and the global scope
- **THEN** no engine-internal binding object or argument validator used by a
  C-implemented function is reachable from either

#### Scenario: Shared validation keeps the layer tag
- **WHEN** a C-implemented function validates its option object in the shared
  pure-ES6 layer before entering native code
- **THEN** its API reference entry is still tagged C-implemented and its
  documented signature, defaults and errors are unchanged

## ADDED Requirements

### Requirement: Identical argument errors on every runtime
For every engine API call that throws because an argument is invalid, or
because a requested resource cannot be read or decoded, the desktop runtime
and the web runtime SHALL throw an error of the same class with the identical
message text. A number-typed argument or option field SHALL accept only values
whose type is number. Any other type — including strings that contain digits
and booleans — SHALL throw `TypeError`. Non-finite numbers and out-of-range
values keep their documented error class.

#### Scenario: Same message on both runtimes
- **WHEN** the same script performs an invalid call (for example
  `efx.setLight(7, { … })` with a slot outside 0..3) on the desktop player and
  in the web player
- **THEN** both throw an error of the same class whose `message` is
  byte-identical

#### Scenario: Missing resource reports identically
- **WHEN** a script loads an image, glTF asset or audio file that does not
  exist in the resource root, on each runtime
- **THEN** both runtimes throw the same error class with the identical message

#### Scenario: Numeric string is rejected everywhere
- **WHEN** a script passes the string `'0.5'` where a number-typed option is
  expected (for example a physics body's `mass`)
- **THEN** both runtimes throw `TypeError` and no resource is created

#### Scenario: Error catalog has no known divergences
- **WHEN** the error-catalog script is run through both runtimes and the
  outputs are compared
- **THEN** the outputs are byte-identical and the catalog lists no known
  divergences
