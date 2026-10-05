# Spec Delta

## MODIFIED Requirements

### Requirement: Whole-mesh drawing with depth

`efx.graphics.drawMesh(mesh, opts?)` SHALL record one draw for the whole mesh, with
`mesh` as a required first positional argument that MUST be a live Mesh
(nothing, a non-Mesh, or a destroyed Mesh throws `TypeError`). `opts?` is an
optional option bag restricted to `{ transform?, color?, skinned? }`: `transform?`
is a flat array (or typed array) of exactly 16 finite numbers — a column-major
4×4 matrix, default identity (a wrong length throws `RangeError`, non-number
elements throw `TypeError`); `color?` is a `[r, g, b, a]` tint, default
opaque white; `skinned?` is a boolean, default `false` — `false`/absent draws
the mesh's bind-pose vertices and `true` draws the current CPU-posed vertices,
and `true` on a mesh without a rig throws `TypeError`. The option bag, when
present, MUST be an object; unknown option fields (including `mesh`, which is no
longer an option) SHALL throw `TypeError`. Playback SHALL draw every surface in
surface order under the recorded camera, with the depth test enabled and depth
writing on: a nearer surface occludes a farther one regardless of record order,
and equal-depth fragments resolve by record order (deterministic). Per-surface
shading SHALL be the `lighting` capability's F4 lit result — the F4a Phong
equation **including the F4b per-channel maps and alpha mask** — with albedo
equal to the tint multiplied by the surface's vertex color where the `colors`
attribute is present, or the tint alone otherwise. The surface `uvs`
attribute (validated and stored since F3) SHALL be consumed by that shading
as the map/alpha-mask texture coordinate; a surface without `uvs` uses the
`(0, 0)` default, so shading stays defined for every mesh. Each surface SHALL
blend using its bound material's `blend` override when the material specifies
one, and otherwise the frame's blend render state (see the `2d-layer` blending
requirement) in effect when the mesh draw was recorded. Both the frame state
and the per-surface material blend SHALL be resolved and value-snapshotted at
record time (ADR 0019): a later `setBlendMode` call or a later
`setMeshSurfaceMaterial` rebind MUST NOT change the recorded draw. 2D quad
records SHALL be untouched by mesh depth (F2 behavior and goldens unchanged).
Mesh draws participate in the per-frame record budget like any record.

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

#### Scenario: Mesh is a required positional argument

- **WHEN** `drawMesh` is called without a mesh (no argument), with a non-Mesh
  first argument, or with a destroyed Mesh
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Option bag holds only transform and color

- **WHEN** `drawMesh(mesh, { transform, color })` is called with a valid bag,
  or `drawMesh(mesh, { mesh })` / `drawMesh(mesh, { frobnicate: 1 })` is
  called with an unknown field
- **THEN** the valid bag records the draw (transform/color applied; the
  documented `skinned` option is also accepted), and the unknown field throws
  `TypeError` and records nothing

#### Scenario: Skinned draw on a static mesh rejected

- **WHEN** `drawMesh(mesh, { skinned: true })` is called on a mesh with no
  imported rig
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Validation errors

- **WHEN** `drawMesh(mesh, { transform })` is called with a 15-element
  `transform`
- **THEN** the call throws `RangeError` and records nothing
