# 0033 — glTF rig payload: per-surface joints/weights, opaque skeleton + clips

Status: Accepted (2026-09, change `f6c-rig-import`)

Supports: ADR 0014 (skinning follows the glTF data model);
ADR 0017 (skins/skeletons are implicit Mesh payload; `skinned` is a draw
flag); ADR 0024 (multi-surface meshes); ADR 0032 (the pinned glTF 2.0
static-import profile this extends).

## Context

F6b imports static glTF geometry, materials, and textures. F6's scope also
covers glTF **skins and animation clips**, which F7 needs to CPU-pose. The
glTF data model (ADR 0014) puts skin weights on primitives and a skin
(joint list + inverse bind matrices) on a node, while the engine's
consumer pipeline is fixed-function with a fixed interleaved GPU vertex
layout (pos/normal/uv/color) and no script-visible shader or pose state
until F7. Without a settled rig model, F7 would have to invent both the
import mapping and the posing API at once.

## Decision

- **Joints/weights are CPU-only per-surface attributes.** `efx_surface`
  gains `joints` (four `uint32` joint indices per vertex) and `weights`
  (four `float` weights per vertex), imported from glTF `JOINTS_0` /
  `WEIGHTS_0` (u8/u16 component types normalized to `uint32`) and accepted
  by `createMeshData`. They are an all-or-nothing pair with exactly the
  vertex count of `positions`; a mismatch or unpaired attribute fails
  (`RangeError` at the binding, `EFX_MESHERR_LEN` in the core). They are
  **not** interleaved into the GPU vertex buffer in F6c.
- **The skeleton and clips are opaque rig payload.** The `MeshData` carries
  an optional `efx_rig` — the skin's joint node indices, parent
  relationships (nearest ancestor that is a skin joint, `-1` at a root),
  inverse bind matrices (column-major, identity-filled when the accessor is
  absent), and every `animations[]` clip. `createMesh` deep-copies the rig
  onto the `Mesh`; destroying the `MeshData` leaves the `Mesh` rig intact.
- **Node→skin binding.** The importer scans nodes that reference the
  selected mesh and uses the first with a `skin`; without one the mesh has
  no skeleton. This needs no API surface for the one-mesh/many-skins case
  the profile does not cover.
- **Interpolation.** LINEAR and STEP samplers import exactly. A
  CUBICSPLINE sampler's output stores in-tangent, value, out-tangent per
  keyframe; the importer keeps the middle value per keyframe and marks the
  channel LINEAR, discarding tangents, and never fails a valid CUBICSPLINE
  clip. Morph-target (`weights`) channels are skipped.
- **Clips are named internally** (the glTF `name`, or a stable `clipN`
  when unnamed) for F7's `poseMesh({ clip, time })` by name or index.
- **No new script surface.** The only new script-visible data is the
  joints/weights surface attributes; there is no clip/joint query function
  or property, no separate Skeleton/Animation resource, and the existing
  `MeshData`/`Mesh` `destroy()` lifecycle releases the payload.

## Consequences

- F7 adds only posing math and the `skinned` draw flag: it consumes the
  CPU joints/weights and the rig payload already carried by the Mesh.
- The GPU interleave and all F6b goldens are unaffected; skinned
  attributes cost CPU memory, not vertex bandwidth, until F7 poses them.
- Imported rigs follow the existing native-backed resource lifetime, so
  the ADR 0012/0013 destroy discipline extends unchanged.
- A future exact-CUBICSPLINE mode or procedural rig construction is a
  separate change; neither is reachable now.

## Rejected alternatives

- **A separate Skeleton/Animation resource type**: removed by ADR 0017;
  scripts get one Mesh and (in F7) `poseMesh`, so a second resource would
  re-expose state the engine owns.
- **GPU vertex-input skinning now**: the consumer pipeline is
  fixed-function and CPU posing is the chosen F7 strategy (ADR 0014); a
  skinned vertex format would change the pinned GPU layout for no F6c
  benefit.
- **Store the payload only on the Mesh**: the CPU `MeshData` is the natural
  import result and the test seam for rig parsing; carrying it is trivial.
- **A node selector in `loadMeshData`**: extra API for a case (one mesh,
  multiple differently-skinned nodes) that PS2-era assets and F6c
  verification do not need.
- **Preserving CUBICSPLINE tangents**: pulls a cubic sampler into F6c/F7
  for a rare mode; linear approximation is adequate at PS2-era fidelity.
- **Rejecting CUBICSPLINE assets**: hostile to common exporters that
  default to cubic sampling.
- **Read-only clip-name / joint-count queries**: keeps the rig opaque by
  decision (ADR 0017); tests verify via a rest-pose golden and native unit
  tests, and authors know their asset's clip names.
