# Design

## Context

See `proposal.md` — Why. The 2D layer draws immediate-mode quads in a virtual
frame (`bouncing-sprites` uses 640×480); text requires a shipped TTF
(`loadFontData` → `createFont` → `drawText`); input offers keyboard events,
held-state queries, and mouse position (`input-playground`). The 2D layer has
no collision and no camera — both are script concerns here.

## Goals / Non-Goals

**Goals:**

- The smallest complete game loop on the shipped surface: serve, rally,
  score, match win, restart — plus attract-mode self-play for the gallery.
- A single readable `main.js` a visitor can learn from; zero per-frame
  allocations.

**Non-Goals:** particles/audio/post (rung 2), persistence, two-player
networking, engine changes.

## Decisions

### D1 — Fixed 640×480 virtual frame, dark neon palette

Match the `bouncing-sprites` convention (640×480) so the game reads in the
gallery's application area without scaling surprises. Neon look from bright
quad colors on a near-black clear color — no post chain (bloom is rung 2's
addition; keeping rung 1 bare keeps the ladder honest).

- **Rejected — larger frame**: no benefit; every existing 2D sample uses
  640×480.

### D2 — Script AABB math, not the physics engine

Pong collision is a handful of axis-aligned rect/point tests with paddle
offset → bounce angle. The physics world is 3D and would need a z=0 staging;
that is more machinery than the game.

- **Rejected — `efx.physics` for ball/paddle contact**: wrong dimensionality
  for the problem; harder to tune the classic feel (angle-from-offset,
  speed ramp).

### D3 — Ball: angle from paddle offset, speed ramp per hit

Bounce angle = f(hit offset from paddle center); ball speed grows a few
percent per paddle hit, capped. Point scored when the ball exits the frame;
brief serve delay with the ball at the scorer's opponent's side. Tuning
constants at the top of the file.

### D4 — Controls: mouse-Y or arrows for the player; AI with a deadzone

Right paddle follows mouse-Y when the pointer moved last, else ↑/↓ held
state. Left paddle is AI: capped speed, a reaction deadzone around the
predicted intercept, and a small per-rally aim error so it is beatable.
P toggles two-player (W/S). First-input rule (D6) supersedes control choice
in attract.

### D5 — One state machine: ATTRACT → SERVE → RALLY → POINT → WIN

`ATTRACT` runs both paddles as AI; the first keyboard/mouse input starts
SERVE with the player paddle handed over. `WIN` at 7 points; any-key/click
restarts into SERVE. One `update(dt)` switch; all state in module-level
variables.

- **Rejected — separate attract script**: doubles the game code path; an AI
  paddle is already needed for 1P and is reused verbatim for attract.

### D6 — First-input edge detection, one handler

A single wrapped input check (any key down, any mouse button, first mouse
move) both exits attract and is reused for win-screen restart. Avoids
per-state listener wiring.

### D7 — Resources: one font pair, created once

`loadFontData('font.ttf')` at top level; two `createFont` sizes (score,
messages). Nothing else is a native resource — quads are immediate-mode. No
`destroy()` needed at restart (resources survive state resets); the gallery
runner releases everything when the run is replaced.

## Risks / Trade-offs

- [AI too strong/weak to be fun] → capped speed + deadzone + seeded per-rally
  error are three independent knobs; tune in one place.
- [Mouse + keyboard fight over the paddle] → last-input-wins with a small
  dead time; document in the on-screen hint text.
- [Attract match runs forever at max ball speed] → attract resets the match
  when either side reaches the target, same as a player match.
