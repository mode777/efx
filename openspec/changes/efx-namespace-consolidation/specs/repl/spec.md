# Spec Delta

## MODIFIED Requirements

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
