# repl Specification

## Purpose
Defines the interactive console run mode: evaluating JavaScript entered on
stdin in the player's persistent script context while the normal frame loop
runs, so the `efx` namespace can be driven by hand for authoring and testing.

## Requirements

### Requirement: Interactive console run mode

On desktop (the embedded-runtime player), `--repl [<root>]` SHALL open the
normal window and run the frame loop while reading JavaScript from stdin one
line at a time and evaluating each line as a global script snippet in the same
persistent context the entry script would use. State (variables, bound
resources, registered hooks) SHALL persist across entered lines. The mode is
not available on Emscripten.

#### Scenario: Enter and evaluate
- **WHEN** a line of JavaScript is entered
- **THEN** it is evaluated in the persistent context and its completion value
  is printed when it is not `undefined`

#### Scenario: State persists
- **WHEN** one line defines `let x = 2` and a later line evaluates `x + 3`
- **THEN** the later line produces `5`

#### Scenario: Not available on web
- **WHEN** the Emscripten player is asked for REPL mode
- **THEN** it reports that the mode is unavailable rather than silently
  ignoring it

### Requirement: Optional resource root and interactive loading

When `--repl` names a resource root, the player SHALL set it as the resource
root and, if the root contains `main.js`, evaluate that entry script before
accepting input; otherwise it SHALL start with the bare namespace and the given
root. The resource-loading functions (`efx.io.loadText`, `efx.io.loadData`,
`efx.graphics.loadImage`, glTF import) SHALL be usable from entered lines and
resolve against that root.

#### Scenario: Root with entry script
- **WHEN** `--repl <root>` is given and the root has `main.js`
- **THEN** the entry script runs first and its scene state is interactive

#### Scenario: Load from the REPL
- **WHEN** a line calls a load function for a resource in the root
- **THEN** it returns the loaded resource as in a script

#### Scenario: Bare namespace
- **WHEN** `--repl` is given with no root
- **THEN** the namespace is available with no resource root

### Requirement: Host commands

The REPL SHALL handle `.help` and `.exit` as host commands before evaluating
input; they SHALL NOT be exposed as `efx` functions, and the run SHALL add no
script-facing API. `.help` SHALL print the available host commands.

#### Scenario: Help does not evaluate as script
- **WHEN** `.help` is entered
- **THEN** the host command listing is printed and no JavaScript is evaluated

#### Scenario: Exit command
- **WHEN** `.exit` is entered
- **THEN** the run ends cleanly with code 0

### Requirement: Error recovery and exit contract

A line that throws SHALL print the error to the error channel and SHALL NOT
stop the run; subsequent lines SHALL still evaluate. EOF on stdin SHALL shut
down cleanly with code 0. `efx.quit(code)` entered at the console SHALL end the
run with that code. An error printing a result SHALL NOT corrupt the exit code.

#### Scenario: Throw then continue
- **WHEN** a line throws and a later line is entered
- **THEN** the error is printed and the later line still evaluates

#### Scenario: Quit from the console
- **WHEN** `efx.quit(3)` is entered
- **THEN** the player exits with code 3

#### Scenario: EOF exits cleanly
- **WHEN** stdin reaches EOF
- **THEN** the player shuts down and exits 0
