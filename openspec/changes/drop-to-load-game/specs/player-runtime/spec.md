# Spec Delta

## ADDED Requirements

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
