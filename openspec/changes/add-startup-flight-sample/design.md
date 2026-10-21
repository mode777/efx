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
  fly-through wash, no audio. (The post chain non-goal was dropped by the
  reference-look revision, D7.)

## Decisions

### D1 — The whole scene is 2D; no 3D camera, no billboards, no dome

Cloud sprites, sky, and logo are all `drawQuad` records in one
painter's-ordered sequence. Perspective is computed in JS:
`f = (H/2)/tan(FOV/2)`, `sx = cx + f·x/z`, `sy = horizonY − f·y/z`,
`w = f·worldW/z`.

- *Rejected: 3D billboards + camera (the original sketch).* Lost because
  the billboard pipeline brings camera/light state and depth-test subtleties
  the scene does not use, while per-sprite projection in JS is ~15 lines
  and gives full control (horizon placement, distance squash, haze) the
  fixed pipeline does not. Boot-screen destiny favors minimal state. The
  flight is straight ahead: the bank roll was removed in the
  reference-look revision.
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

**Revision (reference-look pass).** fBm density blobs — additive or
source-over — washed the deck to a white blur: they carry no crest/trough
structure, which is what makes a cloud sea read. Billows are now baked as
height fields: a union of sphere caps (crest spheres on a peaked mound
profile, smaller spheres budding off their upper rims for the cauliflower
look, wide low body spheres), merged with a polynomial smooth-max so creases
become soft valleys, rims wobbled by `noise.fill2`. The winning normal is
lit by the sun direction, blended with a crest-to-base gradient into
blue-grey, plus a silver rim on thin silhouettes; alpha comes from the
rim normal and a base fade. Eight wide ridges (512×160) and four round
puffs (256²) share one 1024² mipmapped atlas (4 px gutters; sphere radii
clamped inside the slot), so the deck is one `drawSprites` batch with
`sourceRect`s. Load-time bake ≈ 2 s on desktop QuickJS.

### D3 — The sea is three depth tiers plus a horizon band; tops are
### pinned, motion is a recycler (revised after review: realism pass)

One recycler (z shrinks with speed·dt; below the near threshold a billow
respawns at the far end) drives ~130 sprites in three tiers: a broad faint
**far floor** keeping the horizon line solid, **mid billows** carrying the
visible topography, and a sparse **near rim** whose tall tops brush the
flight path. Six extra-wide sprites straddle the vanishing point as a
horizon haze band. Each sprite's center is derived from a target top height
(`setTop`) — the far floor pins just under the horizon, mid/near tops rise
a distance-scaled poke above it — so the sea's upper edge stays at the
flight path at every phase. Distance treatment: tint lerps toward the sky
color at the sprite's own screen height while alpha eases at both z ends
(additive feeds the horizon band instead of cutting off).

- *Rejected: alpha-only distance fade.* White ghosts over blue sky; the
  haze lerp is the aerial-perspective cue that dissolves far clouds into
  the sky instead of floating over it.
- *Rejected (then un-rejected): painter's back-to-front sort.* Dropped
  during the additive experiment, restored when the review moved the sea
  back to source-over alpha, where it is required for correct occlusion.

**Revision (reference-look pass).** The tiers are replaced by one
world-uniform population (~900 billows spawned across the full far-plane
width, flying straight at the camera, so the near field thins exactly as
the frustum narrows). Eye height sits above the mean crest so everything
nearer than ~12 units has slid off the frame bottom. Distant billows are
squashed vertically (grazing view), tinted toward the haze color, and
their alpha melts into a haze band drawn *behind* the deck — a band drawn
between two depth batches popped every billow crossing the split depth.

### D4 — Composition derives from the live frame size, not 640×480

`cx`, `cy`, `horizonY`, and `f` are computed from the frame actually in
effect at boot (and the layout is center-relative thereafter), so non-4:3
windows get a correct composition instead of a stretched 4:3 one. This is
the boot-screen promotion insurance; the gallery embed is 4:3 either way.

### D5 — Nothing reaches the camera; no fly-through beat

Billows rising above eye level only spawn clear of the flight lane
(`|x| − halfWidth > 3`); lane billows are capped below the eye, so they
slide off the frame bottom before they could engulf the camera. Deliberate
scope; a wash beat can be added later without structural change.

### D6 — Logo is recorded after the deck and flare, floating in the sky

`efx_logo.png` copied into the sample dir, `loadImage` → `createTexture`,
centered in the frame (0.30 of frame width) over the deck, with a gentle
bob (period 3.5 s, ~1.2% of frame height). Rejected: rendering the
wordmark with the text API — it cannot reproduce the hibiscus mark.

### D7 — Flash pass: sun, flare, logo effects, bloom

All additive 2D quads from small baked alpha textures: a sun glow with
slowly rotating god rays recorded before the deck (so billows occlude
them), a lens flare (core, anamorphic streak, ghosts along the
sun→center axis) after it. The logo pops in with an elastic overshoot, an
additive white flash and an expanding shock ring, then gets a soft
drop shadow (ring of faint offset copies), a halo, twinkling star glints,
and a periodic diagonal shine sweep. The shine is masked by the logo's own
alpha by re-drawing the logo additively in 3×6-texel `sourceRect` cells
near the sweep line (3×6 balances the line's 1 : 0.5 slope; 8×8 cells
showed stair-steps). The frame finishes with `bloom` (threshold 0.86) and
a light `colorFilter` (saturation/contrast) via `setPostEffects`.

### Unmanaged resources

No new engine exposure: the sample only creates engine-managed,
GC-finalized opaque resources through public factories (`createTexture`;
ADR 0011/0012 discipline). None need explicit `destroy()` — they live for
the program's lifetime; no fixed-bank resources are touched.

## Risks / Trade-offs

- [Minified far sprites shimmer without mipmaps] → `mipmaps: true` on all
  cloud textures; the art is intentionally low-contrast and soft.
- [Per-frame JS work (sort + sprite entries) on quickjs/web] → ~900
  billows sorted in place (nearly sorted frame to frame), one pooled
  `drawSprites` batch, no per-frame allocation; the shine batch
  (≤ ~2.5k pooled cells) runs only 0.9 s of every 4 s.
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
