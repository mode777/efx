# Design

## Context

See `proposal.md` for motivation. Constraints:

- **glTF data model (ADR 0014):** weights are primitive attributes, a `skin`
  is joints + inverse bind matrices, node-mesh-skin links are on nodes.
- **Implicit rig payload (ADR 0017/0018):** skeletons and clips live inside the
  mesh; playback is F7's stateless `poseMesh` (script owns the clock).
- **MeshData/Mesh split (ADR 0013):** CPU attributes → uploaded Mesh; the
  interleaved GPU layout is fixed at pos/normal/uv/color, so skinned attributes
  are CPU-side engine state for F7's CPU posing, not GPU vertex inputs.
- **F6b import** provides cgltf parsing, the provider read callback, and the
  material/texture pipeline this change extends.

## Goals / Non-Goals

**Goals:**

- Complete the glTF data mapping so F7 adds only posing math and the draw flag.
- Extend the public mesh-data model just enough for skinned attributes while
  keeping rigs and clips opaque.

**Non-Goals:**

- Skinning math, posed buffers, `poseMesh`, `skinned` draw flag (F7).
- Procedural skeleton/clip construction; clip/joint queries.
- Exact CUBICSPLINE sampling (tangents dropped).

## Decisions

### D1 — Joints/weights are surface attributes in MeshData

`efx_surface_src`/`efx_surface` gain parallel `joints` (4×u16 or normalized to
u32) and `weights` (4×f32) arrays; `createMeshData` validates the pair and the
vertex count. They are CPU-only and not interleaved into the GPU vertex buffer
in F6c (F7 consumes them to produce posed positions). Imported glTF
`JOINTS_0`/`WEIGHTS_0` fill them directly.

- **Rejected — a separate skin resource (ADR 0013):** superseded by 0014; the
  glTF mapping is attribute-based.
- **Rejected — GPU vertex-input skinning now:** the consumer pipeline is
  fixed-function and CPU posing is the chosen F7 strategy (ADR 0014).

### D2 — Opaque rig payload carried MeshData → Mesh

The `MeshData` holds an optional rig payload: skeleton (joint node indices,
parent relationships, inverse bind matrices) and clips (name, channels, and
keyframe samplers). `createMesh` deep-copies it onto the `Mesh`; destroying
the `MeshData` after upload does not disturb the `Mesh` (same copy semantics as
geometry). No new JS shape is exposed.

- **Rejected — exposing Skeleton/Animation classes:** ADR 0017 removed them;
  scripts get one Mesh and (in F7) `poseMesh`.
- **Rejected — keeping the payload only in the Mesh:** the CPU MeshData is the
  natural import result and the test seam for rig parsing; carrying it is
  trivial.

### D3 — Skin linking by the selected mesh's node

glTF binds skin to a node, not a mesh. The importer scans nodes that reference
the selected mesh and uses the first node with a `skin`; if none, the mesh is
static. Joint indices resolve through `skin.joints`; inverse bind matrices come
from `skin.inverseBindMatrices` (identity-filled when absent, per glTF).

- **Rejected — requiring a node selector in the API:** extra surface for a
  case (one mesh, multiple differently-skinned nodes) that PS2-era assets and
  F6c verification do not need.

### D4 — Interpolation: LINEAR/STEP exact, CUBICSPLINE as LINEAR

LINEAR and STEP samplers import their times and values unchanged. A
CUBICSPLINE sampler's output layout is three values per keyframe (in-tangent,
value, out-tangent); the importer keeps the middle value per keyframe and marks
the channel LINEAR, discarding tangents. The import never fails for a valid
CUBICSPLINE clip.

- **Rejected — preserving tangents:** pulls a cubic-sampling implementation
  into F6c/F7 for a rare mode; linear approximation is visually adequate at
  PS2-era fidelity.
- **Rejected — rejecting CUBICSPLINE assets:** hostile to common exporters.

### D5 — Clips are named internally; resolution is F7's

A glTF animation's `name` becomes the clip's internal name; unnamed clips get a
stable index-based internal name. F7's `poseMesh({ clip, time })` resolves by
name or index (first match on duplicates). Names are not script-enumerable
(D6); authors know their asset's clip names.

- **Rejected — read-only clip-name queries:** the rig stays opaque by decision
  (user-cleared); tests verify via a rest-pose golden and internal unit tests.

### D6 — Opaqueness and lifetime

No new script functions or read-only properties; the only new script-visible
data is the joints/weights surface attributes. The payload is released with the
`MeshData`/`Mesh` that owns it under the existing `destroy()` lifecycle.

## Risks / Trade-offs

- **[Joint index component types]** → glTF allows u8/u16 joints; normalize to a
  stable engine representation at import and unit-test each type.
- **[CUBICSPLINE value extraction]** → off-by-one in the ×3 output stride is
  easy to get wrong; unit-test a known cubic keyframe sequence.
- **[Weight normalization]** → glTF weights are guaranteed to sum to 1; do not
  renormalize at import (F7's contract renormalizes blends), but validate
  finite values.
- **[Skin binding ambiguity]** → first-skin rule documented; a golden with one
  skinned node proves the mapping.
- **[Memory: bind pose + rig retained for F7]** → accepted (ADR 0017); rigs are
  small relative to geometry.
- **[No GPU path for skinned attributes yet]** → they are intentionally CPU
  payload; the GPU interleave is unchanged, so all F6b goldens are unaffected.

## Migration Plan

1. Extend the mesh-data model and `createMeshData` with joints/weights;
   unit-test validation in isolation.
2. Implement skin/attribute parsing and the skeleton payload; unit-test the
   node→skin resolution and inverse bind matrices.
3. Implement clip parsing with LINEAR/STEP and CUBICSPLINE→LINEAR; unit-test a
   known clip's times/values.
4. Carry the payload through `createMesh`; assert a rest-pose golden renders a
   skinned asset identically to its bind pose.
5. Update docs, `efx.d.ts`, and the rig-payload ADR; run the Linux → Windows →
   macOS gate order.

Rollback: additive to F6b; reverting removes the attributes and payload and
leaves F6b behavior intact.

## Open Questions

None that affect the specs, approach, or tasks. Exact CUBICSPLINE sampling and
procedural rigs are recorded non-goals for a future change.
