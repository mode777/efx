# Proposal

## Why

F6c imports glTF skins, skeletons, and animation clips into the `Mesh`, but the
payload is deliberately opaque: a rigged asset can only be drawn in its bind
pose. F7 is the milestone that turns that imported data into visible motion —
CPU skinning plus the script-facing posing call that makes a rigged model walk.

## What Changes

- Add `efx.poseMesh(mesh, pose)`: sample one clip (`{ clip, time, weight? }`) or
  a weighted array of samples (cross-fade), then CPU-skin the `Mesh` **in
  place** into its posed vertex buffer. `time` wraps modulo clip length; weights
  are normalized engine-side, negative weights throw.
- Extend `efx.drawMesh(mesh, opts?)` with an optional `skinned` boolean: absent
  or `false` draws the retained bind-pose buffer, `true` draws the current posed
  buffer. `skinned: true` on a mesh without a rig throws `TypeError`.
- Resolve clip names/indices (`glTF name` or internal `clipN`) and channel
  targets (glTF node index) to joints using the F6c rig payload; LINEAR/STEP
  sampling stays exact (CUBICSPLINE already degraded to LINEAR at import).
- Keep the immutable bind-pose vertex buffer and add a posed buffer for skinned
  meshes (≈2× vertex memory, accepted by ADR 0017). Posing is CPU-only; the
  skinned vertex layout is not uploaded as a second GPU vertex input.
- Ship a curated gallery sample of a **CC0 rigged walking model** (Quaternius'
  `Fox` from *Ultimate Animated Animals*, whose clips include `Walk`/`Gallop`)
  that is posed per frame via `poseMesh` and drawn with `skinned: true`.
- **BREAKING** (provisional contract only): the F7 entry in `docs/js-api.md`
  and `gallery/src/api/efx.d.ts` stops being provisional and is finalized to
  exactly `poseMesh` + `drawMesh({ skinned })`. No stateful playback helper
  (`playAnimation`/`pauseAnimation`/`blendAnimations`) is added — ADRs 0017/0018
  replaced them, and a pure-JS convenience remains an F8 candidate.

## Capabilities

### New Capabilities

- `skinning-animation`: CPU linear-blend skinning of an imported rig and the
  `poseMesh` / `skinned`-draw behavior that drives it (sample selection,
  time wrap, weight normalization, posed-buffer lifetime, rig-less errors).

### Modified Capabilities

- `js-api`: adds `poseMesh` and the `skinned` draw option, converts the F7
  catalog entry from provisional to current, and records that rig data stays
  implicit `Mesh` payload (no new resource class, no query property).
- `3d-core`: extends `drawMesh`'s option bag from `{ transform?, color? }` to
  `{ transform?, color?, skinned? }` and documents the per-mesh posed buffer
  that `skinned: true` selects; `skinned` on a rig-less mesh is a `TypeError`.

## Impact

- Code: `src/render/` (skin/palette math, posed buffer, mesh record flag),
  `src/api/api.c` (quickjs `poseMesh` + `drawMesh` opts), `src/web/` (bridge
  wire path + `entry.js`), `src/resource/gltf.c` only if bind-local transforms
  must be derived at import, plus `src/math` (matrix ops already exist).
- API: `efx.poseMesh`, `efx.drawMesh({ skinned })`; `docs/js-api.md`,
  `gallery/src/api/efx.d.ts`, `gallery/src/api/efx.type-test.ts`.
- Assets/tests: `tests/goldens/` (posed-rig golden scene), unit tests against a
  CPU reference, `tests/scripts/` smoke script, a curated `gallery/samples/`
  CC0 asset pack + `CREDITS.md` row, and a `smoke_showcase_*` player test.
- Docs/ADR: new ADR `docs/decisions/0035-cpu-skinning-pipeline.md` recording the
  bind-transform derivation and matrix-palette posing pipeline (the API shape is
  already settled by ADR 0017/0018/0033). No change to `rig-import` behavior.

## Non-goals

- No stateful engine playback state or `play`/`pause`/`blend` API (ADR 0018);
  the script owns the clock. A pure-JS convenience is an F8 candidate.
- No procedural rig construction from `createMeshData` (import-only, as in F6c).
- No exact CUBICSPLINE sampling (tangents were discarded at import).
- No GPU-vertex-input skinning, morph targets, dual quaternion skinning, IK, or
  animation events.
- No new native-backed resource class and no clip/joint query properties.
- No changes to `rig-import` required behavior; F7 consumes its payload as-is.
