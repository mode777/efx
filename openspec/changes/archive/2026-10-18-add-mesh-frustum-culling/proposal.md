# Proposal

## Why

Mesh draws that lie entirely outside the camera's view are still recorded,
budgeted, and played back — script authors who want culling today have no
frustum API at all (the camera is write-only to scripts, and meshes carry no
bounds). Auto frustum culling at record time removes that entire class of
wasted work invisibly: the engine already holds the camera, viewport aspect,
and model matrix at the `drawMesh` call site, so the test costs a handful of
multiplies per draw and saves whole draws in any scene larger than one room.

## What Changes

- Meshes gain bounds: `createMeshData` computes the axis-aligned bounding box
  of all surface vertex positions at ingest (mirrored on the glTF import path
  and the web bridge); `createMeshData` accepts a trailing `opts` bag with a
  `bounds` override; the Mesh exposes a `bounds` query property
  (`{ min, max, center, radius }`) for debugging.
- `drawMesh` gains a `cull` option (boolean, default `true`): when enabled and
  the draw is not skinned, the engine frustum-culls the draw at record time —
  the mesh's local bounds are transformed by the draw's model matrix to a
  world-space box and tested against the currently active 3D camera's frustum;
  a fully-outside box records nothing. `cull: false` disables the test for
  that draw.
- Culled draws consume no display-list budget and do not error; culling is
  fail-open (degenerate camera, empty/non-finite bounds, or a straddling box
  always draws). Skinned draws are never culled: bind-pose bounds do not
  describe posed geometry.
- New ADR `docs/decisions/0065` records the record-time, engine-side,
  fail-open culling doctrine.

## Capabilities

### New Capabilities

<!-- none: culling is a behavior of the existing 3D core; it does not
     introduce a new capability area. -->

### Modified Capabilities

- `3d-core`: the `Multi-surface mesh data` requirement gains the trailing
  `createMeshData` options bag with the `bounds` override; the `Mesh upload
  and lifecycle` requirement gains the `Mesh.bounds` query property; the
  `Whole-mesh drawing with depth` requirement gains the `cull` option; a new
  requirement pins the mesh-bounds/frustum-culling contract (ingest
  computation, world-box test, skinned bypass, fail-open rules, budget
  semantics).

## Impact

- `src/resource` / `src/render`: bounds computation at MeshData creation
  (`efx_meshdata_create` and the glTF ingest path), bounds storage on the mesh
  slot, and the cull test inside `efx_render_mesh` — the single site both the
  desktop API (`src/api/api_3d.c`) and the web bridge
  (`src/web/bridge_render3d.c`) already funnel through, so the test is
  implemented once and cannot drift between runtimes.
- `src/web/js`: the web `createMeshData` fragment computes bounds identically;
  both option-bag validators grow one field.
- Script-facing surface: `gallery/src/api/efx.d.ts` + regenerated `docs/api/`;
  `docs/js-api.md` unchanged (no layering or design-rule change).
- Golden images are unaffected: culling only skips draws that are provably
  fully outside the view, so every currently-visible draw renders identically.
- Docs: new ADR `docs/decisions/0065` + `docs/decisions/README.md` index row.

## Non-goals

- No culling for billboards or particles (a later change can reuse the same
  frustum; billboards already carry a world position and size).
- No occlusion culling, portals, BVH, or spatial hierarchy — frustum-only.
- No per-clip posed bounds for skinned meshes (bind pose + never-cull is the
  contract; per-clip conservative bounds are a future extension).
- No script-facing frustum object or camera getter — the test is engine-side.
- No level-of-detail, sorting, or draw-call batching changes.
