# Proposal

## Why

The texture API currently offers two overlapping creation paths: the
primitive `createTexture(loadImage(path), opts)` and a pure-JS
`loadTexture(path)` convenience that composes it. Because `loadTexture`
takes no options it silently drops the sampler controls `createTexture`
gained in F6b, so a loaded image cannot be clamped, nearest-filtered, or
(given the new work) mipmapped without abandoning the convenience. This
also makes the API inconsistent with the mesh path, where `loadMeshData`
produces data and `createMesh` turns it into a GPU resource — there is no
`loadMesh` convenience. Removing `loadTexture` leaves one obvious flow and
lets every loaded texture receive creation options. Separately, `Texture`
creation still has no mipmaps, so minified textures alias badly; F6b
deferred mipmaps explicitly, and this change closes that gap.

## What Changes

- **Remove `efx.loadTexture(path)` — BREAKING.** The pure-JS convenience
  is deleted. The single documented flow becomes
  `efx.createTexture(efx.loadImage(path), opts?)`, mirroring
  `createMesh(loadMeshData(path))`.
- **Add a `mipmaps` texture option.** `createTexture(imageData, opts?)`
  gains `mipmaps?: boolean` (default `false`). When `true` the engine
  builds a full mip chain at upload and minifies with mipmap filtering;
  `filter` still selects `'linear'` (default) or `'nearest'`, with
  `mipmaps: true` upgrading the minification filter to its mipmap variant
  (`linear` → trilinear, `nearest` → nearest-mipmap). `mipmaps: false`
  preserves today's behavior exactly.
- **Keep the existing sampler options.** `wrap`
  (`'repeat'`/`'clamp'`/`'mirror'`, default `'repeat'`) and `filter`
  (`'linear'`/`'nearest'`, default `'linear'`) are unchanged; only the
  mipmap dimension is added. Unknown fields/values still throw `TypeError`.
- **Update the in-tree callers.** The `load_png` golden, the gallery
  `texture-showcase` sample, and the F6a resource smoke test are rewritten
  to the composed flow; the golden output stays byte-identical.
- **Docs/contract.** `docs/js-api.md`, `gallery/src/api/efx.d.ts`, and the
  gallery type-test gain the new flow and option; the gallery
  `texture-showcase` sample is updated to the new flow.

## Capabilities

### New Capabilities

_None._

### Modified Capabilities

- `2d-layer`: the "Image and texture resources" requirement gains the
  optional `mipmaps` flag on `createTexture` (build-and-use semantics,
  default off) and its validation, alongside the existing `wrap`/`filter`.
- `js-api`: the "Resource-loading API" requirement drops the
  `loadTexture` convenience and states that the resource→texture flow is
  `createTexture(loadImage(path), opts?)`; `loadText`/`loadImage` remain
  the C-layer loaders.

## Impact

- **Code:** `src/api/api.c` and `src/web/entry.js` (parse `mipmaps`,
  remove `loadTexture`), `src/web/bridge.c` (bridge arg), `src/render/
  render.[ch]` and `src/platform/pipeline.[ch]` (mip-chain creation +
  mipmap-aware sampler cache), `src/prelude/prelude.js` (delete the
  convenience; regenerate `src/prelude/prelude.h` via
  `tools/gen_prelude.py`).
- **Docs:** `docs/js-api.md`, `gallery/src/api/efx.d.ts`,
  `gallery/src/api/efx.type-test.ts`, new
  `docs/decisions/0034-texture-mipmaps.md` + `docs/decisions/README.md`
  index row, and the AGENTS.md current-state note.
- **Tests/samples:** `tests/scripts/s_6a_resource.js`,
  `tests/goldens/load_png/main.js`,
  `gallery/samples/curated/texture-showcase.js`, plus unit coverage for
  mipmap option validation (macOS/Windows/Linux/Emscripten).
- **Milestone:** F6 (a revision of the F6a/F6b texture surface, not a new
  roadmap slice). Predecessors F6a–F6d already pass their gates; this
  change's own four-target gate must pass before archive.
- **Verification:** ctest smoke/unit suites everywhere; golden-image suite
  (including a mipmapped scene) on the display-bearing targets; the
  existing four-target gate.

**Non-goals (out of scope):**

- Script-visible mip levels, LOD bias, or per-level control; `mipmaps` is
  a boolean.
- Mipmaps for RenderTargets, or regenerating a mip chain after upload.
- Separate min/mag filters, anisotropic filtering, or sRGB/color
  management.
- Mipmap awareness in the glTF importer: glTF mipmap sampler filters stay
  collapsed to `linear`/`nearest` as in F6b (imported textures get no mip
  chain in this change).
- Keeping a deprecated `loadTexture` shim; the function is removed
  outright.
