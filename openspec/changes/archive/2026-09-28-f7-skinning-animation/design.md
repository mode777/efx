# Design

## Context

F6c leaves the engine with an imported rig that is deliberately data-only:
`efx_rig` carries `joint_nodes` (glTF node index per joint), `joint_parents`
(joint-index space, `-1` at a root), `inverse_bind` matrices (column-major), and
named clips whose channels target **glTF node indices** and carry inlined
`times`/`values` with LINEAR or STEP interpolation (CUBICSPLINE was already
degraded to LINEAR). Per-surface `joints` (four `uint32`) and `weights` (four
`float`) are stored CPU-only on the `Mesh` but never uploaded. See
`docs/decisions/0033-gltf-rig-payload.md` and
`openspec/specs/rig-import/spec.md`.

Constraints that shape the approach:

- The API shape is already pinned: `efx.poseMesh(mesh, pose)` and
  `efx.drawMesh(mesh, { skinned })`, no engine playback state
  (ADR 0017, ADR 0018). The F6c payload was designed with this exact consumer
  in mind (ADR 0033).
- The bind-pose vertex buffer is immutable and must stay drawable; skinning
  writes a second, posed vertex buffer (ADR 0017, ~2× vertex memory accepted).
- Consumer API stays fixed-function; skinning is CPU (ADR 0017), so no new
  shader/vertex-input path is introduced.
- `createMeshData` can build joints/weights, but only `loadMeshData` produces a
  rig; script-built meshes always have `rig == NULL` and must keep working.

## Goals / Non-Goals

**Goals:**

- CPU linear-blend skinning of imported rigs, driven by one stateless call.
- Deterministic, unit-testable posing (sample in → posed vertices out) that a
  CPU reference in the test suite can reproduce exactly enough for the gate.
- Deliver the pinned API, turn the F7 reference/type entries current, and prove
  it end-to-end with a CC0 rigged walking gallery sample.

**Non-Goals:**

- Any engine-side clock/playback state, resource class, or query property.
- GPU/vertex-shader skinning, dual-quaternion skinning, IK, morph targets, or
  root-motion extraction.
- Procedurally authored rigs (import-only, per F6c).
- Changing `rig-import`'s required behavior or the imported payload format.

## Decisions

### D1 — Derive joint bind-local transforms from the inverse bind matrices

The rig stores only `inverse_bind` plus `joint_parents`, not each joint's
bind-local TRS. Reconstruct them: `world_bind[j] = inverse(inverse_bind[j])`,
then `local_bind[j] = inverse(world_bind[parent(j)]) * world_bind[j]`, and
`local_bind[root] = world_bind[root]`. This is exact (a glTF inverse bind matrix
is by definition the inverse of the joint's world bind transform) and needs no
import change.

- Alternatives: extend the importer to store each joint's local bind TRS or the
  full node hierarchy TRS → rejected; F6c's payload is deliberately minimal and
  the reconstruction is lossless. A singular/uninvertible matrix falls back to
  identity for that joint.

### D2 — FK in joint space, with animation channels resolved by node index

Build a `node → joint index` map from `joint_nodes` once per mesh. On each
`poseMesh` call, sample the selected clips to produce local-TRS overrides keyed
by joint index, then walk joints in hierarchy order:
`local = override ?? local_bind`, `world = world[parent] * local`, and the skin
palette `M[j] = world[j] * inverse_bind[j]`.

A channel whose `target_node` is not one of the skin joints is **ignored** at
pose time (no propagation through non-joint ancestors). This keeps the F6c
payload unchanged and covers skeleton-style assets (including the Quaternius
Fox).
Models that animate a non-joint ancestor above the skeleton (root motion) will
not move that ancestor — recorded as a risk and a future-change candidate.

### D3 — Per-vertex weights normalized at pose time; zero-weight vertices stay at bind

Import intentionally does not renormalize weights (ADR 0033). Skinning therefore
normalizes the four influence weights of each vertex at pose time: divide by
their sum when it is non-zero, otherwise leave the vertex at its bind position
(and bind normal). This makes malformed-but-imported weights well-defined
without mutating imported data.

### D4 — Dual vertex buffers; the posed buffer is referenced live by the record

Each skinned surface keeps its immutable interleaved bind-pose GPU buffer and
gains a second GPU buffer holding the posed positions/normals (uv/color never
change). `poseMesh` writes the CPU posed arrays; the skinned draw path updates
the posed GPU buffer from them (Sokol `sg_update_buffer`) and draws it. Static
meshes allocate no second buffer and take a byte-identical path.

- Alternatives: mutate the bind buffer in place → rejected (violates the ADR
  0017 dual-buffer/retained-bind-pose contract and breaks `drawMesh(mesh)`).
  GPU skinning in a vertex shader → rejected (ADR 0017 chose CPU; would add a
  second vertex layout and palette uniforms for no v1 benefit).
- Consequence: `skinned` is snapshotted per record, but the **posed buffer is
  read live at playback** (like materials), so the last `poseMesh` before
  playback wins. Snapshotting posed vertices per record would cost a vertex
  copy per draw; rejected. This matches the existing display-list model where
  transforms/colors are snapshotted but referenced resources are live.

### D5 — Clip lookup and error taxonomy

`clip` accepts the internal clip name (glTF `name`, else `clipN`) or an index.
Recorded rules: non-Mesh/destroyed/rig-less mesh and unknown option fields throw
`TypeError`; an out-of-range clip index and a negative sample weight throw
`RangeError`; an unknown clip name throws a standard `Error` (consistent with
the import loaders); a clip with no channels poses to the bind pose rather than
failing.

### D6 — Bindings on both runtimes

Desktop: `efx_js_poseMesh` in `src/api/api.c` plus `"skinned"` added to
`efx_js_drawMesh`'s known-opts and to the mesh display-list record. Web: expose
`_efx_bridge_pose_mesh` and thread the `skinned` flag through `entry.js`
`drawMesh`; the rig lives in the native `Mesh` from `loadMeshData`, so no rig
marshalling is needed. `createMeshData` stays rig-less on both runtimes.

### D7 — Verification is reference-first

`poseMesh` is deterministic by construction, so the primary test is a unit test
that runs an independent CPU reference (its own FK + LBS over the fixture's rig
and clips) and compares every posed vertex within epsilon, including time wrap,
STEP hold, and weighted blend. Golden images cover the visual path: a committed
posed-rig scene (a deterministic pose of a fixture) plus the gallery Fox sample.
The reference lives in the test, never in engine code.

### D8 — Gallery sample: Quaternius Fox (CC0)

The Khronos `Fox` sample is not fully CC0 (the model is CC0 but its rigging,
animation, and glTF conversion are CC-BY 4.0), which the CC0-only gallery
convention excludes. The sample therefore uses **Quaternius' `Fox`** from
*Ultimate Animated Animals* — a single-mesh, single-skin, textureless glb that
is fully **CC0 1.0** and ships `Walk`/`Gallop` and other clips. Following the
Avocado precedent, the pack is optimized to the importer's supported feature
set (base-color factors only; `metallicFactor` 0, `roughnessFactor` 0.6 for a
matte dielectric Phong material), zipped deterministically at the archive root,
documented with a `CREDITS.md` row and recipe, and exercised by a Linux
`smoke_showcase_*` player test. No Cesium Man (CC-BY would break the CC0-only
gallery convention).

### D9 — New ADR

`docs/decisions/0035-cpu-skinning-pipeline.md` records the durable pipeline:
bind-local derivation from inverse bind matrices, joint-space FK, palette
composition, per-vertex weight normalization, the non-joint-ancestor limitation,
and the dual-buffer/live-posed-buffer semantics. ADR 0017/0018/0033 keep the API
shape and payload, so this ADR records only the posing mechanics.

## Risks / Trade-offs

- [Channels on non-joint ancestors are ignored → root motion missing] →
  documented limitation; the Quaternius Fox and joint-space clips are
  unaffected; future change
  can import the node hierarchy.
- [Per-vertex weight normalization silently "fixes" bad exported weights] →
  intentional and bounded: only the four influences, finite-checked at import;
  a zero-sum vertex is left at bind rather than moved to the origin.
- [Matrix inversion of `inverse_bind` can be ill-conditioned] → near-identity
  bounds are handled by an identity fallback; fixtures assert exact bind pose
  round-trips.
- [Posed GPU buffer adds memory, and `sg_update_buffer` per frame costs
  bandwidth] → only skinned surfaces allocate it; update is skipped until the
  pose changes; static meshes are byte-identical to F6.
- [Cross-target float differences in LBS] → golden tolerance already allows
  2/255 per channel over 99.5% of pixels; the reference test uses a loose
  absolute epsilon and the goldens pin the visual result.
- [Finalizing the provisional F7 API may surprise adopters] → the pinned shape
  already matches ADR 0017/0018 and the provisional reference; no stateful
  helper is added, exactly as documented.

## Migration Plan

Additive feature; no data migration. The only compatibility change is the
provisional F7 catalog entry becoming current, which narrows the documented
surface to the already-published shape. Roll back by reverting the branch;
`rig-import` payloads and all pre-F7 goldens are untouched.

## Open Questions

None blocking: the clip-naming/lookup rules are inherited from ADR 0033, and any
future root-motion support is a separate change that does not alter this spec.
