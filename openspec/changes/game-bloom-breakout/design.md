# Design

## Context

See `proposal.md` — Why. Everything in Neon Pong applies here (640×480,
script AABB math, state machine, first-input rule). New surface: particle
systems (`createParticleSystem`, burst `emit`, additive blend), post chains
(`addPostEffect` bloom — the `post-bloom-tv` declarative pattern), one-shot
audio (`loadAudioData` + `playAudio` with pan/pitch), and the
`gen-audio-assets.py` deterministic-audio precedent.

## Goals / Non-Goals

**Goals:** juice as gameplay feedback — every world event has a sound, the
destruction has particles, and the whole frame blooms; still one readable
`main.js`.

**Non-Goals:** streamed music (rung 3), power-ups, layouts beyond three,
persistence, engine changes.

## Decisions

### D1 — Script AABB math for all collisions (inherited)

Ball–paddle, ball–brick, ball–walls: rect tests in script. Same reasoning as
Neon Pong D2.

### D2 — One particle system, one shard texture, burst-only

A single `createParticleSystem` with an additive blend and a small
`createImageData` shard/glow texture; brick death calls a burst `emit` at the
brick with its tint. Continuous emitters are unnecessary; one system keeps
the budget flat.

- **Rejected — a system per brick color**: N systems for a tint parameter;
  one system with per-particle color is the shipped model.

### D3 — Bloom chain over the whole frame, authored like `post-bloom-tv`

`setPostEffects([{ effect: 'bloom', ... }, { effect: 'colorFilter', ... }])`
over the default frame. Neon bricks, the ball, and bursts glow; text blooms
only mildly (keep score crisp: bright text color rather than pure white).

### D4 — Screen shake as a script offset added to every draw

The 2D layer has no camera, so a decayed random offset vector is added to
every quad position each frame (magnitude by event: life loss > brick row
clear > paddle hit). Text stays unshaken for readability.

- **Rejected — shake via clear-color or post wobble**: clear color cannot
  move geometry; post chains have no offset effect in the shipped set.

### D5 — Four synthesized WAVs, deterministic generator

`paddle`, `brick`, `wall`, `life` — short synthesized WAVs extending the
committed `gallery/scripts/gen-audio-assets.py` pattern (deterministic
writes, `--check` drift gate). Fired as one-shots with slight pan (ball x)
and pitch jitter (±5%) so repeats don't grate. On the web, audio unlocks on
first input (existing behavior; attract mode is silent until then by
necessity, which reads fine).

- **Rejected — CC0-sourced SFX**: provenance churn for sounds a 20-line
  synth reproduces; the generator keeps assets regenerable in-repo.

### D6 — Layouts as data, three of them

Each layout: brick rows (count, colors = hit tiers), any gaps. Data-driven
so attract/self-play and tuning never touch loop code. Multi-hit bricks
darken per hit; top rows worth more. Clear → next layout, ball speed resets
per layout, three lives shared across the game.

### D7 — Attract AI paddle with lag

The paddle tracks the ball with capped speed plus a small deliberate error
term; identical to Neon Pong's AI approach. First input hands over control
mid-rally.

### D8 — Resources: fonts + one texture + one particle system + audio data

All created once at load; restart reuses them (state reset only). The
gallery runner releases them when the run is replaced.

## Risks / Trade-offs

- [Bloom blows out the dark frame] → tune bloom threshold/intensity once,
  keep quads ≤ 2 brightness tiers.
- [Shake + bloom + particles overwhelm the 2D frame at 60 fps] → particle
  `max` capped; bursts are short-lived; measure on the web build.
- [Pitch jitter sounds off] → cap jitter, make it a constant.
