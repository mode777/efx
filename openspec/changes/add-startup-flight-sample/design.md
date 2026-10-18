# Design: add-startup-flight-sample

## Context

The scene is authored entirely on the existing public API; see proposal.md
for motivation. Renderer facts this design relies on:

- 2D and 3D draws share **one** display list; sort key = record index, so
  playback order equals record order (`src/render/render_records.c:447`,
  `src/platform/pipeline.c:1624`). 2D quads pipeline with depth compare
  ALWAYS, write off — a quad recorded first stays behind everything
  recorded after it. There is no 2D overlay pass to fight.
- `noise.fill2` / `noise.noise2` are seeded and deterministic on all four
  targets (ADR 0059) — procedural generation at load is reproducible.
- Textures accept `wrap: 'clamp'` and `mipmaps: true`; the 2D quad path
  minifies with them, which distant sprites need.
- The 2D virtual frame **stretches** onto the surface (no letterbox), and
  the default frame tracks window resizes (`openspec/specs/2d-layer`).

## Goals / Non-Goals

- Goals: deterministic frame-for-frame animation; composition derived from
  the actual frame size so the same code survives promotion to a real boot
  screen; no per-frame heap churn beyond the unavoidable option bags; two
  files only (`main.js`, `efx_logo.png`).
- Non-goals: as in the proposal — no player wiring, no goldens, no
  fly-through wash, no audio/post chain.

## Decisions

### D1 — The whole scene is 2D; no 3D camera, no billboards, no dome

Cloud sprites, sky, and logo are all `drawQuad` records in one
painter's-ordered sequence. Perspective is computed in JS:
`f = (H/2)/tan(FOV/2)`, `sx = cx + f·x/z`, `sy = horizonY − f·y/z`,
`w = f·worldW/z`.

- *Rejected: 3D billboards + camera (the original sketch).* Lost because
  the billboard pipeline brings camera/light state and depth-test subtleties
  the scene does not use, while per-sprite projection in JS is ~15 lines
  and gives full control (bank rotation, horizon placement, near fade) the
  fixed pipeline does not. Boot-screen destiny favors minimal state.
- *Rejected: sky dome (skybox-showcase pattern).* An inverted unlit sphere
  to paint a gradient is 3D machinery for what a first-recorded fullscreen
  quad does with zero state. Lost on simplicity; flat clear color also
  rejected — it forfeits the horizon gradient that sells altitude.

### D2 — Cloud sprites are sculpted per-pixel in JS on top of `noise.fill2`

At load: fBm into a `Float32Array` via `noise.fill2`, then one pass over
the pixels applying radial edge falloff, a vertical density bias (denser
underside), a soft knee, and baked top-light shading into RGB (billboards
and quads are unlit — tint alone cannot shade; alpha carries the shape and
RGB stays white-ish). Output: 4–6 puff variants + 1–2 wisp strips, 256²
clamp-wrapped mipmapped textures. Runtime draws tint them.

- *Rejected: raw `noise.imageData(w, h, {alpha: true})`.* The alpha-mode
  shortcut yields uniform fuzzy blobs — fine for far wisps, too formless
  for hero puffs. Used only if it suffices for wisp strips.
- *Rejected: render-target compositing (GPU clustering of puff quads).*
  `drawQuad` tint is constant per draw, so shaping control is far coarser
  than per-pixel, and it adds a load-time pass for no visual gain here.
  Route A's per-pixel falloff/knee is what separates puffy cumulus from fog.
- *Rejected: offline CC0 sprite assets.* Ships binaries into the sample and
  weakens the noise-API showcase; the sample's pitch is "this entire sky is
  seeded calls plus one logo".

### D3 — Motion is a recycler; ordering is a per-frame z sort

Each cloud stores camera-relative `(x, y, z)`; `z` shrinks with speed·dt;
below `zNear` the cloud respawns at `zFar` with fresh lateral/vertical
jitter and a new sprite variant. Frame render sorts the (≤ ~60) clouds by
`z` descending and draws far→near. Distance treatment: tint lerps white →
horizon haze color while alpha smoothsteps in at `zFar` and out at `zNear`.

- *Rejected: alpha-only distance fade.* White ghosts over blue sky; the
  haze lerp is the aerial-perspective cue that makes far clouds dissolve
  into the sky instead of floating over it.

### D4 — Composition derives from the live frame size, not 640×480

`cx`, `cy`, `horizonY`, and `f` are computed from the frame actually in
effect at boot (and the layout is center-relative thereafter), so non-4:3
windows get a correct composition instead of a stretched 4:3 one. This is
the boot-screen promotion insurance; the gallery embed is 4:3 either way.

### D5 — Puffs fade out before reaching the camera; no fly-through beat

The near fade (`alpha → 0` by `zNear`) avoids the full-screen white wash
when a sprite crosses the eye. Deliberate v1 scope; a wash beat can be
added later without structural change.

### D6 — Logo is the last-recorded 2D quad, swaying on a sine

`efx_logo.png` copied into the sample dir, `loadImage` → `createTexture`,
centered `drawQuad` with `y = cy + A·sin(t·ω)` (period ~3.5 s, amplitude
~1.5% of frame height), recorded after all clouds. Rejected: rendering the
wordmark with the text API — it cannot reproduce the hibiscus mark.

### Unmanaged resources

No new engine exposure: the sample only creates engine-managed,
GC-finalized opaque resources through public factories (`createTexture`;
ADR 0011/0012 discipline). None need explicit `destroy()` — they live for
the program's lifetime; no fixed-bank resources are touched.

## Risks / Trade-offs

- [Minified far sprites shimmer without mipmaps] → `mipmaps: true` on all
  cloud textures; the art is intentionally low-contrast and soft.
- [Per-frame JS work (sort + option bags) on quickjs/web] → ≤ ~60 sprites,
  in-place sort, module-scope scratch arrays for colors; trivially inside
  frame budget, but keep it that way (no per-frame allocation beyond
  unavoidable option objects).
- [Cloud-sculpt tuning is taste, not spec] → all shape constants (falloff
  radius, knee, bias, octaves, seeds) are named module constants at the top
  of `main.js`; tuning is one file.
- [Aspect policy stretches non-4:3 surfaces] → D4 makes composition
  frame-derived; the stretch then only scales, never misplaces, elements.
- [Determinism relies on ADR 0059] → seeds are fixed constants; no
  `Math.random` anywhere in the sample.

## Migration Plan

Additive sample; none needed. Rollback = revert the commit.

## Open Questions

None — remaining unknowns (exact speed, amplitudes, palette) are named
tuning constants, resolved by eye during apply.
