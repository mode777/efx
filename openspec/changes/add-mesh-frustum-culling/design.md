# Design

## Context

Both `drawMesh` bindings — the desktop quickjs API (`src/api/api_3d.c`) and
the Emscripten bridge (`src/web/bridge_render3d.c`) — funnel into one C
entry point, `efx_render_mesh` (`src/render/render_records.c`), which already
holds everything the test needs at record time: the mesh handle (→ its slot),
the model matrix, and the engine state (`R.camera3d`, `R.viewport_w/h`, the
active render target). Meshes today carry no bounds; mesh ingest is
dual-implemented (desktop: `efx_meshdata_create` in `src/resource` + the
cgltf path in `src/resource/gltf.c`; web: `createMeshData` in
`src/web/js/audio.js`). The camera is a projection state whose aspect derives
from the active rendering surface (3d-core camera requirement), and the
camera is value-snapshotted into the record at record time — the cull test
evaluates exactly the camera the record will carry.

## Goals / Non-Goals

**Goals:**

- One implementation of the cull test, shared by both runtimes (put it where
  the funnels merge).
- Fail-open doctrine: culling may only skip draws provably fully outside.
- Zero visible change for currently-visible scenes (goldens must stay
  byte-identical).
- Bounds as plain value data on the existing resource classes — no new
  resource kind, no new lifetime rules.

**Non-Goals:**

- Billboards/particles, occlusion, hierarchy, LOD, per-clip posed bounds —
  see proposal Non-goals.
- A script-facing frustum object or camera getter (rejected — see
  Decisions).

## Decisions

### D1: The test lives inside `efx_render_mesh`, not in the API layers

The signature grows a `cull` flag (`efx_render_mesh(handle, transform,
color, skinned, depth_write, cull)`); both callers pass their bag field
through. The API layers only parse/validate the option.

*Alternative — script-side frustum object + JS tests:* lost because the
camera is write-only to scripts (a getter would be a new API just to
re-implement the engine's math), it pushes per-draw policy into every game,
and it cannot be enforced uniformly. *Alternative — playback-time culling in
`play_records`:* lost because record time is where camera, matrix, and bounds
are co-located, playback stays dumb and re-orderable, and a culled record
should not be budgeted in the first place.

### D2: World-space box test — 8 corners → world AABB → 6-plane p-vertex

Transform the local box's 8 corners by the model matrix (with w-divide;
mirrors and negative scale fall out naturally), take the world AABB, extract
the frustum's 6 planes from `R.camera3d` + active-surface aspect (view basis
built the same way `screenRay` builds it — one shared helper in
`src/render`), then per-plane p-vertex test with early-out. ~130 multiplies
worst case, early-outs on the first outside plane. Conservative by
construction: only a box fully behind one plane culls.

*Alternative — bounding sphere (center through matrix, radius × max column
scale):* cheaper and rotation-invariant but strictly looser for elongated
geometry (walls, ground planes); can be added later as a pre-test without
spec change (it only tightens what "fully outside" may reject... it must
remain conservative — sphere-in-box or box test both satisfy the spec).
*Alternative — testing in clip/normalized device space:* loses the
world-space interpretability and gains nothing at this scale.

### D3: Bounds computed at ingest, stored as `min`/`max` on the slot

`efx_meshdata_create` scans surface positions (min/max); the glTF path
computes identically before `efx_meshdata_create`; the web `createMeshData`
fragment computes identically in JS before marshaling. The override
(`createMeshData` third positional `opts.bounds`) stores the given box
verbatim. `mesh_slot` gains 6 floats; `Mesh.bounds` derives
`center`/`radius` on read. This is the second dual-implementation detail on
mesh data (after the material wire block) — both sites get cross-linking
comments like the material block's.

*Alternative — computing bounds at `createMesh` (upload) only:* lost because
the override must survive MeshData→Mesh copies and MeshData should describe
its geometry (the getter is on Mesh, but the data belongs to the mesh
payload, like materials).

### D4: Skinned draws bypass the test entirely

The test runs only when the record's `skinned` flag is 0. Bind-pose bounds
do not bound posed geometry; per-clip conservative bounds are deferred. The
`bounds` override exists for script authors who post-process MeshData and
(when per-clip bounds arrive) as the skinned escape hatch.

### D5: Fail-open rules as engine invariants

Cull only on a provably-outside box. Degenerate camera (eye ≈ target within
epsilon — same degeneracy `screenRay` rejects), inverted override box
(`min > max` on any axis), or a non-finite intermediate anywhere in the test
→ record the draw. The test never throws; `drawMesh` error behavior is
unchanged. JS-reachable non-finite inputs are already rejected by option
validation; this clause guards the engine's decision.

### D6: Culling happens per record segment, not per frame

`frustum` state is not cached across `beginRenderTarget` segments — the test
reads live `R` state (camera + active target extent for aspect) each
`drawMesh`, matching how the record snapshots the camera. Cost is a
handful of flops; caching would introduce stale-state bugs for no measurable
win.

### Resource exposure (per config rule)

No new unmanaged resources: bounds are value data inside the existing
GC-finalized opaque classes (`MeshData`, `Mesh` — ADR 0011, discipline ADR
0012) with existing `destroy()` semantics; the fixed-bank pattern (lights)
does not apply. `drawMesh`/`createMeshData` option validation stays native
in both bindings (hot path per ADR 0049 — `api_3d.c` known-field lists +
web `render3d.js` `__efxCheckKnown`); the `createMeshData` bag follows that
call's existing validation placement.

## Risks / Trade-offs

- [Dual bounds implementations drift (C vs web JS)] → cross-linking comments
  at both sites; the cross-runtime comparison harness
  (`tools/run_web_compare.mjs`) and the identical-culling scenario pin
  agreement; a shared golden scene with an off-screen mesh would catch
  drift in CI.
- [Auto-cull silently hides meshes when bounds are wrong] → the `bounds`
  override is the pressure valve; `Mesh.bounds` getter is the debugging
  surface; `cull: false` is the per-draw opt-out; the spec's fail-open
  rules bound the blast radius to "provably outside".
- [AABB under rotation is looser than the visual hull] → accepted
  conservatism (spec: straddling boxes always draw); the sphere pre-test
  (D2 alternative) can tighten later without spec change.
- [Budget tests that count off-screen draws] → the spec makes budget
  exemption normative (culled draws consume none); existing tests that
  draw off-screen meshes while asserting budget exhaustion need updating in
  this change, not worked around.

## Migration Plan

Purely additive surface: new option fields default to the new behavior
(`cull` defaults `true`) — the one behavior change (off-screen draws stop
recording) is the feature. Rollback = flip `cull` default or pass
`cull: false`; no data migration. Existing scenes and goldens are
unaffected (visible draws unchanged).

## Open Questions

None — the flag name, skinned rule, and bounds getter were resolved during
exploration.
