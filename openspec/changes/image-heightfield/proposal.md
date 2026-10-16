# Proposal

## Why

Scripts cannot turn height data into geometry: terrain generation today means
hand-rolling a grid builder in JS and piping noise or an image's pixels
through it. Reading pixels from `EfxImageData` is not even possible (the
class exposes only `width`/`height`), and pushing a 512×512 heightmap
(~262k vertices, ~2M floats) through QuickJS array marshaling is far too slow
for load-time terrain. Engine-provided image → mesh generation closes this
once, natively, on all four targets — the PS2-era terrain use case the vision
names.

## What Changes

- Add `efx.graphics.makeHeightfield(image, opts?)` — the first primitive in
  the `make*` family that takes a resource argument: a required `EfxImageData`
  (loaded via `loadImage`, generated via `noise.imageData`/`createImageData`)
  whose **per-pixel luminance** becomes vertex height. Returns a
  single-surface `EfxMeshData`, directly usable by `createMesh`, with the
  same option-bag conventions as the other prims (`material` binding at
  creation, `RangeError`/`TypeError` validation).
- Option surface (suggestion, pinned in the spec delta):
  - `region` — `[x, y, w, h]` pixel sub-rect of the image (default: whole
    image); the requested "subarea" support.
  - `step` — positive integer decimation stride (default `1`): samples every
    `step`-th pixel in x and y.
  - `height` — world-space height scale (default `1`); luminance 0 maps to
    y = 0, luminance 1 maps to y = `height`.
  - `size` — world-space x extent (default: 1 world unit per pixel, i.e.
    the region's pixel width); the z extent follows the region aspect so
    texels stay square.
  - `material?` — bound to the single surface at MeshData creation, exactly
    like the other `make*` prims.
- Geometry contract: field lies in the XZ plane facing `+Y` (like
  `makePlane`), centered on the origin; uv maps the region's normalized
  sub-rect of the **full image** (full-image default → 0..1), so
  `loadImage(path)` textures the field with no rewrapping; normals are
  derived from the sampled heights by central differences (edge-clamped).
- Native builder behind the established layering (ADR 0049): the option bag
  is validated once in the shared prelude (`__efxMakeHeightfield`), a
  marshal-only native reads the decoded RGBA8 pixels, builds the
  `efx_meshdata` directly, and returns it — no bulk pixel or vertex arrays
  ever cross the JS boundary. Wired identically on the desktop quickjs
  binding and the web bridge.
- Determinism policy as for noise (ADR 0059): the builder TU compiles with
  FP contraction disabled so golden captures stay reproducible across
  targets.
- New ADR (`docs/decisions/0062`), `gallery/src/api/efx.d.ts` surface +
  type-test additions, regenerated `docs/api/`, prelude regen
  (`tools/gen_prelude.py`).

## Capabilities

### New Capabilities

<!-- none: the heightfield is a new member of the existing make* primitive
     family, specified inside the existing capabilities; it does not
     introduce a new engine capability area. -->

### Modified Capabilities

- `3d-core`: new requirement "Image heightfield primitive" alongside
  "Procedural primitives" — pins the `makeHeightfield` geometry contract
  (XZ field, luminance→height, region/step/height/size semantics, uv
  mapping, central-difference normals), option validation and material
  binding, and mesh layout determinism.
- `js-api`: MODIFIED "Graphics namespace API" — `makeHeightfield` joins the
  enumerated `efx.graphics` member list (the requirement pins the exact
  membership).

## Impact

- **Code**: native heightfield builder (new TU beside the meshdata code in
  `src/render/`, `-ffp-contract=off`, unit-tested); `src/api/api_3d.c` wire
  native; web bridge fragment (`src/web/js/`, concat order in
  `CMakeLists.txt`); `src/prelude/prelude.js` wrapper + `gen_prelude.py`
  regen.
- **Script API**: one new `efx.graphics` member; `gallery/src/api/efx.d.ts` +
  `gallery/src/api/efx.type-test.ts`; `docs/api/` regeneration (ADR 0048
  flow; `npm --prefix gallery run docs:markdown`).
- **Verification**: new unit tests (grid layout, luminance mapping, region
  clamping, step, uv, error contract); one deterministic golden scene
  (heightfield from a `noise.imageData` field — no new binary asset needed)
  added to the ADR 0020 harness; four-target gate per ADR 0020/0023.
- **Docs**: new ADR `docs/decisions/0062-*` + index entry in
  `docs/decisions/README.md`; `docs/js-api.md` only if a guideline-level
  rule changes (none anticipated — ADR 0049/0053 patterns are reused as-is).

## Non-goals

- No generic `EfxImageData` pixel readback to JS (no `imageData.pixels`
  getter) — bulk pixel transfer across the JS boundary is exactly what this
  change avoids; revisit only with a measured need.
- No vertex-color splatting (e.g. slope-based material blends), no
  multi-material terrain slicing — the surface stays single-material.
- No uv tiling option (the `uvs` span of `makePlane`) — uv is pinned to the
  image's normalized region; tiling a texture over a heightfield is a
  one-line material `map` change today.
- No collision/physics body generation from the field (a `physics` follow-up
  can consume the same heights).
- No morphological smoothing/erosion/gap-filling filters — the image is the
  data; preprocessing stays in script land (noise + `createImageData`).
- No scripted per-vertex callback API — the mapping is fixed luminance, not
  a user transform.
