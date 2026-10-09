# Proposal

## Why

Rung 2 of the six-game game-gallery ladder: the first "juice" rung, where
particles, audio, and post FX stop being demo toggles and become gameplay
feedback. Bloom Breakout composes the 2D layer with the F11 particle system,
the F14 SFX bank, and the F5 post chain — the systems Pong (rung 1) left out.
Post-roadmap content change; consumes completed milestones F2 (2D), F5 (post
FX), F8 (text), F9 (input), F11 (particles), F14 (audio).

## What Changes

- Ship a curated game sample `gallery/samples/curated/game-bloom-breakout/`
  (self-contained directory: `main.js`, the standard CC0 Kenney font, and a
  small self-synthesized wav pack) implementing a complete Breakout:
  - paddle (mouse and keyboard), ball, and a brick grid with multi-hit rows
    rendered as 2D quads; score and lives as text;
  - brick shatter as additive particle bursts (`createImageData`-generated
    shard texture — no third-party image), growing speed over the rally;
  - a declarative bloom post chain over the whole frame so neon bricks and
    bursts glow;
  - screen shake on paddle hits and life loss (camera offset in the 2D
    frame);
  - SFX blips for paddle/brick/wall/life from a small wav set fired through
    the one-shot effect bank (on the web, audio unlocks on first input per
    the existing F14 behavior);
  - level clear advances to the next of three procedural layouts, game over
    and win screens with restart on key/click;
  - **attract mode**: scripted input self-plays until the first player input.
- Add the sample to `gallery/samples/curated/manifest.json` under the `Games`
  category, with a `CREDITS.md` row per shipped audio file.
- Extend the gallery smoke run to drive the sample and fail on console or
  page errors.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `web-gallery`: a new requirement pins the Bloom Breakout sample — the
  catalog contains it under the Games category, it renders a complete
  breakout loop composing 2D quads, text, particles, post bloom, and one-shot
  audio from the public API, it self-plays until first input, its shipped
  audio follows the curated asset-pack and CREDITS contract, it is runnable
  by the player under the resource-root contract, and it is covered by the
  gallery smoke run.

## Impact

- **Code**: none in `src/`.
- **Assets / gallery**: `gallery/samples/curated/game-bloom-breakout/`
  (`main.js`, 3–5 short wavs), `CREDITS.md` provenance rows, manifest entry,
  catalog regeneration, smoke-run coverage.
- **Docs**: no API change — `efx.d.ts` and `docs/api/` untouched. No ADR —
  content-only change; uses only existing public API.
- **Verification**: gallery build + smoke run; the four-target gate still
  crosses the change per ADR 0020/0023.

### Non-goals

- No streamed music — music enters the ladder at rung 3 (Glow Gauntlet);
  this rung ships one-shot SFX only.
- No power-ups, bosses, or unlocks — a tight three-layout arc.
- No persistence: high scores are session-only.
- No golden scene (interactive/nondeterministic; smoke is the gate).
- No engine or script-API changes; an API gap surfaced by the game becomes
  its own engine change.
