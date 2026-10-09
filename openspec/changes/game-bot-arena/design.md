# Design

## Context

See `proposal.md` — Why. Surface being composed: character controller
(`moveAndSlide`, top-down), dynamic bodies (`applyForce`, `applyImpulse`,
`contacts`), queries (`raycast` for line-of-sight), particle bursts, the
post chain, one-shot + streamed audio, gamepad sticks, text HUD, emissive
materials, blob shadows.

## Goals / Non-Goals

**Goals:** every engine system doing real work in one game; the light budget
and fixed material set experienced as aesthetic constraints; twin-stick feel
on both control schemes.

**Non-Goals:** glTF assets (procedural primitives), networking, persistence,
engine changes.

## Decisions

### D1 — Player is a character controller, top-down

`createCharacter` capsule on the XZ plane; `moveAndSlide` gives wall slide
for free in a maze-like arena. Aim: mouse cursor → ground-plane raycast
(same construction as mini-golf D3) for the facing/turret direction;
gamepad: right-stick vector when deflected, else keep last.

- **Rejected — player as dynamic body**: capsule-vs-wall sliding is exactly
  what the controller already solves; a dynamic player gets shoved by
  enemies, which is wrong for a health-based game.

### D2 — Enemies are dynamic spheres driven by forces; knockback on hit

Chasers: `applyForce` toward the player (capped speed via damping). Shooters
(`applyForce` to a stand-off ring, fire slow bolts). Collisions between
enemies resolve naturally (they jostle instead of stacking). Player shots
apply `applyImpulse` on hit — knockback is the showcase of impulse dynamics,
and crowd control emerges (shove bots into each other).

- **Rejected — kinematic teleporting bots**: loses jostle/knockback, and
  sensor-body teleports fight the solver.

### D3 — Projectiles: pooled script entities, sphere-distance hits; wall LOS via raycast

Player bolts and enemy bolts are pooled (fixed array, reuse), rendered as
small emissive stretched boxes; hit tests are distance checks per frame
(deterministic, cheap at arena counts). Shooter bots `raycast` toward the
player before firing — walls block line-of-sight, so pillar cover works.

- **Rejected — hitscan for the player**: moving bolts are the game's visual
  rhythm and pool fine; the raycast budget goes to enemy LOS where cover
  matters.

### D4 — Light budget: 4 slots, priority = explosions > muzzle flashes, steal oldest

One directional key light. The 4 point lights form a slot pool: each shot
claims a slot for ~80 ms (muzzle flash at the barrel), each death/explosion
claims one for ~300 ms with a brighter range; claiming steals the
lowest-priority (oldest flash) slot. The fixed budget becomes the game's
lighting style — the arena stays readable because the floor is keyed by the
directional light.

### D5 — Arena: procedural static boxes, neon emissive trim, grid texture

Floor + perimeter walls + a few pillar boxes as static bodies and rendered
meshes. `createImageData` grid/grunge tile on the floor; emissive trim
strips on walls/pillars bloom. Blob shadows under player and bots. Fixed
arena (one layout, wave-scoped spawn points), not randomized — a readable
competitive space.

### D6 — Waves: state machine INTERMISSION → SPAWN → ACTIVE → CLEARED

Wave n: chaser count grows, shooters appear from wave 2, speed and fire-rate
scale mildly. Spawns at perimeter points away from the player. INTERMISSION
(3 s banner) paces waves; CLEARED → INTERMISSION loops forever (endless
escalation — no authored end; the game-over screen is the exit).

### D7 — Health, damage, game over; screen shake and flashes

Contact with a chaser or an enemy bolt costs health (brief i-frames); health
bar as 2D quads + text. Death → slow-mo burst → game over → any-input
restart. Shake, hit flashes (emissive pulse on the bot material), and
particle bursts reuse rung-2 patterns.

### D8 — Audio: SFX bank + streamed music, one convention

Shot/hit/explosion/wave-start WAVs from the deterministic generator
(`gen-audio-assets.py` extension); pan from screen x, ±5% pitch jitter.
Streamed loop per the audio-showcase pattern; M / `[` / `]` controls; web
unlock on first input. Same keys as rung 3 — one convention across the
series.

### D9 — Attract: scripted twin-stick with aim at nearest enemy

Move on a slow patrol figure-eight; aim/fire at the nearest bot; take a few
hits on purpose so the game-over loop is also visible in long embeds. First
input takes over mid-wave.

### D10 — Resources once at load; restart resets state

Bodies destroyed and recreated per wave only where counts change; pooled
bolts never reallocate; fonts/textures/particle systems/audio created once.
Character body persists across restarts (position reset).

## Risks / Trade-offs

- [Dynamic bots tunnel or jitter at low iteration counts] → keep `iterations`
  at the physics showcase's proven setting; bots are sphere-sphere and
  sphere-box only.
- [Force-driven bots orbit instead of reaching the player] → add a small
  velocity-alignment term toward the desired heading; cap desired speed.
- [Light-slot stealing looks strobing] → clamp concurrent claims per frame
  and fade the oldest slot instead of popping it.
- [Attract mode dies too fast] → give the attract player modest auto-dodge;
  restart attract on game-over without showing the full game-over beat.
