# Proposal

## Why

Rung 3 of the six-game game-gallery ladder: the first rung where F13 gamepad
and F14 streamed music carry the experience. A one-button dodger is the
smallest game that reads naturally on both keyboard and controller, and a
music loop is what makes it feel like a game rather than a demo. Composes
the F2 2D layer, F8 text, F9 input, F13 gamepad, F11 particles (death
burst), and F14 audio (streamed music + one death sting). Post-roadmap
content change; consumes completed milestones only.

## What Changes

- Ship a curated game sample
  `gallery/samples/curated/game-glow-gauntlet/`   (self-contained directory: `main.js`, the standard CC0 Kenney font, one
  CC0/self-authored looping music track, and one sting wav)
  implementing a complete one-button dodger:
  - vertical scroller: a glowing avatar rises through scrolling gates with
    gaps; one input (Space / click / any gamepad face button) darts the
    avatar sideways or punches through, per the final control scheme;
  - difficulty ramps gate speed and gap tightness; score is gates passed,
    rendered as text with the running best (session-only);
  - death as an additive particle burst, game-over screen with instant
    restart on the same button;
  - streamed looping music through the F14 playback bank (M mutes, `[` / `]`
    volume), death sting through the effect bank; on the web, audio unlocks
    on first input per the existing F14 behavior;
  - gamepad as a first-class input alongside keyboard and mouse;
  - **attract mode**: scripted input self-plays until the first player input.
- Add the sample to `gallery/samples/curated/manifest.json` under the `Games`
  category, with `CREDITS.md` rows for the shipped audio.
- Extend the gallery smoke run to drive the sample and fail on console or
  page errors.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `web-gallery`: a new requirement pins the Glow Gauntlet sample — the
  catalog contains it under the Games category, it renders a complete
  one-button dodge loop driven by keyboard, mouse, and gamepad with streamed
  music and particle death feedback from the public API, it self-plays until
  first input, its audio follows the curated asset-pack and CREDITS
  contract, it is runnable by the player under the resource-root contract,
  and it is covered by the gallery smoke run.

## Impact

- **Code**: none in `src/`.
- **Assets / gallery**: `gallery/samples/curated/game-glow-gauntlet/`
  (`main.js`, one CC0 music loop, one sting wav), `CREDITS.md` rows,
  manifest entry, catalog regeneration, smoke-run coverage.
- **Docs**: no API change — `efx.d.ts` and `docs/api/` untouched. No ADR —
  content-only change; uses only existing public API.
- **Verification**: gallery build + smoke run; the four-target gate still
  crosses the change per ADR 0020/0023.

### Non-goals

- No 3D, lighting, or physics — those enter at rungs 4–5; this rung stays
  flat 2D plus particles/audio.
- No procedural music generation — a shipped CC0 loop, not synthesis.
- No persistence: best scores are session-only.
- No golden scene (interactive/nondeterministic; smoke is the gate).
- No engine or script-API changes; an API gap surfaced by the game becomes
  its own engine change.
