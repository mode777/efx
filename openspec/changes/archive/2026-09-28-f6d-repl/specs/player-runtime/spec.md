# Spec Delta

## ADDED Requirements

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
