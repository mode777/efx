# Spec Delta: js-runtime

## ADDED Requirements

### Requirement: Script clock

The runtime SHALL expose an engine-owned clock to scripts as
`efx.time.now`: the number of seconds elapsed since the script started,
advanced natively by the frame loop (never accumulated in script). The
value SHALL be snapshotted at frame start: every read within one frame —
update hooks, render hooks, input callbacks — SHALL observe the identical
value, and the value SHALL strictly increase frame over frame while frames
run. The accumulator SHALL apply an internal per-frame maximum-step clamp so
a stall (window drag, background tab, debugger pause) does not advance the
clock by more than the clamp; animation driven by `now` therefore cannot
teleport. The clock SHALL be monotonic within a run, SHALL be a plain
read-only number (assignment SHALL have no effect), and SHALL behave
identically on the desktop and web runtimes. The `efx.time` sub-namespace
SHALL carry no other members in this change.

#### Scenario: Snapshot per frame

- **WHEN** an update hook and the render hook of the same frame both read
  `efx.time.now`, and the next frame reads it again
- **THEN** the two same-frame reads are identical, and the next frame's
  value is greater

#### Scenario: Stall does not teleport the clock

- **WHEN** the frame loop stalls for a long interval (for example a
  backgrounded tab or a deliberate long sleep in the harness)
- **THEN** `now` advances by at most the internal clamp for that frame, and
  subsequent frames advance normally

#### Scenario: Read-only plain number

- **WHEN** a script reads `efx.time.now` in the first update hook and then
  assigns to it
- **THEN** the first read is approximately zero, the value remains a plain
  number on every read, and the assignment has no effect

#### Scenario: Identical across runtimes

- **WHEN** the same scripted sequence of frames runs on the desktop player
  and the web player
- **THEN** `now` follows the same start-at-zero, per-frame-snapshot,
  clamped-advance behavior on both
