# 0059 — Script noise is a stateless FastNoise Lite surface with raw per-type ranges

Status: Accepted

## Context

Scripts had no procedural-generation primitive: terrain heightmaps, generated
textures, and motion variation required every game to hand-roll noise, and
script-side implementations cannot guarantee the cross-target determinism the
golden-image contract (ADR 0020) demands of anything that reaches a rendered
pixel. The `noise-api` change added the script-facing surface; this record
pins the library choice and the API shape. Evaluation record:
`openspec/changes/noise-api/design.md`.

## Decision

The noise engine is the **official FastNoise Lite C99 port**, vendored as a
pinned single header (`vendor/fastnoise-lite/FastNoiseLite.h`, MIT, ADR 0006)
and compiled once as a relaxed-warnings library behind the stateless wrapper
`src/noise/efx_noise.c` — the same vendor pattern as miniz/cgltf/dr_libs.

The script surface `efx.noise` (`src/api/api_noise.c` + shared prelude) is
**stateless**: pure functions of a seed, coordinates, and an options bag.
There are no generator objects, no resource class, and no unmanaged
resources — ADR 0011/0012 do not apply to this surface.

Values **pass through raw per type**, never normalized: gradient/value types
return approximately `[-1, 1]`; cellular returns the nearest-feature distance
in FastNoise Lite's centered convention (`distance − 1`, approximately
`[-1, 0.5]` in practice). The `imageData` helper carries an explicit
`domain` option (default `[-1, 1]`) so byte mapping never assumes a range.

The native surface is minimal by layering (ADR 0049): point queries
(`noise2`/`noise3`) and one bulk fill (`fill2`, row-major into a
`Float32Array`, `out[row·w+col] ≡ noise2(col, row)`) — octave stacking and
domain warp happen inside the engine. Everything else, including the
`imageData` helper, is pure JS in the shared prelude over existing entry
points.

Cross-target determinism is a pinned contract with two receipts: exact-bit
values in `tests/unit/noise_tests.c`, and a committed noise golden scene
gated on all four targets. The noise core must stay off any fast-math flag
and must not adopt trig-based hashing (`sin` is not bit-identical across
libm/wasm).

## Consequences

- Scripts get seeded, reproducible noise on all four targets; anything a
  script renders from noise is golden-verifiable.
- `fill2`'s fill ≡ query identity is now spec-pinned — changing the
  evaluator or its call pattern is a behavior change, not a refactor.
- Cellular's centered (shifted) range is forever part of the script-facing
  contract; changing it would break every domain-mapped texture.
- New noise types or warp flavors are additive spec deltas; distance-only
  cellular (no F2/cell-color) keeps the documented range story small.
- Any future noise-engine change must re-pin the determinism unit values
  and regenerate the golden scene.

## Rejected alternatives

- **stb_perlin.h** — would extend the existing stb pin, but is Perlin-only
  (no simplex, cellular, or warp) and its permutation-offset seeding is
  awkward to expose as a clean u32 seed.
- **FastNoise2** — C++ with explicit SIMD and a node-graph surface; wasm
  SIMD friction and scope far beyond a fixed-function player's needs.
- **libnoise** — LGPL; static-link obligations are a non-starter (same
  reason libmpg123 was rejected for F14).
- **Hand-rolled hash noise** — full control, but the engine owns algorithm
  correctness and cross-target behavior forever; the vetted MIT port wins.
- **A `Noise` generator class** — per-call setup savings nobody measured,
  at the cost of a full ADR 0011 resource lifecycle in both bindings.
- **Normalized (remapped) output ranges** — lossy for cellular and silently
  different from FastNoise Lite references scripts may consult.
