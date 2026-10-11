# Design

## Context

The noise surface slots into an established pattern: a vendored single-header
library behind a thin pure-C wrapper (miniz, cgltf, dr_libs), wire natives in
`src/api/`, a domain sub-namespace the bindings create and the prelude
augments (audio/physics/`efx.io` pattern, ADR 0050), and cold-path option
validation written once in the shared prelude (ADR 0049). Goldens gate
behavior on all four targets (ADR 0020). See proposal.md for scope.

## Goals / Non-Goals

Goals:

- One deterministic noise engine shared by desktop quickjs and the web bridge,
  bit-identical everywhere.
- Zero unmanaged resource exposure — noise adds no lifecycle burden.
- Minimal native surface: point queries + one bulk fill; everything else is
  prelude JS.

Non-goals: see proposal.md (4D, cellular F2/cell-color, standalone warp,
tileable noise, gallery sample).

## Decisions

### D1 — Vendor the official FastNoise Lite C99 port

MIT, single header (`FastNoiseLite.h` in the repo's `C/` directory), no
external deps, u32 seeding, and the full algorithm menu scripts want
(OpenSimplex2/2S, Perlin, Value, ValueCubic, Cellular; fBm/ridged fractals;
two domain-warp flavors). It is the project's own maintained C port, not a
community fork, and follows the miniz/cgltf vendoring playbook exactly.

Rejected:

- **stb_perlin.h** — would extend the existing stb pin, but is Perlin-only
  (no simplex, no cellular, no warp), and its seeding story (permutation
  offsets) is awkward to expose as a clean u32 seed. Breadth is the point of
  a scripting API.
- **FastNoise2** — C++ with explicit SIMD; the wasm story (SIMD128
  translation, feature detection) and the node-graph surface are way outside
  a fixed-function player's needs.
- **libnoise** — LGPL; static-linking obligations are a non-starter here
  (same reason libmpg123 was rejected for F14).
- **Hand-rolled hash noise** — full control, but we own algorithm
  correctness, quality, and cross-target behavior forever; FNL is vetted and
  MIT.

Implementation shape follows the vendor pattern: implementation TU
`src/noise/fastnoise_impl.c` (compiled once, warnings relaxed like the
miniz/cgltf/stb TUs) + thin wrapper `src/noise/efx_noise.c` exposing a
plain-C, seed-in-args surface; `efx_core` links it on all four targets.

### D2 — Stateless functions; no resource class

Noise state is a u32 seed — data, not identity. All state lives in the call
arguments, so there is nothing to finalize, destroy, or track. The ADR 0011
GC-finalized-class machinery and ADR 0012 discipline simply do not apply:
the noise surface exposes **no unmanaged resources at all**. This is a
deliberate contrast with textures/meshes: those own GPU memory; a noise query
owns nothing. Rejected: a `Noise` class caching a configured generator — it
would buy per-call setup savings nobody has measured, at the cost of a whole
resource lifecycle in the API, the bindings, and the web bridge.

### D3 — Raw passthrough with per-type documented ranges

Queries return the generator's value untouched. FastNoise Lite's ranges are
approximately `[-1, 1]` for gradient/value types; its cellular distance is
**centered** like the others — the port returns `distance0 - 1`, so raw
cellular lands in approximately `[-1, 0.5]` (typically `[-1, ~0.15]` for the
default squared-distance function; verified empirically at the pinned
revision). (Discovered during implementation: the classic Voronoi
`[0, 1]` assumption was wrong for this library; spec and wrapper docs
record the actual convention.) Normalizing into one range would be lossy
for cellular (distance 0 at the cell edge is meaningful) and would quietly
break every script that already knows what FNL output looks like. The
asymmetry is handled *once*, at the ImageData helper, via the explicit
`domain` option (D5).

### D4 — One bulk native (`fill2`); octaves in C

A 256×256, 5-octave heightmap is ~328k sample evaluations. Through the
quickjs boundary (or worse, the wasm boundary) that is unacceptable
per-call; inside one native call it is single-digit milliseconds. Octave
stacking and domain warp therefore happen inside `fill2`'s loop. Point
queries share the same evaluator, so `fill2` output is element-wise
identical to repeated `noise2` calls (spec-pinned — it is also the unit
test). Rejected: JS-side octave stacking over a single-octave native —
per-octave-per-sample boundary crossings, and duplicate fractal logic that
the two-layer rules would then force us to keep in sync.

### D5 — ImageData helper: pure JS with an explicit `domain`

The helper composes `fill2` → value→byte remap → existing `createImageData`
(`Uint8Array` accepted since F2). No new native. The `domain` option
(default `[-1, 1]`) exists because D3's raw passthrough makes the byte remap
domain-dependent: cellular at the default domain would render half-dark.
Callers who know their type's range say so: `domain: [0, 1]`. Rejected:
fixed-domain remap (silent misrender for cellular); normalizing fill output
instead (violates D3); a per-pixel JS callback remap (boundary crossings,
nondeterministic user code, v1 complexity).

### D6 — Cellular distance (F0) only

Scripts get `cellular` as distance to the nearest feature point. F2/F2−F1
and cell-color (hash lookup) stay unexposed — they multiply the option
surface and the documented range story for niche returns. If a style needs
Voronoi cell IDs later, that is a small additive spec delta.

### D7 — Determinism policy

FastNoise Lite derives gradients by integer hashing (no trig), computes in
`float`, and has no platform-varying operations — bit-identical output
across x86-64, arm64, and wasm is expected. Expected is not proven: the
golden scene (below) is the proof, and the wrapper pins `float` evaluation
(no `double` promotion drift, no fast-math on the noise TU; the build never
enables fast-math). Trig-based GLSL-style hashing is rejected outright —
`sin` is not bit-identical across libm implementations. If a future change
ever touches the noise engine, the golden is the regression gate.

### D8 — Surface and layering

`efx.noise` is a domain sub-namespace per the ADR 0050 organization rule
(bindings create the object, prelude augments it), mirroring
`efx_api_register_audio`/`register_physics` on desktop and a
`src/web/js/noise.js` fragment on web. Argument shape follows ADR 0053:
positional requireds first (`buf, w, h` / `x, y[, z]`), optional bag last.
Cold-path bag validation lives in the prelude behind the private `natives`
object (ADR 0049); the natives stay marshal-only. Defaults pinned here and
in the spec: `seed 0`, `type 'opensimplex2'`, `frequency 1`, `octaves 1`,
`lacunarity 2`, `gain 0.5`, `warp 'none'`. (`frequency 1` rather than FNL's
`0.01` so unconfigured queries operate at unit scale; scripts scaling a
terrain set it explicitly.)

### D9 — Golden scene

One 2D scene, three quads blitted from noise-generated textures via
`fill2` → `imageData` → `createTexture` → `drawQuad`:

- simplex fBm (4 octaves, seed 7), `domain [-1, 1]`
- raw cellular (seed 9), `domain [-1, 0.15]` (the FNL centered distance's
  envelope in practice — see D3)
- domain-warped value fBm (seed 13)

Three textures cover the fractal path, the cellular range story, warp, and
three independent seeds; every pixel of the image is a deterministic
function of the noise engine. Captured on the verification server's
llvmpipe per `docs/verification-server.md`, gated on all four targets under
ADR 0020 tolerances (the noise values themselves must agree exactly; the
tolerance absorbs rasterizer jitter, not noise drift).

## Risks / Trade-offs

- [FNL C99 port is a language port, not the reference C++ implementation]
  → It lives in the FastNoise Lite repo and is the maintained C surface;
  the golden scene catches any cross-target divergence the port might hide.
- [float32 in wasm vs desktop] → Both are IEEE-754 with round-to-nearest;
  no x87 80-bit paths on any current target; golden proves it. Keep the
  noise TU off any future fast-math flag.
- [Raw passthrough surprises scripters who expect [0, 1]] → The type-test
  and docs/api entries state each type's range; the helper's `domain`
  makes the mapping explicit at the one place bytes are produced.
- [fill2 mutates a caller buffer] → Spec pins write-count and the
  reject-untouched behavior for undersized/non-Float32Array inputs.
- [Web point-query latency] (wasm boundary per call) → Documented guidance:
  per-frame queries are fine in the dozens-to-hundreds; bulk work belongs
  in `fill2`.

## Migration Plan

Purely additive API. No existing surface changes; rollback is reverting the
change. The only shared-file edits are registration lines, the CMake
fragment list, prelude regen, the d.ts, and generated docs.

## Open Questions

- Whether a curated gallery sample showcasing noise should follow as its own
  change (proposal non-goal for now).
