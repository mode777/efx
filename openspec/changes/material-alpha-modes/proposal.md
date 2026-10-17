# Proposal

## Why

Meshes have no coherent transparency story. A translucent mesh (vertex-color or
tint alpha below 1) today blends **in place, in submission order, while writing
depth** — so opaque geometry recorded after it is z-killed behind it, and
translucent-vs-translucent order is luck. The material cannot express
transparency at all: `diffuse` alpha is uploaded to the shader but never read,
and the only cutout tool (`alphaMask`, ADR 0027) is an unconditional binary
discard. Scripts cannot say "this surface is glass" — only "this texture has
hard edges".

## What Changes

- **`Material.alphaMode`** — new optional material field (`'opaque'` |
  `'mask'` | `'blend'`, default `'opaque'`), snapshot at binding like every
  other material field:
  - `'opaque'` (default) — today's full path: depth test + depth write
    (`depthWrite` option honored), unsorted, submission order. Fragment alpha
    is **saturated to 1** (glTF OPAQUE semantics); the `alphaMask` map still
    discards below 0.5 (ADR 0027 unchanged).
  - `'mask'` — the alpha-scissor path: same full pipeline (depth test + write,
    unsorted); the `alphaMask` map discards below the fixed 0.5 cutoff; alpha
    saturated to 1.
  - `'blend'` — the transparent pass: after all opaque/mask draws of the pass,
    stable-sorted back-to-front by **record origin** (transform translation
    through the snapshot camera view, record-sequence tiebreak); depth test on,
    depth write **forced off** (`depthWrite` option ignored); fragment alpha =
    `diffuse.a × vertex-color.a × tint.a`; the `alphaMask` map, if bound, does
    **not** discard (mode wins over mask).
- **Blend equations stay orthogonal** — `blend` (`'alpha'`/`'additive'`/
  `'subtractive'`) keeps selecting how fragments combine inside whichever path
  the mode routes to. Routing depends only on `alphaMode`; frame blend state
  and per-surface blend overrides are unchanged (ADR 0054 intact).
- **Playback pass split** — within each render-target pass segment, mesh
  surfaces classified `'blend'` are deferred to a transparent sub-pass;
  quads, sprite batches, text, billboards, and particles are untouched.
- **glTF import mapping** — glTF `alphaMode` `OPAQUE`/`MASK`/`BLEND` maps to
  the matching engine mode; `BLEND` materials honor base-color alpha.
- **Wire growth** — `efx_material` gains the mode; the material wire block
  grows 19 → 20 floats across the five mirrors (prelude, web core, two
  decoders, web stride) with prelude regeneration.
- **BREAKING** — translucent draws that relied on the implicit in-place
  blending of vertex/tint alpha below 1 while leaving the default mode now
  render opaque; they must declare `alphaMode: 'blend'`. Existing scenes using
  alpha < 1 vertex/tint colors on default-mode materials re-baseline.

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `lighting`: "Phong material model" gains the `alphaMode` field, its
  validation, and mode semantics; "F4a lit shading" gains the mode-gated alpha
  composition (saturation rule, `diffuse.a` honored in `'blend'`, mask-discard
  gated to mask-path modes); "Lighting CPU reference" mirrors the alpha rule.
- `3d-core`: "Whole-mesh drawing with depth" gains transparent-pass routing —
  `'blend'` surfaces defer to a sorted, depth-write-free sub-pass per pass
  segment; opaque/mask surfaces keep the full path unchanged.
- `gltf-import`: "PBR-to-Phong material conversion" maps glTF `alphaMode` to
  the engine mode and honors base-color alpha for `BLEND`.

## Impact

- **Code**: `src/render/render.h` (+`alpha_mode`), `src/render/render_records.c`
  / `render_mesh.c` (wire, defaults), `src/api/api.c` (validator),
  `src/api/api_3d.c` + `src/api/api_lighting.c` (block decode), `src/prelude/
  prelude.js` + regenerated `prelude.h`, `src/web/js/core.js`,
  `src/web/bridge_render3d.c`, `shaders/mesh.glsl` + regenerated `shaders/`
  headers, `src/platform/pipeline.c` (pass split, origin sort, one-surface
  replay), `src/resource/gltf.c` (mode mapping), CPU reference (`efx_lighting_shade`)
  and its unit tests.
- **API docs**: `gallery/src/api/efx.d.ts` + regenerate `docs/api/` (standing
  js-api type-doc requirement; no `js-api` requirement text changes, per the
  ADR 0058 / ADR 0054 precedent that material shape is pinned by `lighting`).
- **Docs**: new ADR `docs/decisions/0064-material-alpha-modes-and-transparent-pass.md`
  (routing policy, origin sort key, saturation rule, mode-over-mask rule,
  blend-equation orthogonality) + index update.
- **Goldens**: new scenes (`material_alpha_modes`, `vertex_alpha_blend`);
  verify `blend`, `vertcolor3d`, `map_alpha_mask`, and glTF goldens for
  re-baseline fallout (expected ~zero; the breaking class is exactly the one
  this change makes explicit).
- **Non-goals / out of scope**: per-surface or per-triangle sorting granularity
  (record origin is the v1 key); configurable `alphaCutoff` (ADR 0027 fixed 0.5
  stands); premultiplied alpha, separate per-attachment blend, or any new blend
  equation; transparency routing for quads/billboards/particles; order-independent
  transparency techniques (depth peeling, A-buffers); shader-visible
  customizability of any kind (ADR 0015).
