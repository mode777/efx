# Proposal

## Why

Rung 5 of the six-game game-gallery ladder: the F12 character controller put
to its intended use — a playable 3D platformer. `moveAndSlide`, step height,
floor snapping, and sensors become jumping between floating platforms, and a
CC0 rigged character (the proven fox pipeline) puts F7 skinning/animation
inside gameplay rather than a walk-cycle demo. Composes the F3 3D core, F4
lighting, F6 glTF import, F7 skinning, F8 text (timer/HUD), F9 input, F12
character controller + sensors, plus the shipped blob-shadow and skybox
patterns. Post-roadmap content change; consumes completed milestones only.

## What Changes

- Ship a curated game sample `gallery/samples/curated/game-sky-steps/`
  (self-contained directory: `main.js`, the standard CC0 Kenney font, a CC0
  rigged glTF character, and a CC0 equirectangular sky image, with
  `CREDITS.md` rows) implementing a complete collect-them-all platformer:
  - a capsule character from `createCharacter` (step height, floor snap,
    `moveAndSlide`) with script-owned gravity and jump;
  - a floating platform course over a void: static box platforms and ramps,
    star collectibles as sensor volumes, checkpoints, fall-below-height
    respawn, and a finish sensor that ends the timer;
  - a third-person follow camera (orbit with the mouse, distance behind the
    character);
  - the rigged character drawn `skinned: true` with `mesh.pose` blending
    idle/walk/jump clips driven by speed and airborne state;
  - a blob shadow (procedural radial texture, `facing: 'plane'` billboard)
    under the character and a camera-locked unlit skybox backdrop;
  - one directional light plus up to two point lights; HUD text for timer
    and stars; win screen with time and restart;
  - **attract mode**: scripted movement input self-plays until the first
    player input.
- Add the sample to `gallery/samples/curated/manifest.json` under the `Games`
  category.
- Extend the gallery smoke run to drive the sample and fail on console or
  page errors.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `web-gallery`: a new requirement pins the Sky Steps sample — the catalog
  contains it under the Games category, it renders a complete
  collect-them-all platformer exercising the character controller, sensor
  collectibles/checkpoints, skinned glTF animation, blob shadow, and skybox
  from the public API, it self-plays until first input, its shipped assets
  follow the curated asset-pack and CREDITS contract, it is runnable by the
  player under the resource-root contract, and it is covered by the gallery
  smoke run.

## Impact

- **Code**: none in `src/`.
- **Assets / gallery**: `gallery/samples/curated/game-sky-steps/` (`main.js`,
  CC0 rigged character glTF, CC0 sky image), `CREDITS.md` rows with
  provenance and downscale recipes, manifest entry, catalog regeneration,
  smoke-run coverage.
- **Docs**: no API change — `efx.d.ts` and `docs/api/` untouched. No ADR —
  content-only change; uses only existing public API.
- **Verification**: gallery build + smoke run; the four-target gate still
  crosses the change per ADR 0020/0023.

### Non-goals

- No moving platforms or platform-velocity carry — the controller does not
  expose carrying a rider with a moving surface; that would be an engine
  change if ever wanted, not a game workaround.
- No combat, enemies, or hazards beyond falling.
- No persistence: best times are session-only.
- No audio this rung (reserved for the capstone's full bank).
- No golden scene (interactive/nondeterministic; smoke is the gate).
- No engine or script-API changes; an API gap surfaced by the game becomes
  its own engine change.
