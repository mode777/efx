# 0035 — CPU skinning pipeline: bind-local derivation, joint-space FK, LBS

Status: Accepted (2026-09, change `f7-skinning-animation`)

Supports: ADR 0017 (CPU skinning into a second vertex buffer; `skinned` is a
draw flag); ADR 0018 (script-driven posing via `poseMesh`, no engine
playback state); ADR 0033 (the F6c rig payload this consumes).

## Context

F6c carries an imported rig as opaque `Mesh` payload: joint node indices,
joint parents, inverse bind matrices, and named clips whose channels target
glTF node indices with inlined LINEAR/STEP keyframes. The payload
deliberately omits each joint's bind-local TRS and the full node hierarchy.
F7 must turn that data into posed vertices without changing the payload, the
GPU vertex layout, or the fixed-function pipeline.

## Decision

- **Bind-locals are reconstructed, not imported.** For each joint
  `world_bind = inverse(inverse_bind)` and
  `local_bind = inverse(world_bind[parent]) * world_bind` (root:
  `world_bind`). A singular matrix falls back to identity for that joint.
  A glTF inverse bind matrix is by definition the inverse of the joint's
  world bind transform, so the reconstruction is lossless.
- **FK runs in joint space.** A `node → joint` map is built once from
  `joint_nodes`. Each sample resolves channels by target node; a channel
  targeting a node that is not a skin joint is ignored (no propagation
  through non-joint ancestors). Joints are walked in hierarchy order:
  `local = override ?? bind local`, `world = world[parent] * local`, and
  the palette is `M[j] = world[j] * inverse_bind[j]`.
- **Sampling is exact for LINEAR/STEP.** Translation/scale LINEAR lerps,
  rotation LINEAR slerps, STEP holds the previous keyframe; `time` wraps
  modulo the clip length (the max keyframe time over its channels).
  CUBICSPLINE was already degraded to LINEAR at import (ADR 0033).
- **Weights normalize at pose time.** Import does not renormalize (ADR
  0033); LBS divides each vertex's four influence weights by their sum.
  A zero-sum vertex keeps its bind position/normal rather than moving to
  the origin, and posed normals are renormalized.
- **Dual vertex buffers.** A skinned surface keeps its immutable bind-pose
  GPU buffer and gains a second dynamic buffer holding posed
  positions/normals (uv/color are copied once and never change). `poseMesh`
  writes only the CPU posed array; the skinned draw path uploads it once per
  pose revision and draws it. Static meshes allocate no second buffer and
  take the byte-identical pre-F7 path. Posing never mutates the bind data,
  so `drawMesh(mesh)` still renders the rest pose.
- **One stateless call, no playback state.** `poseMesh(mesh, pose)` accepts
  a single `{ clip, time, weight? }` sample or an array (a weighted blend,
  weights normalized engine-side, negative weights rejected); the script
  owns the clock. The rig stays implicit `Mesh` payload — no new resource
  class, no clip/joint query property, no `play`/`pause`/`blend`.

## Consequences

- F7 adds only posing math and the `skinned` draw flag; it consumes the F6c
  payload unchanged, so `rig-import` behavior and every pre-F7 golden are
  untouched.
- Skinned meshes cost ~2× vertex memory (bind + posed) and one
  `sg_update_buffer` per pose change; a static mesh is unaffected.
- Root motion animated on a non-joint ancestor above the skeleton does not
  move that ancestor. This is a documented limitation; supporting it would
  require importing the node hierarchy and is a future change.
- The GPU/vertex-shader skinning, dual-quaternion, IK, and morph-target
  alternatives remain rejected (ADR 0017 chose CPU for v1).

## Rejected alternatives

- Extending the importer to store bind-local TRS or the full node hierarchy
  — rejected; F6c's payload is deliberately minimal and the reconstruction
  is lossless.
- Mutating the bind buffer in place — rejected; it violates the ADR 0017
  retained-bind-pose contract and breaks `drawMesh(mesh)`.
- GPU skinning in a vertex shader — rejected; it would add a second vertex
  layout and palette uniforms for no v1 benefit.
- Snapshotting posed vertices per display-list record — rejected; a vertex
  copy per draw is costly and the display list already references live
  resources (materials, textures).
