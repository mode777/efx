# Spec Delta

## ADDED Requirements

### Requirement: Glow Gauntlet game sample

The curated gallery catalog SHALL include a Glow Gauntlet game sample — the
third rung of the game series, the first where gamepad input and streamed
music carry the experience — and it SHALL use only the public `efx` API and
standard ES6, with no browser or Node.js dependency.

The sample SHALL implement a complete one-button dodger: an avatar that rises
while the single control is held and sinks while it is released, scrolling
gates with gaps that must be threaded, a score of gates passed with a
session-best display, and death on gate collision that shows a game-over
screen restarting on the same button. The single control SHALL map
identically to a keyboard key, a mouse button, and a gamepad face button.
Difficulty SHALL ramp gate speed and tighten gaps as the score grows, with a
floor on gap width. Death SHALL emit an additive particle burst and a
one-shot sting, and a looping music track SHALL stream through the audio
playback bank with mute and volume controls; on the web, audio SHALL unlock
on the first input per the existing audio behavior. The sample SHALL
self-play (script-driven button) from launch until the first player input.

The sample's shipped assets (font, music loop, sting) SHALL follow the
existing curated sample asset-pack contract with provenance recorded in
`gallery/samples/curated/CREDITS.md`; the sting SHALL be synthesized
deterministically by a committed generator script with a drift check. The
sample SHALL be runnable by the player under the same resource-root contract
as any other sample, and SHALL be covered by the gallery smoke run.

#### Scenario: Catalog contains the game sample

- **WHEN** the curated gallery catalog is read
- **THEN** it contains the Glow Gauntlet sample under a Games category, and
  selecting it runs the sample in the application area

#### Scenario: One-button loop across input modes

- **WHEN** the player holds and releases the keyboard key, the mouse button,
  or a gamepad face button
- **THEN** the avatar rises while held and sinks while released in all three
  modes, and death on a gate shows a game-over screen that restarts on the
  same button

#### Scenario: Difficulty ramps within a run

- **WHEN** the passed-gate score grows during a run
- **THEN** gates scroll faster and gaps tighten toward a floor, and the
  session-best score is displayed as text

#### Scenario: Music streams with controls

- **WHEN** the sample runs
- **THEN** a looping track streams through the audio playback bank, the mute
  and volume controls act on it, and death fires a one-shot sting through the
  effect bank

#### Scenario: Self-play until first input

- **WHEN** the sample runs with no player input
- **THEN** a script-driven button plays visibly in the gallery embed, and the
  first player input takes control

#### Scenario: Assets, portability, and smoke

- **WHEN** the sample's directory and credits are read, its source is run by
  the `efx` player against its resource-root directory, or the gallery smoke
  run executes
- **THEN** it ships the standard CC0 font, a credited music loop, and a
  deterministically generated sting with provenance rows in
  `gallery/samples/curated/CREDITS.md`; it runs using only the public API and
  standard ES6; and the smoke fails on any console or page error from it
