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
and equal-depth fragments resolve by record order (deterministic). Depth
**writing** SHALL be enabled when `depthWrite` is `true` and disabled when it
is `false`; with `depthWrite: false` the mesh is depth-tested against the
existing depth buffer but MUST NOT change it, so any geometry recorded after
it (near or far) is not occluded by it. The option SHALL be value-snapshotted
at record time (ADR 0019), like the other draw state. Per-surface
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

### Requirement: Procedural primitives

The engine-bundled pure-JS primitives `efx.graphics.makeCube(opts?)`,
`efx.graphics.makePlane(opts?)`, and `efx.graphics.makeSphere(opts?)` SHALL each return a
single-surface MeshData (directly usable by `createMesh`), with pinned
defaults so scenes are reproducible:

- `makeCube({ size? = 1, inverted? = false, material? })` — an axis-aligned
  cube centered on the origin with side length `size`, per-face normals,
  outward-facing (CCW) winding, and per-face uv mapping of the full 0..1
  square; when `inverted` is `true` the per-face normals point inward and the
  winding is reversed so the inside of the cube is front-facing;
- `makePlane({ size? = 1, segments? = 1, material? })` — a plane in the XZ
  plane facing `+Y`, centered on the origin with side length `size`,
  subdivided into `segments × segments` quads, uv coordinates spanning 0..1;
- `makeSphere({ radius? = 1, segments? = 16, inverted? = false, material? })` —
  a UV sphere centered on the origin with radius `radius` and `segments`
  latitude and longitude bands, normals equal to normalized positions,
  equirectangular uv spanning 0..1; when `inverted` is `true` the normals
  point inward and the winding is reversed so the inside of the sphere is
  front-facing.

A non-finite or non-positive `size`/`radius` SHALL throw `RangeError`;
`segments` SHALL be a positive integer (`RangeError` otherwise); a non-boolean
`inverted` SHALL throw `TypeError`; unknown fields SHALL throw `TypeError`.
Missing option objects take all defaults.

When `material` is present it SHALL be bound to the primitive's single
surface at MeshData creation, exactly as `createMeshData`'s parallel
`materials` array binds a material to its surface: the material is a
JS-managed object snapshotted at call time, the binding carries onto the
Mesh at `createMesh`, and an omitted or `null` `material` selects the engine
default material. Material validation SHALL reuse the existing binding
rules (unknown material/channel fields and non-`null`/`undefined` values
that are not material objects throw `TypeError`).

#### Scenario: Defaults produce a usable mesh
- **WHEN** `efx.graphics.makeCube()` is called with no arguments
- **THEN** the result is a single-surface MeshData with `surfaceCount` 1
  that uploads and draws like any other MeshData

#### Scenario: Parameter overrides
- **WHEN** `makeSphere({ radius: 2, segments: 24 })` is called
- **THEN** the surface's positions lie on a radius-2 sphere and the vertex
  count follows the 24-band layout deterministically

#### Scenario: Inverted primitive faces inward
- **WHEN** `makeCube({ inverted: true })` or `makeSphere({ inverted: true })`
  is created and drawn with the camera inside it
- **THEN** the inside surface renders (the winding is reversed and the normals
  point inward), where the non-inverted primitive would be back-face culled

#### Scenario: Invalid parameters throw
- **WHEN** `makeCube({ size: 0 })`, `makePlane({ segments: 1.5 })`, or
  `makeCube({ inverted: 1 })` is called
- **THEN** the call throws (`RangeError` for the numeric bounds,
  `TypeError` for the non-boolean)

#### Scenario: Primitive binds its material at creation
- **WHEN** `makeCube({ size: 1, material })` is called and the result is
  passed to `createMesh`
- **THEN** the primitive's single surface carries `material` (snapshotted at
  call time) and the Mesh renders it, while an omitted or `null` `material`
  renders with the engine default material

#### Scenario: Invalid material throws
- **WHEN** `makePlane({ material: 42 })` is called with a value that is not
  a material object
- **THEN** the call throws `TypeError`
