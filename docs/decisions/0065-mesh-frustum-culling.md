# 0065 — Mesh draws are frustum-culled at record time, engine-side, fail-open

Status: Accepted

## Context

Mesh draws outside the camera's view were recorded, budgeted, and played
back like any other draw, and scripts had no way to prevent it: the camera
is write-only to scripts, and meshes carried no bounds. The engine already
holds everything the test needs at the `drawMesh` call site — the mesh
handle, the model matrix, and the camera/aspect state that the record
snapshots anyway (ADR 0019) — so the natural place to decide is record time,
inside the shared `efx_render_mesh` that both the desktop and web bindings
funnel through. Proposed in `openspec/changes/add-mesh-frustum-culling/`.

## Decision

Every MeshData carries axis-aligned bounds — computed from all surface
positions at ingest (`efx_meshdata_create`, the glTF path, and the web
bridge's hand-assembled commit path), replaceable verbatim via the
`createMeshData` `{ bounds }` override, exposed read-only as `Mesh.bounds`.

`drawMesh` frustum-culls by default (`cull: true`): when the draw is not
skinned, the engine transforms the local bounds box by the model matrix to a
world-space box and tests it against the six frustum planes of the currently
active 3D camera (basis construction shared with `screenRay`; aspect from
`efx_render_surface_size`). A box fully outside any plane records nothing —
no record, no record-budget consumption, no error. Three invariants are part
of the contract:

- **Conservative**: only a box provably fully beyond one plane culls; a
  straddling box always draws.
- **Fail-open**: a degenerate camera (eye ≈ target), an inverted override
  box, or any non-finite intermediate keeps the draw. Culling never throws.
- **Skinned bypass**: draws using posed buffers are never culled —
  bind-pose bounds do not describe posed geometry. (Per-clip conservative
  bounds are the future escape from this conservatism, with the `bounds`
  override as the manual one.)

## Consequences

- The test is implemented once, in `src/render/render_records.c`, and
  web/desktop cannot drift; the bounds scan is the one intentionally
  dual-implemented piece (C ingest vs the web fragment), cross-linked by
  comments like the material wire block.
- Off-screen crowds stop consuming the display-list budget; scripts get
  culling with zero boilerplate, and `cull: false` / the `bounds` override /
  the `Mesh.bounds` getter are the escape hatches for the invisible-mesh
  failure mode.
- Golden output for visible scenes is unchanged — culling only skips draws
  whose geometry lies entirely outside the view.
- Debug visualizations (`api_physics_debug`) deliberately pass
  cull-disabled: culling is a `drawMesh` policy, not an engine-wide rule.
- Future tightening (a bounding-sphere pre-test, posed-bounds per clip,
  billboard/particle culling) can extend this without changing the contract.

## Rejected alternatives

- **Script-side frustum object + JS tests**: needs a new camera getter (the
  camera is write-only), re-implements the engine's math in every game, and
  cannot be enforced uniformly. Lost to the engine-side test.
- **Bounding-sphere test (center through matrix, radius × max column
  scale)**: cheaper and rotation-invariant, but strictly looser for
  elongated geometry (walls, terrain); kept as a possible future pre-test.
- **Playback-time culling in `play_records`**: record time is where camera,
  matrix, and bounds are co-located; playback stays dumb and re-orderable,
  and a culled draw should never be budgeted in the first place.
- **Opt-in `cull: true`**: default-on matches the zero-boilerplate intent;
  the opt-out preserves escape. Default-off would ship the feature off.
