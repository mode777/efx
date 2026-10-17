# Design: image-heightfield

## Context

The `make*` primitive family (`makeCube`/`makePlane`/`makeSphere`/
`makeCapsule`) is pure JS in the shared prelude: it builds plain number
arrays and feeds them to the native `createMeshData` wire
(`src/prelude/prelude.js`, `__efxMakePlane` et al.). That works because
primitives are small. A heightfield is not: a 512×512 image is ~262k vertices
(~2.1M floats + ~786k indices), and `EfxImageData` deliberately exposes no
pixel readback to JS. Building this in JS would mean (a) adding a bulk pixel
transfer across the quickjs boundary and (b) doing the whole grid loop in
un-JIT'd QuickJS — both slow and both new surface area. The decoded RGBA8
pixels already live in native memory (`src/resource/image.h`,
`efx_image.pixels`), and the CPU meshdata pipeline
(`efx_surface_src` → `efx_meshdata_create` → `efx_meshdata_set_material`,
`src/render/render.h`) is directly feedable from C. The noise change set the
precedent for exactly this shape: heavy/bulk work native, option bags
validated once in the shared prelude (ADR 0049), determinism pinned by
contract (ADR 0059).

## Goals / Non-Goals

Goals:

- One new `efx.graphics` member that turns image luminance into a
  single-surface MeshData with a pinned, reproducible layout.
- Sub-area (`region`) and decimation (`step`) support without extra API.
- Zero bulk data crossing the JS boundary; millisecond-scale builds at
  512×512 on the desktop runtime.
- Byte-identical vertex streams on all four targets (golden-verifiable).

Non-Goals (see proposal for the product-level list):

- No generic `EfxImageData.pixels` readback — this change adds no pixel
  egress; revisit separately with a measured need.
- No physics body generation, splat maps, smoothing filters, or uv tiling.

## Decisions

### D1 — Native builder behind a prelude-validated wrapper (ADR 0049)

`efx.graphics.makeHeightfield` is installed by the shared prelude like the
other prims, but its body validates the option bag once and delegates to a
marshal-only wire native:

```
__efxMakeHeightfield(image, opts)          /* prelude.js */
  → validate image is an EfxImageData, opts per family rules
  → natives.makeHeightfieldWire(image, rx, ry, rw, rh, step, height, size, material)
      desktop: efx_js_makeHeightfield_wire (src/api/api_3d.c)
      web:     render3d.js fragment → exported C function
  → native: clamp region, build efx_surface_src, efx_meshdata_create,
    efx_meshdata_set_material(0, …), return the MeshData object
```

Why not pure JS like the sibling prims: the sibling prims generate ≤ a few
thousand vertices; a heightfield is bulk geometry, and doing it in JS
requires the bulk pixel readback this change explicitly avoids (see
proposal Non-goals). Why not expose `makeHeightfield` fully native with
native option validation: ADR 0049 pins cold-path option-bag validation in
the shared prelude so both runtimes share one validation and one error
catalog; the wire stays marshal-only (scalars + the image object + material
object).

Validation split: the prelude wrapper throws the family `TypeError`/
`RangeError`s (unknown fields, non-integer step, degenerate region after
clamping is checked natively where the clamp happens — the wire returns the
same `RangeError` taxonomy). The wire re-checks marshallable types
defensively, matching the existing wire functions.

### D2 — Builder TU: `src/render/render_heightfield.c`

Pure C11, no quickjs/sokol: input `const efx_image*` + a plain options
struct, output `efx_meshdata*` via `efx_meshdata_create` +
`efx_meshdata_set_material`. This keeps the whole geometry contract unit-
testable headless (the same pattern that makes `src/noise/` testable) and
reuses the existing CPU meshdata path — no new GPU code, no display-list
changes, fixed-function untouched (ADR 0015).

Rejected: putting the builder in `api_3d.c` — the api layer is quickjs-
facing wiring; geometry with a determinism contract belongs beside
`render_mesh.c` where meshdata lives.

### D3 — Geometry and determinism

Pinned formulas (unit receipts assert exact values):

- Grid: `cols = floor((rw−1)/step) + 1`, `rows` analogously; row-major
  vertices `(i + j·cols)`; quad indices in the `makePlane` pattern
  `(a, d, e, a, e, b)`, CCW seen from +Y.
- Height: `y = luma(px) · height`, Rec. 709 luma on 0..1 channels, alpha
  ignored.
- Extents: `xSpan = size` (default `rw`, i.e. 1 unit/pixel),
  `zSpan = size · rh / rw`; centered on the origin; `y` as sampled.
- uv: `(px/(imgW−1), py/(imgH−1))` of the full image — corner-aligned like
  the family's 0..1 mapping; region calls keep world-texture alignment.
- Normals: `n = normalize(cross(Δz, Δx))` over neighboring sampled heights
  (central differences interior, one-sided at edges, Δ in world units),
  sign fixed so a flat field yields exactly `(0, 1, 0)`.

The TU compiles with `-ffp-contract=off` (MSVC `/fp:precise` never
contracts) — same receipt as ADR 0059: Apple arm64 otherwise FMA-contracts
the luma/normal math by 1 ulp and breaks byte-identical streams. Unit tests
pin exact values; the golden scene is the cross-target receipt.

Rejected: tolerance-only verification — the repo's noise precedent showed
bit-exact unit receipts are the stricter, cheaper catcher; tolerance hides
cross-target drift until a golden fails on one target.

### D4 — Golden scene via `loadImage`, not noise coupling

One committed tiny grayscale heightmap PNG (e.g. 64×64) in the golden scene
directory; the scene loads it, builds a heightfield, lights it with the
directional light, and captures one frame. Using a real PNG exercises the
actual path (decode → luminance → mesh) with no coupling to the still
unarchived `noise-api` change; `noise.imageData`/`createImageData` inputs
work identically (same `EfxImageData` type) and stay covered by unit tests.

### D5 — Resource lifecycle: nothing new

The input image is a caller-owned `EfxImageData` (the engine never mutates
or retains it beyond the call); the output is the existing `EfxMeshData`
GC-finalized opaque class with explicit `destroy()` (ADR 0011/0012). No new
resource class, no fixed bank. A destroyed input image throws `TypeError`
in the wrapper (same check style as `natives.checkMesh`).

### D6 — Type doc and reference flow

`gallery/src/api/efx.d.ts` gains `MakeHeightfieldOptions` and the method
(next to `makeCapsule`), with `efx.type-test.ts` coverage; `docs/api/` is
regenerated (`npm --prefix gallery run docs:markdown`); prelude regen via
`tools/gen_prelude.py` after the prelude edit. ADR 0062 records the native-
builder decision, the luminance/normal contract, and the determinism
policy.

## Risks / Trade-offs

- [Huge image → huge mesh (a 4096² image is ~16.7M vertices, ~800 MB of
  vertex data)] → `efx_meshdata_create`'s existing allocation-failure
  path propagates as a JS `Error`; `step` is the documented decimation
  lever; the reference entry notes the cost.
- [Region clamping hides typos (e.g. `w` past the image edge silently
  clamps)] → spec pins clamp-then-degenerate-check behavior; a region that
  clamps to nothing throws `RangeError`, so gross errors still surface.
- [Cross-target float drift in luma/normals] → D3's `-ffp-contract=off` +
  bit-exact unit receipts; no fast-math flags on the TU.
- [Validation drift between prelude wrapper and native wire] → single
  validation site in the prelude (ADR 0049 pattern); wire marshals and
  builds only.
- [noise-api still in flight shares the `EfxImageData` producer story] →
  D4 decouples the golden scene; no ordering dependency either way.

## Migration Plan

Purely additive: one new `efx.graphics` member, no renames, no default
changes anywhere. Ship behind nothing; the four-target gate (ADR 0020) with
the new unit tests + golden scene is the rollout. Rollback = revert the
single change commit set; nothing persists.

## Open Questions

None — the option surface and geometry contract are pinned in the spec
delta; remaining choices (TU flag spelling per compiler, exact index
emission) are implementation-level and covered by the unit receipts.
