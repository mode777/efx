# Proposal

## Why

F6b imports static glTF geometry, materials, and textures. F6's scope also
covers glTF **skins and animation clips** (roadmap), and ADR 0014/0017 already
pin the data model: joints/weights are surface attributes and the skeleton and
clips are implicit Mesh payload. F6c delivers that import so F7 can focus on
CPU posing and playback instead of also inventing the rig data model.

## What Changes

- **Skin attributes import.** `loadMeshData` (F6b) additionally imports
  `JOINTS_0`/`WEIGHTS_0` per primitive into the corresponding surface's
  joints/weights attributes (four influences, glTF-style).
- **Public skinned-mesh attributes.** `createMeshData` accepts optional
  `joints` and `weights` per surface, so the data model is complete and
  script-constructed skinned geometry is expressible (no procedural rigs).
- **Skeleton and clip payload.** The importer parses the glTF `skin`
  (joint list, inverse bind matrices) and each `animations[]` entry (channels
  and samplers) and bundles them into the returned `MeshData`; `createMesh`
  carries the payload into the `Mesh`. The payload is opaque — no new script
  functions and no new resource type (ADR 0017).
- **Interpolation.** LINEAR and STEP samplers import exactly; CUBICSPLINE
  samplers are approximated as LINEAR (tangents dropped).
- **Docs/contract.** `docs/js-api.md` and `gallery/src/api/efx.d.ts` gain the
  skinned-surface attributes; an ADR records the rig payload model.

## Capabilities

### New Capabilities

- `rig-import`: glTF skin, skeleton, and animation-clip import — surface
  joints/weights, the implicit skeleton/animation payload carried from
  `MeshData` to `Mesh`, and the interpolation policy. No script-facing
  playback (that is F7).

### Modified Capabilities

- `js-api`: `createMeshData` surfaces accept optional `joints`/`weights`;
  the implicit rig payload is documented as MeshData/Mesh internals.
- `3d-core`: the multi-surface mesh data requirement gains the joints/weights
  surface attributes.

## Impact

- **New code:** skin/animation parsing and payload storage in
  `src/resource/`; skinned-attribute plumbing through `MeshData` → `Mesh`.
- **Modified code:** `src/api/api.c` and `src/web/bridge.c` (`createMeshData`
  joints/weights), `src/render` (MeshData surface arrays + Mesh payload).
- **Docs:** `docs/js-api.md`, `gallery/src/api/efx.d.ts`, rig-payload ADR.
- **Milestone:** F6 (F6c slice). Predecessor F6b must pass its gate first.
- **Verification:** unit tests for joints/weights/skeleton/clip parsing and
  interpolation fallback; a rest-pose golden of a skinned asset; opaqueness
  asserted (no new script properties).

**Non-goals (out of scope for F6c):**

- CPU skinning, `poseMesh`, and the `skinned` draw flag (F7).
- Procedural skeleton/animation construction from script.
- Exact CUBICSPLINE (tangent) sampling.
- Any read-only clip name / joint count query — the rig stays opaque.
- REPL mode (F6d).
