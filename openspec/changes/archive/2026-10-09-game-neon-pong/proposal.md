# Proposal

## Why

The curated gallery demonstrates each engine system in isolation (one sample
per feature), but nothing shows systems **composed into a complete game** —
goal, score, win/lose state, restart. This is the first rung of a six-game
"game gallery" ladder (pong → breakout → dodger → mini-golf → platformer →
twin-stick arena) that proves the engine by building games on it, each rung
adding one or two systems. Neon Pong is the smallest complete game loop the
2D layer can express. Post-roadmap content change; consumes completed
milestones F2 (2D layer), F8 (text), F9 (input) and nothing else.

## What Changes

- Ship a curated game sample `gallery/samples/curated/game-neon-pong/`
  (self-contained directory: `main.js` plus the standard CC0 Kenney font every
  text-bearing sample ships) implementing a complete
  Pong game:
  - two paddles and a ball as 2D quads over a dark neon palette, dashed
    center line, score rendered with `drawText`;
  - keyboard (W/S and arrow keys) and mouse control for the player paddle;
  - an AI opponent paddle with imperfect tracking, player-vs-player toggle;
  - first to 7 points wins, serve delay between rallies, win screen with
    restart on key/click;
  - **attract mode**: both paddles driven by AI (the `input-playground`
    pattern) until the first player input, so the sample shows live gameplay
    in the gallery embed without interaction.
- Add the sample to `gallery/samples/curated/manifest.json` under a new
  `Games` category.
- Extend the gallery smoke run to drive the sample and fail on console or
  page errors.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `web-gallery`: a new requirement pins the curated game series and the Neon
  Pong sample — the catalog contains it under the Games category, it renders
  a complete pong loop (paddles, ball, score text, win state, restart) from
  the public 2D/text/input API, it self-plays until first input, it is
  runnable by the player under the resource-root contract, and it is covered
  by the gallery smoke run.

## Impact

- **Code**: none in `src/` — the game is a pure script using only the shipped
  public API.
- **Assets / gallery**: `gallery/samples/curated/game-neon-pong/main.js`
  and `font.ttf` (same Kenney CC0 font as `text-showcase`, with its
  `CREDITS.md` row), manifest entry, catalog regeneration via
  `gallery/scripts/gen-catalog.mjs`, smoke-run coverage.
- **Docs**: `gallery/src/api/efx.d.ts` and `docs/api/` are untouched (no API
  change). No ADR — content-only change with no durable architecture
  decision; uses only existing public API.
- **Verification**: gallery build + smoke run against the new sample; the
  four-target gate is unaffected (no native or API surface change), but the
  change still crosses it per ADR 0020/0023.

### Non-goals

- No particles, audio, or post effects — those enter the ladder at rung 2
  (Bloom Breakout); Pong stays quads + text + input.
- No persistence: scores are session-only (no storage API exists; growing one
  would be a separate engine change).
- No golden scene: the game is interactive and nondeterministic; verification
  is the smoke run, not a pixel diff.
- No engine or script-API changes of any kind — if implementation reveals an
  API gap, the gap is proposed as its own engine change, not folded into the
  game.
