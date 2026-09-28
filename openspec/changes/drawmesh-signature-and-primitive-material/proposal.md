# Proposal

## Why

`drawMesh` is the one F3 function whose required argument hides inside an
options bag: `drawMesh({ mesh, transform?, color? })`. The runtime does
enforce `mesh` (both bindings throw `TypeError` when it is absent), but the
signature does not show that at a glance, and it is the only draw function
inconsistent with the resource-first convention already used by
`setMeshSurfaceMaterial(mesh, …)` and planned `poseMesh(mesh, …)`. Moving
the mesh to a positional first argument makes the requirement visible and
aligns the call shape with the rest of the resource-taking API.

Separately, the procedural primitives (`makeCube` / `makePlane` /
`makeSphere`) always produce default-material geometry, so the common case —
"make a shape and shade it" — takes two calls
(`createMeshData` → `setMeshSurfaceMaterial`, or a manual `materials`
array). Letting each primitive take an optional `material` collapses that to
one call while reusing the existing per-surface binding path.

Finally, the gallery type document `gallery/src/api/efx.d.ts` mis-describes
the API in ways the change touches: `CreateMeshDataOptions extends
MeshSurfaceData` makes `positions` **required even in batch form**, so the
documented `createMeshData({ surfaces: [...] })` call fails `tsc`; the
`drawMesh` option bag and the primitive option bags will change with this
work. The type document must describe the valid call shapes accurately.

## What Changes

- **BREAKING — `drawMesh` becomes `drawMesh(mesh, opts?)`.** The mesh is
  now the first positional argument and must be a live `Mesh` (a missing,
  non-Mesh, or destroyed mesh throws `TypeError`). The option bag is
  optional and holds only `{ transform?, color? }`; `mesh` inside the bag is
  now an unknown field and throws `TypeError`. All other semantics
  (transform/color validation, value-snapshot, depth-tested whole-mesh
  playback, display-list record) are unchanged. Every call site — examples,
  golden scenes, smoke/unit tests, docs samples, the gallery samples, and
  the type-test — is rewritten. **No golden pixel changes**; committed PNGs
  must be reproduced byte-for-byte.
- **Add an optional `material` to the primitive option bags.**
  `makeCube({ size?, material? })`, `makePlane({ size?, segments?,
  material? })`, `makeSphere({ radius?, segments?, material? })`. When
  present, the material is bound to the primitive's single surface at
  MeshData creation (exactly as `createMeshData({ surfaces: [s], materials:
  [material] })` does today), so it carries to `createMesh`. Omitted or
  `null` selects the engine default material. Unknown primitive option
  fields still throw `TypeError`; material validation reuses the existing
  binding path.
- **Fix the typing gaps in `gallery/src/api/efx.d.ts`** for the shapes this
  change touches: `createMeshData` is typed as a union of the batch bag and
  the single-surface shorthand (batch form no longer requires `positions`,
  and mixing both forms is rejected at compile time); `drawMesh` gets the
  new positional signature; the primitive option interfaces gain
  `material?`. `gallery/src/api/efx.type-test.ts` covers the new shapes and
  the rejected ones.
- **Docs/contract.** `docs/js-api.md` (F3 `drawMesh` entry and samples, F4
  samples, F6b sample, provisional F7/F8 samples, primitive entries) and the
  AGENTS.md current-state note are updated in the same change.

## Capabilities

### New Capabilities

_None._

### Modified Capabilities

- `3d-core`: the "Whole-mesh drawing with depth" requirement is rewritten to
  the positional signature `efx.drawMesh(mesh, opts?)` with `mesh` required
  and the bag restricted to `transform?`/`color?`; the "Procedural
  primitives" requirement gains the optional `material` in each primitive's
  option bag (bound to the primitive's single surface at creation).
- `js-api`: a new requirement pins that the gallery type document
  accurately describes every valid call shape — the `createMeshData`
  batch/short-form union (batch does not require `positions`; mixing forms is
  rejected), the positional `drawMesh(mesh, opts?)`, and the primitive
  `material` option — and rejects invalid shapes.

## Impact

- **Code:** `src/api/api.c` (`efx_js_drawMesh` reads `argv[0]` as the mesh
  and `argv[1]` as the bag), `src/runtime/runtime.c` (`drawMesh` arity hint
  1 → 2), `src/web/entry.js` (bridge-side `drawMesh(mesh, opts)` wrapper
  with identical validation), `src/prelude/prelude.js` (the three primitives
  thread `material` into `createMeshData`'s `materials`), regenerated
  `src/prelude/prelude.h` via `tools/gen_prelude.py`. `src/web/bridge.c`
  is unchanged (the bridge already takes mesh + transform/color pointers).
- **Docs:** `docs/js-api.md`, `gallery/src/api/efx.d.ts`,
  `gallery/src/api/efx.type-test.ts`, and the AGENTS.md current-state note.
  **No ADR** — this is a signature refinement of the F3 contract; ADR 0024
  already records the resource-first-arg convention and per-surface material
  binding, and both are unaffected (the same posture as the f2c `drawQuad`
  reshape).
- **Tests/samples:** `examples/browser/main.js`; the curated gallery samples
  (`hello-cube`, `light-show`, `texture-showcase`, `gltf-showcase`);
  `tests/goldens/*/main.js` (every 3D scene); `tests/scripts/
  s_3d_validation.js`, `s_3d_math.js`, `s_4a_validation.js`,
  `s_4b_validation.js`, `s_5a_validation.js`; `tests/unit/api_tests.c`. New
  coverage: the positional-signature throw cases and the primitive
  `material` binding.
- **Milestone:** F3 (a revision of the F3 mesh surface; the primitive
  `material` convenience reuses the F4 per-surface binding, not new lighting
  behavior). Predecessors F3/F4 already pass their gates; this change's own
  four-target gate must pass before archive.
- **Verification:** ctest smoke/unit suites everywhere (including the
  rewritten `drawMesh` call sites and primitive-material cases); the
  golden-image suite on the display-bearing targets must reproduce the
  committed PNGs unchanged; the existing four-target gate.

**Non-goals (out of scope):**

- No per-draw material override on `drawMesh` (`drawMesh(mesh, { material })`)
  — rejected in ADR 0024; materials stay bound to surfaces.
- No material option on `createMeshData` beyond the existing `materials`
  array, and no `material` on `loadMeshData` (imported materials are the
  glTF profile's concern).
- No change to `setMeshSurfaceMaterial`, the material object shape, or the
  lighting equation.
- No new resource type, no display-list record change, no renderer/playback
  change, no shader-visible anything (ADR 0015).
- No backward-compatible shim for the old `drawMesh({ mesh })` bag — the
  break is hard, matching the f2c `drawQuad` precedent.
- No full audit of `efx.d.ts` beyond the mesh shapes this change touches.
