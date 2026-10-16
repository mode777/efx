# 0062 — Image heightfields build natively from decoded pixels behind a prelude-validated bag

Status: Accepted

## Context

Scripts could not turn height data (a heightmap image, a `noise.imageData`
field) into geometry: `EfxImageData` deliberately exposes no pixel readback,
a 512×512 heightmap is ~262k vertices whose float arrays would take seconds
to churn through un-JIT'd QuickJS, and hand-rolled JS grid builders cannot
guarantee the cross-target determinism the golden-image contract (ADR 0020)
demands. The `image-heightfield` change added the script surface; this
record pins where the conversion lives and the geometry contract. Process
record: `openspec/changes/image-heightfield/`.

## Decision

`efx.graphics.makeHeightfield(image, opts?)` is a **native builder behind a
shared-prelude option-bag validator** (ADR 0049): `__efxMakeHeightfield` in
`src/prelude/prelude.js` validates the bag once and passes only scalars, the
image handle, and a material wire block to the marshal-only wire
(desktop `efx_js_make_heightfield_wire` in `src/api/api_3d.c`, web
`efx_bridge_make_heightfield` in `src/web/bridge_render3d.c`). The geometry
lives in the pure-C TU `src/render/render_heightfield.c`, which reads the
decoded RGBA8 pixels and feeds `efx_surface_src` → `efx_meshdata_create` —
no bulk pixel or vertex data ever crosses a JS boundary.

The geometry contract is pinned: Rec. 709 luminance (alpha ignored) times
`height` as vertex y; `region` clamps to the image before sampling and
`step` decimates (`cols = floor((rw−1)/step) + 1`); the field lies in the
XZ plane facing `+Y`, centered, with uv mapped over the **full image** so a
region keeps the un-cropped world-texture alignment; normals come from
central differences (one-sided at edges) and a locally flat area yields the
exact bits `(0, 1, 0)`. The TU compiles with **FP contraction disabled**
(`-ffp-contract=off`; MSVC `/fp:precise` never contracts) — the same
receipt as the noise core (ADR 0059) — so vertex streams are byte-identical
across targets, proven by bit-exact unit tests in `tests/unit/heightfield_tests.c`
and the `heightfield` golden scene.

## Consequences

- Terrain-class meshes build in milliseconds on every target, and anything
  an `EfxImageData` can express (loaded PNG/JPEG, `createImageData`,
  `noise.imageData`) is a valid source — no new resource kinds.
- The no-pixel-readback rule stays intact: this TU is the only sanctioned
  consumer of `EfxImageData` pixels outside the texture path; adding a JS
  pixel getter later is a separate, measured decision.
- The determinism contract binds future edits to the builder: any formula
  change (luma weights, normal scheme, uv mapping) is a golden re-baseline
  per ADR 0020 and must re-pin the unit receipts — not a refactor.
- Material binding reuses the existing 19-float/5-double material wire
  block; surface-less MeshData semantics (ADR 0052) apply unchanged.

## Rejected alternatives

- **Pure-JS builder over a new pixel readback** (`imageData.pixels`) —
  would add bulk pixel egress and push a ~2M-float loop through un-JIT'd
  QuickJS (seconds at 512×512); the sibling prims stay pure JS only because
  they are small.
- **Fully native member with native option validation** — would fork the
  error catalog and duplicate bag rules per binding; ADR 0049 puts
  cold-path validation in the shared prelude once.
- **Tolerance-only verification** — hides cross-target drift (FMA
  contraction, libm differences) until a golden fails on one target; the
  bit-exact unit receipt is the stricter, cheaper catcher.
- **uv tiling / slope-splatting / physics-body options** — scope creep for
  a primitive; a script can tile via a material `map` today, and terrain
  collision is a `physics` follow-up that can consume the same heights.
