# Design

## Context

See `proposal.md` — Why. New surface: gamepad (`efx.gamepad.get`, semantic
buttons), streamed audio (`loadAudioStream` + `playAudio` loop — the
`audio-showcase` music pattern, incl. web unlock-on-first-input), particles
(inherited from rung 2's burst pattern). 2D quads + `createImageData` glow
textures for the avatar and gates.

## Goals / Non-Goals

**Goals:** a game where one analog axis (hold = rise, release = sink) reads
identically on key, mouse, and face button; music as the emotional core;
instant restart.

**Non-Goals:** 3D, physics, procedural music synthesis, persistence, engine
changes.

## Decisions

### D1 — Hold-to-rise, not tap-to-flap

A single held-state axis: avatar accelerates up while held, gravity pulls
down when released — a buoyancy feel. Held state maps 1:1 across Space (key
held query), LMB (button held), and a mapped gamepad face button (button
state from `gamepad.get`), with no edge semantics to port.

- **Rejected — tap-to-flap (impulse per press)**: edge-triggered input needs
  per-device repeat handling (key auto-repeat vs click vs button) and is
  harder to keep identical across the three input modes.
- **Rejected — steer left/right dodger**: more buttons, and vertical
  threading showcases the tuned-feel physics better.

### D2 — Horizontal scroller: fixed-x avatar, gates scroll right-to-left

The avatar holds a fixed x near the left edge with a little vertical lag;
gates (vertical walls with a vertical gap) spawn at the right edge and move
left. This is the canonical "copter" reading of hold-to-rise: the single
axis the player controls is the avatar's height, and threading a moving gap
is the whole game. Everything lives in frame coordinates.

### D3 — Gates from a seeded pattern library

A small library of gap patterns (sine hole, double slot, zigzag, pinch);
per run, a seeded PRNG picks and spaces them with difficulty-scaled speed
and gap width (floor-capped). Seeded = the attract demo and the smoke run
are reproducible; the seed rotates per run.

### D4 — Death: burst, sting, freeze, one-button restart

Collision (AABB vs gate rects) → additive particle burst at the avatar, one
`sting` one-shot, world freezes, game-over text with score/best; the same
control restarts instantly (edge-detected here — a single deliberate press
edge, consistent because it's the only edge in the game).

### D5 — Music loop via `loadAudioStream`, synthesized WAV, showcase controls

A streamed looping track, synthesized deterministically as a WAV by
`gallery/scripts/gen-audio-assets.py` (its `game-glow-gauntlet` bank) so it
is byte-reproducible and needs no external encoder. M mutes, `[` / `]`
volume — identical keys to `audio-showcase` so the series teaches one
convention. On the web the track starts on first input (unlock); attract
runs silent until then, which the run-over text acknowledges.

- **Rejected — a committed MP3**: needs a pinned ffmpeg and is not
  byte-reproducible across encoders (the `audio-showcase` precedent commits
  one only because it predates the generator bank); a synthesized WAV is one
  source of truth and regenerable with `--check`.
- **Rejected — a sourced CC0 track**: adds third-party provenance for a
  simple loop the generator can reproduce in-repo.

### D6 — Score = gates passed; session best in a module variable

Best persists across restarts in the same run only. No storage API exists;
inventing one is an engine change and out of scope.

### D7 — Attract AI: hold when below the next gap's center line

One-line heuristic on the next gate's gap y. Good enough to look alive;
deliberately mediocre so players beat it.

### D8 — Resources: fonts, glow texture, one particle system, stream + sting

Created once at load. The stream's `destroy()` is called never (single run
owns it; the gallery runner tears the run down).

## Risks / Trade-offs

- [Hold-to-rise is flappy-hard] → gravity/thrust constants behind tuning
  names; aim for a floatier feel than Flappy Bird.
- [Web silence in attract] → on-screen "press anything" hint doubles as the
  audio-unlock invitation.
- [Music loop licensing] → CC0 track with a CREDITS row; committed once.
