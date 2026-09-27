# Spec Delta

## ADDED Requirements

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
