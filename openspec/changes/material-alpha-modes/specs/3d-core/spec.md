# Spec Delta

## MODIFIED Requirements

### Requirement: Whole-mesh drawing with depth

`efx.graphics.drawMesh(mesh, opts?)` SHALL record one draw for the whole mesh, with
`mesh` as a required first positional argument that MUST be a live Mesh
(nothing, a non-Mesh, or a destroyed Mesh throws `TypeError`). `opts?` is an
optional option bag restricted to `{ transform?, color?, skinned?, depthWrite? }`: `transform?`
is a flat array (or typed array) of exactly 16 finite numbers — a column-major
4×4 matrix, default identity (a wrong length throws `RangeError`, non-number
elements throw `TypeError`); `color?` is a `[r, g, b, a]` tint, default
opaque white; `skinned?` is a boolean, default `false` — `false`/absent draws
the mesh's bind-pose vertices and `true` draws the current CPU-posed vertices,
and `true` on a mesh without a rig throws `TypeError`; `depthWrite?` is a
boolean, default `true` (a non-boolean throws `TypeError`). The option bag,
when present, MUST be an object; unknown option fields (including `mesh`,
which is no longer an option) SHALL throw `TypeError`. Playback SHALL draw
every surface in surface order under the recorded camera, with the depth test
enabled: a nearer surface occludes a farther one regardless of record order,
and equal-depth fragments resolve by record order (deterministic).

Within each render-target pass segment, playback SHALL split mesh surfaces by
the bound material's `alphaMode` (see the `lighting` Phong material model
requirement). Surfaces in `'opaque'` or `'mask'` mode SHALL take the **full
rendering path**: they draw in record order in the main sweep, depth-tested
and depth-writing, so a nearer surface occludes a farther one regardless of
record order. Surfaces in `'blend'` mode SHALL be **deferred to the pass's
transparent sub-pass**, drawn after every opaque/mask surface of that pass has
drawn, sorted **back-to-front by record origin** — the recorded draw's
transform translation transformed into the recorded camera's view space — with
ties and equal origins resolved by record order (a stable, deterministic sort;
the sort key is not affected by the D3D11/Metal depth-range remap, which is
monotonic). Blend-mode surfaces always depth-test but never depth-write: the
`depthWrite` option SHALL be ignored for them, so anything drawn later —
opaque or transparent — is never occluded by a transparent surface. Within one
transparent surface, triangle order is unchanged; sorting granularity is the
record origin, which is documented as approximate for a single stretched mesh
whose parts span very different depths.

Depth **writing** SHALL be enabled when `depthWrite` is `true` and disabled when
it is `false` for opaque/mask surfaces; with `depthWrite: false` the mesh is
depth-tested against the existing depth buffer but MUST NOT change it, so any
geometry recorded after it (near or far) is not occluded by it. The option
SHALL be value-snapshotted at record time (ADR 0019), like the other draw
state. The `alphaMode` route SHALL likewise be snapshotted per surface at
record time through the material binding: a later `setMeshSurfaceMaterial`
rebind MUST NOT change the recorded draw's routing or sort participation.

Per-surface
shading SHALL be the `lighting` capability's F4 lit result — the F4a Phong
equation **including the F4b per-channel maps and alpha mask**, or the F4a
**unlit** result when the bound material sets `unlit: true` — with albedo
equal to the tint multiplied by the surface's vertex color where the `colors`
attribute is present, or the tint alone otherwise. The surface `uvs`
attribute (validated and stored since F3) SHALL be consumed by that shading
as the map/alpha-mask texture coordinate; a surface without `uvs` uses the
`(0, 0)` default, so shading stays defined for every mesh. Each surface SHALL
blend using its bound material's `blend` override when the material specifies
one, and otherwise the frame's blend render state (see the `2d-layer` blending
requirement) in effect when the mesh draw was recorded. The blend equation
never selects the pass: an `'additive'` or `'subtractive'` surface in
`'opaque'` mode draws in the main sweep exactly as today, and the same
equation inside `'blend'` mode combines in the sorted transparent sub-pass.
Both the frame state
and the per-surface material blend SHALL be resolved and value-snapshotted at
record time (ADR 0019): a later `setBlendMode` call or a later
`setMeshSurfaceMaterial` rebind MUST NOT change the recorded draw. 2D quad
records SHALL be untouched by mesh depth and by the transparent sub-pass (F2
behavior and goldens unchanged); quads, sprite batches, text, billboards, and
particles keep their existing draw order and blend behavior. Mesh draws
participate in the per-frame record budget like any record.

#### Scenario: Multi-surface mesh draws all surfaces

- **WHEN** a 2-surface mesh is drawn where the surfaces occupy different
  screen areas
- **THEN** both surfaces appear, in surface order

#### Scenario: Depth test occludes by distance

- **WHEN** a nearer mesh is recorded before a farther mesh that occupies the
  same screen area
- **THEN** the nearer mesh's fragments win the depth test despite the later
  record

#### Scenario: Vertex colors multiply the tint

- **WHEN** a surface with per-vertex colors is drawn with
  `color: [r, g, b, a]`
- **THEN** each fragment's albedo is the vertex color multiplied
  component-wise by the tint (the tint alone where the surface has no vertex
  colors), and that albedo is what the lighting equation shades

#### Scenario: Transform positions the mesh

- **WHEN** the same mesh is drawn twice with different `transform` matrices
  (e.g. translation versus identity)
- **THEN** the two renders appear at different world positions per the
  recorded matrices

#### Scenario: Surface uvs drive bound maps

- **WHEN** two surfaces of the same mesh have different `uvs` and a bound
  `diffuse.map`
- **THEN** each surface samples the map at its own interpolated `uv`, and a
  surface with no `uvs` samples `(0, 0)`

#### Scenario: Per-surface blend resolves at record time

- **WHEN** a mesh whose surface 0 has `blend: 'additive'` and surface 1 has no
  blend override is drawn while the frame state is `'alpha'`, and the frame
  state is then changed before playback
- **THEN** surface 0 is additive and surface 1 is alpha, using the values in
  effect at record time

#### Scenario: Depth write can be disabled

- **WHEN** a mesh is drawn with `depthWrite: false` and then a farther mesh
  occupying the same screen area is recorded after it
- **THEN** the later, farther mesh renders where it is visible, because the
  first draw wrote no depth

#### Scenario: Disabled depth write still occludes against existing depth

- **WHEN** an opaque mesh is recorded first, then a mesh is drawn with
  `depthWrite: false` at a position behind it
- **THEN** the second mesh is not visible through the first, because it is
  still depth-tested against the existing depth buffer

#### Scenario: Depth write is snapshotted at record time

- **WHEN** a mesh draw is recorded with `depthWrite: false`, and then another
  draw is recorded with the default, and the frame plays back
- **THEN** only the first draw skips depth writing

#### Scenario: Blend-mode surfaces defer to the transparent pass

- **WHEN** a `'blend'`-mode mesh is recorded between two opaque meshes, all
  overlapping on screen
- **THEN** the blend mesh composites over the opaque content drawn before and
  after it in record order, because both opaque meshes draw in the main sweep
  before the transparent sub-pass

#### Scenario: Transparent surfaces sort back-to-front by origin

- **WHEN** a far `'blend'`-mode mesh is recorded before a near
  `'blend'`-mode mesh, both overlapping on screen
- **THEN** playback draws the far mesh first and the near mesh second —
  sorted by origin against the recorded camera, not by record order — so the
  near mesh's nearer fragments composite correctly over the far one

#### Scenario: Blend mode forces no depth write

- **WHEN** a `'blend'`-mode mesh is recorded, then an opaque mesh farther from
  the camera is recorded behind it, overlapping on screen
- **THEN** the farther opaque mesh renders where visible, because the
  transparent surface wrote no depth even if `depthWrite` defaulted to true

#### Scenario: Blend equations do not route

- **WHEN** the frame blend state is `'additive'` and a mesh with the default
  `alphaMode` is drawn
- **THEN** it draws in the main sweep with depth writing, exactly as with the
  `'alpha'` frame state; routing follows only the material's `alphaMode`

#### Scenario: Mesh is a required positional argument

- **WHEN** `drawMesh` is called without a mesh (no argument), with a non-Mesh
  first argument, or with a destroyed Mesh
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Option bag holds only transform and color

- **WHEN** `drawMesh(mesh, { transform, color })` is called with a valid bag,
  or `drawMesh(mesh, { mesh })` / `drawMesh(mesh, { frobnicate: 1 })` is
  called with an unknown field
- **THEN** the valid bag records the draw (transform/color applied; the
  documented `skinned` and `depthWrite` options are also accepted), and the
  unknown field throws `TypeError` and records nothing

#### Scenario: Skinned draw on a static mesh rejected

- **WHEN** `drawMesh(mesh, { skinned: true })` is called on a mesh with no
  imported rig
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Validation errors

- **WHEN** `drawMesh(mesh, { transform })` is called with a 15-element
  `transform`, or `drawMesh(mesh, { depthWrite: 1 })` is called
- **THEN** the call throws (`RangeError` for the transform length,
  `TypeError` for the non-boolean) and records nothing
