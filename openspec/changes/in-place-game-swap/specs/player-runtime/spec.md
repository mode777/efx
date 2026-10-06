# Spec Delta

## ADDED Requirements

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
