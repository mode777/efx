# Proposal

## Why

Rung 6, the capstone of the six-game game-gallery ladder: a twin-stick arena
that composes everything the engine ships — 3D rendering, lighting, post FX,
particles, physics queries, text, keyboard/mouse/gamepad input, and audio —
in one game. It also turns two fixed constraints into features: the 4
point-light budget becomes muzzle flashes competing for slots, and the
fixed-function material set becomes an emissive-neon aesthetic.
Post-roadmap content change; consumes completed milestones F2–F14 as
applicable; modifies no engine surface.

## What Changes

- Ship a curated game sample `gallery/samples/curated/game-bot-arena/`
  (self-contained directory: `main.js`, the standard CC0 Kenney font, and a
  small self-synthesized SFX/music pack with `CREDITS.md` rows)
  implementing a complete wave-based twin-stick arena:
  - player movement on WASD, aiming/firing on mouse; full gamepad mapping
    (left stick move, right stick aim/fire) as a first-class control;
  - enemy waves from a spawn state machine: chaser bots and shooter bots,
    wave counter, escalating counts and speeds;
  - a procedural neon arena — floor, walls, pillar cover — built from
    primitives with emissive accents, procedural textures via
    `createImageData`; blob shadows under actors;
  - projectiles and hit feedback: additive particle bursts, screen shake,
    hit flashes; enemy death scored on the HUD;
  - muzzle-flash lighting within the fixed budget: point lights reserved
    for the loudest recent shots and explosions (design decides the exact
    slot policy);
  - a bloom post chain over the frame; HUD text for score, wave, and
    health; death → game over → restart;
  - SFX bank (shot, hit, explosion, wave start) plus streamed music
    (M mutes); on the web, audio unlocks on first input;
  - **attract mode**: scripted twin-stick input self-plays until the first
    player input.
- Add the sample to `gallery/samples/curated/manifest.json` under the `Games`
  category.
- Extend the gallery smoke run to drive the sample and fail on console or
  page errors.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `web-gallery`: a new requirement pins the Bot Arena sample — the catalog
  contains it under the Games category, it renders a complete wave-based
  twin-stick loop composing 3D, lighting, post bloom, particles, physics
  queries, text, keyboard/mouse/gamepad input, and audio from the public
  API, it self-plays until first input, its audio follows the curated
  asset-pack and CREDITS contract, it is runnable by the player under the
  resource-root contract, and it is covered by the gallery smoke run.

## Impact

- **Code**: none in `src/`.
- **Assets / gallery**: `gallery/samples/curated/game-bot-arena/`
  (`main.js`, 4–6 short CC0 wavs plus one music loop), `CREDITS.md` rows,
  manifest entry, catalog regeneration, smoke-run coverage.
- **Docs**: no API change — `efx.d.ts` and `docs/api/` untouched. No ADR —
  content-only change; uses only existing public API.
- **Verification**: gallery build + smoke run; the four-target gate still
  crosses the change per ADR 0020/0023.

### Non-goals

- No new engine features — if gameplay reveals an API gap (e.g. a needed
  query or draw option), it is proposed as a separate engine change and the
  game is shaped around the shipped surface in the meantime.
- No leaderboards, persistence, or networking; scores are session-only.
- No glTF assets — the arena and actors are procedural primitives; rigged
  models were rung 5's showcase.
- No golden scene (interactive/nondeterministic; smoke is the gate).
