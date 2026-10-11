# Proposal

## Why

Scripts have no procedural-generation primitive: terrain heightmaps, generated
textures, and motion variation are impossible without hand-rolled noise in
every game, and script-side implementations cannot guarantee the
cross-target determinism the golden-image contract (ADR 0020) already demands
of engine behavior. Engine-provided, seeded noise closes that gap once, for
all scripts, on all four targets.

## What Changes

- Vendor the **official FastNoise Lite C99 port** (MIT, single header, pinned
  per ADR 0006) as the noise engine: OpenSimplex2/2S, Perlin, Value,
  Value Cubic, Cellular; fractal (fBm/ridged) variants; OpenSimplex2 and
  gradient domain warp; u32 seeding.
- Add a small pure-C noise module (stateless, seed-in-args — no generator
  objects, no unmanaged resources) compiled into `efx_core`.
- Expose a new `efx.noise` sub-namespace (ADR 0050 organization rule):
  - `efx.noise.noise2(x, y, opts?)` / `efx.noise.noise3(x, y, z, opts?)` —
    point queries returning **raw, unnormalized** generator values.
  - `efx.noise.fill2(buf, w, h, opts?)` — one native call writing `w*h`
    values row-major into a `Float32Array`, octave stacking done in C.
  - `efx.noise.imageData(w, h, opts?)` — **pure-JS prelude helper**
    composing `fill2` + value→grayscale remap + the existing
    `createImageData`; takes an explicit `domain` (default `[-1, 1]`).
- Add a determinism **golden scene** (three blitted noise textures:
  fractal simplex, raw cellular, domain-warped — distinct seeds) gated on
  all four targets per ADR 0020.
- New ADR (`docs/decisions/0059`), `gallery/src/api/efx.d.ts` surface,
  regenerated `docs/api/`, updated `docs/js-api.md` if any guideline-level
  rule is touched, `vendor/README.md` pin row, prelude regen
  (`tools/gen_prelude.py`).

## Capabilities

### New Capabilities

- `noise`: the script-facing deterministic noise surface — stateless
  seed-in-args point queries and bulk fills with raw per-type value ranges,
  the pure-JS ImageData helper with its `domain` remap, option validation,
  and the cross-target determinism contract proven by a golden scene.

### Modified Capabilities

<!-- none: js-api already covers sub-namespace membership, two-layer
     structure, and option-bag validation generically; no js-api
     requirement text changes. -->

## Impact

- **Code**: new `src/noise/` module (vendored FastNoise Lite impl TU with
  relaxed warnings + thin `efx_noise_*` wrapper); `src/api/api_noise.c`
  wire natives; desktop binding namespace registration (audio/physics
  pattern); web bridge `src/web/js/noise.js` fragment + `CMakeLists.txt`
  concat order; `src/prelude/prelude.js` (+ `gen_prelude.py` regen).
- **Vendor**: `vendor/fastnoise-lite/` (C99 header + LICENSE),
  `vendor/README.md` pin row.
- **Script API**: new `efx.noise` members; `gallery/src/api/efx.d.ts` +
  `docs/api/` regeneration (ADR 0048 flow); type-test additions.
- **Verification**: new unit tests (determinism, ranges, fill contract),
  one golden scene added to the ADR 0020 harness (llvmpipe captures per
  `docs/verification-server.md`), four-target gate.
- **Docs**: ADR 0059 + `docs/decisions/README.md` index; `vendor/README.md`.

## Non-goals

- No 4D noise or time dimension (scripts z-slice for animation).
- No Cellular F2 / F2−F1 or cell-color (hash lookup) return channels —
  distance (F0) only.
- No standalone `warp2()` verb — domain warp is an option on queries/fills.
- No tileable/wrap noise, SIMD paths, or threading.
- No `Noise` resource class or generator objects — the surface is stateless
  by design.
- No script-visible shader/noise integration — the fixed-function contract
  (ADR 0015) is untouched.
- No curated gallery sample in this change (can follow separately).
