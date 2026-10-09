# Design

## Context

See `proposal.md` — Why. Character surface: `createCharacter(radius, height,
{position, floorMaxAngle, stepHeight, floorSnapLength})`, `moveAndSlide`
returning `{onFloor, onWall, onCeiling, floorNormal}`, writable `velocity`,
script-integrated gravity (the physics-showcase pattern). Rigged assets: the
fox pipeline (`loadMeshData`, `mesh.pose`, `skinned: true`); blob shadow and
skybox patterns are shipped showcases. Physics `step` drives sensor overlaps.

## Goals / Non-Goals

**Goals:** the character controller used as intended — a playable 3D
platformer with jump feel, checkpoints, and a collect-all goal; a rigged,
animated player character; self-play in the gallery.

**Non-Goals:** moving platforms (see D6), combat, audio (capstone), authored
animation clips, persistence, engine changes.

## Decisions

### D1 — Movement: controller motion, script gravity, coyote + jump buffer

Horizontal velocity from input (camera-relative), gravity integrated
script-side into `velocity[1]`, `moveAndSlide(v·dt)` per frame. Jump when
`onFloor` or within a short coyote window; a small jump-buffer makes taps
land. Constants (speed, jump impulse, gravity) named at the top.

### D2 — Camera: follow + LMB-drag orbit, pitch-clamped

Camera orbits the character at fixed distance; dragging LMB moves yaw (and
clamped pitch); no drag → yaw eases back behind the velocity heading.
`setCamera3D(orbitPos, characterPos)`. Mouse-move deltas come from the input
API's move events; no pointer lock needed.

- **Rejected — always-behind camera**: platforming demands lookahead and
  manual aim at jumps; a fixed chase camera fights the player on backtrack.
- **Rejected — pointer-locked free look**: no pointer-lock API is exposed,
  and drag-orbit reads fine for a small course.

### D3 — Course: static boxes + ramp meshes; stars, checkpoints, finish as sensors

Platforms are static box bodies; ramps as triangle meshes (both proven by
the physics showcase). Stars and checkpoints are `sensor: true` bodies —
`overlap` against the character each `step`; checkpoint sensors set the
respawn point; the finish sensor only counts when all stars are collected
(the finish beacon's color signals locked/unlocked). Kill plane: y below the
lowest platform → respawn at the checkpoint, no physics state to reset (the
character is kinematic from the solver's view).

### D4 — Fox: pose clips by speed; airborne holds the gallop

`mesh.pose` advances Idle / Walk / Gallop (`AnimalArmature|Idle`, `|Walk`,
`|Gallop`) by ground speed with direct crossfade; airborne holds the Gallop
clip (authoring a jump clip is out of scope and the pack's jump clips are
one-shots that read oddly under time-wrapped posing). Draw with
`skinned: true` per the fox-walk recipe. The clip names are the pack's
actual animation names (verified against `Fox.glb`).

### D5 — Look: directional key light + ≤2 point accents, blob shadow, sky

Blob shadow (procedural radial, `facing: 'plane'`, small lift) under the
character — cheap grounding over the void. Skybox: the shipped
inverted-sphere + unlit `depthWrite: false` pattern with the CC0 equirect
(own copy + CREDITS row). Stars/finish beacons emissive.

### D6 — No moving platforms

The controller exposes no platform-velocity carry: standing on a moved
platform requires the game to add platform velocity to character motion
every frame — an engine-level concept. Static course only; if carry is ever
wanted it is its own engine change.

- **Rejected — script-side carry hack (teleport character with platform
  delta)**: breaks under jump timing and teaches a wrong pattern in a
  showcase.

### D7 — Timer + stars HUD; ATTRACT → PLAY → WIN

Timer runs PLAY → WIN; star count "3/8" text; WIN shows time and restart.
Attract drives the character toward the nearest uncollected star with the
jump heuristic (jump when the next platform's top is above the character and
the gap is near); deliberately slow. First input takes over instantly.

### D8 — Resources: all created once at load

Character body, static course bodies, sensor bodies, meshes, fonts, textures
— one-time. Restart resets positions/star flags only. The gallery runner
releases the run.

## Risks / Trade-offs

- [Camera-relative movement on an orbiting camera confuses players] →
  input basis is the camera's yaw only (never pitch); ease-back keeps
  default orientation forward.
- [Fox scale/step tuning] → character capsule (0.4, 1.8 per showcase) may
  dwarf the fox; scale the model to the capsule, not the reverse.
- [Attract AI falls into the void] → falling is fine — respawn is instant
  and the demo continues; cap attract restarts per minute in code so it
  never reads as broken.
