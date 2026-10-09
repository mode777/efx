# Proposal

## Why

Rung 4 of the six-game game-gallery ladder: the first 3D rung and the first
game built on F12's impulse dynamics. Mini-golf turns collision bodies,
restitution/friction, sensors, and raycasts into **game feel** — aim, power,
roll, sink — instead of the prop-pushing of the existing physics showcase.
Composes the F3 3D core, F4 lighting, F8 text (stroke/par HUD), F9 input
(mouse aim), and F12 collision + impulse dynamics + queries. Post-roadmap
content change; consumes completed milestones only.

## What Changes

- Ship a curated game sample `gallery/samples/curated/game-mini-golf/`
  (self-contained directory: `main.js` plus the standard CC0 Kenney font for
  its HUD; all geometry and textures procedural) implementing a
  complete 9-hole procedural mini-golf:
  - nine holes authored as data: tee, cup (a sensor volume), wall boxes,
    static triangle-mesh ramps and obstacles;
  - the ball as a dynamic sphere body (mass, friction, restitution tuned for
    roll and bounce), stroke counting, par per hole;
  - aiming by mouse: a raycast onto the ground plane through the ball sets
    the aim line; hold to charge a power meter, release to apply the
    impulse;
  - cup entry detected through the sensor (`overlap` / contacts) advances to
    the next hole; a final scorecard screen shows strokes vs par with
    restart;
  - a follow camera behind the ball, one directional light plus one point
    light, a procedural grass texture (`createImageData`), and a blob shadow
    under the ball;
  - **attract mode**: scripted aim-and-stroke input self-plays until the
    first player input.
- Add the sample to `gallery/samples/curated/manifest.json` under the `Games`
  category.
- Extend the gallery smoke run to drive the sample and fail on console or
  page errors.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `web-gallery`: a new requirement pins the Mini Golf sample — the catalog
  contains it under the Games category, it renders a complete 9-hole loop
  exercising dynamic bodies, sensors, raycast aiming, and stroke/par text
  from the public `efx.physics` + 3D API, it self-plays until first input,
  it needs no third-party assets, it is runnable by the player under the
  resource-root contract, and it is covered by the gallery smoke run.

## Impact

- **Code**: none in `src/`.
- **Assets / gallery**: `gallery/samples/curated/game-mini-golf/`
  (`main.js`, `font.ttf` — same Kenney CC0 font as `text-showcase`, with its
  `CREDITS.md` row; everything else procedural), manifest entry, catalog
  regeneration, smoke-run coverage.
- **Docs**: no API change — `efx.d.ts` and `docs/api/` untouched. No ADR —
  content-only change; uses only existing public API.
- **Verification**: gallery build + smoke run; the four-target gate still
  crosses the change per ADR 0020/0023.

### Non-goals

- No moving obstacles, wind, or animated course elements — a static course
  of boxes and triangle meshes.
- No character controller — that enters at rung 5 (Sky Steps); the ball is
  pure impulse dynamics.
- No audio this rung (SFX arrive with the capstone's full bank) — audio here
  is optional and only from shipped CC0 files if added.
- No persistence: course records are session-only.
- No golden scene (interactive/nondeterministic; smoke is the gate).
- No engine or script-API changes; an API gap surfaced by the game becomes
  its own engine change.
