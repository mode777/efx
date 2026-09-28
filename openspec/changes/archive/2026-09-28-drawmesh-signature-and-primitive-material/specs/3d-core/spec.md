# Spec Delta

## MODIFIED Requirements

### Requirement: Whole-mesh drawing with depth

`efx.drawMesh(mesh, opts?)` SHALL record one draw for the whole mesh, with
`mesh` as a required first positional argument that MUST be a live Mesh
(nothing, a non-Mesh, or a destroyed Mesh throws `TypeError`). `opts?` is an
optional option bag restricted to `{ transform?, color? }`: `transform?` is a
flat array (or typed array) of exactly 16 finite numbers — a column-major 4×4
matrix, default identity (a wrong length throws `RangeError`, non-number
elements throw `TypeError`); `color?` is a `[r, g, b, a]` tint, default
opaque white. The option bag, when present, MUST be an object; unknown option
fields (including `mesh`, which is no longer an option) SHALL throw
`TypeError`. Playback SHALL draw every surface in surface order under the
recorded camera, with the depth test enabled and depth writing on: a nearer
surface occludes a farther one regardless of record order, and equal-depth
fragments resolve by record order (deterministic). Per-surface shading SHALL
be the `lighting` capability's F4 lit result — the F4a Phong equation
**including the F4b per-channel maps and alpha mask** — with albedo equal to
the tint multiplied by the surface's vertex color where the `colors`
attribute is present, or the tint alone otherwise. The surface `uvs`
attribute (validated and stored since F3) SHALL be consumed by that shading
as the map/alpha-mask texture coordinate; a surface without `uvs` uses the
`(0, 0)` default, so shading stays defined for every mesh. 2D quad records
SHALL be untouched by mesh depth (F2 behavior and goldens unchanged). Mesh
draws participate in the per-frame record budget like any record.

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

#### Scenario: Mesh is a required positional argument

- **WHEN** `drawMesh` is called without a mesh (no argument), with a non-Mesh
  first argument, or with a destroyed Mesh
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Option bag holds only transform and color

- **WHEN** `drawMesh(mesh, { transform, color })` is called with a valid bag,
  or `drawMesh(mesh, { mesh })` / `drawMesh(mesh, { frobnicate: 1 })` is
  called with an unknown field
- **THEN** the valid bag records the draw (with transform/color applied), and
  the unknown field throws `TypeError` and records nothing

#### Scenario: Validation errors

- **WHEN** `drawMesh(mesh, { transform })` is called with a 15-element
  `transform`
- **THEN** the call throws `RangeError` and records nothing

### Requirement: Procedural primitives

The engine-bundled pure-JS primitives `efx.makeCube(opts?)`,
`efx.makePlane(opts?)`, and `efx.makeSphere(opts?)` SHALL each return a
single-surface MeshData (directly usable by `createMesh`), with pinned
defaults so scenes are reproducible:

- `makeCube({ size? = 1, material? })` — an axis-aligned cube centered on the
  origin with side length `size`, outward per-face normals, outward-facing
  (CCW) winding, and per-face uv mapping of the full 0..1 square;
- `makePlane({ size? = 1, segments? = 1, material? })` — a plane in the XZ
  plane facing `+Y`, centered on the origin with side length `size`,
  subdivided into `segments × segments` quads, uv coordinates spanning 0..1;
- `makeSphere({ radius? = 1, segments? = 16, material? })` — a UV sphere
  centered on the origin with radius `radius` and `segments` latitude and
  longitude bands, normals equal to normalized positions, equirectangular uv
  spanning 0..1.

A non-finite or non-positive `size`/`radius` SHALL throw `RangeError`;
`segments` SHALL be a positive integer (`RangeError` otherwise); unknown
fields SHALL throw `TypeError`. Missing option objects take all defaults.

When `material` is present it SHALL be bound to the primitive's single
surface at MeshData creation, exactly as `createMeshData`'s parallel
`materials` array binds a material to its surface: the material is a
JS-managed object snapshotted at call time, the binding carries onto the
Mesh at `createMesh`, and an omitted or `null` `material` selects the engine
default material. Material validation SHALL reuse the existing binding
rules (unknown material/channel fields and non-`null`/`undefined` values
that are not material objects throw `TypeError`).

#### Scenario: Defaults produce a usable mesh
- **WHEN** `efx.makeCube()` is called with no arguments
- **THEN** the result is a single-surface MeshData with `surfaceCount` 1
  that uploads and draws like any other MeshData

#### Scenario: Parameter overrides
- **WHEN** `makeSphere({ radius: 2, segments: 24 })` is called
- **THEN** the surface's positions lie on a radius-2 sphere and the vertex
  count follows the 24-band layout deterministically

#### Scenario: Invalid parameters throw
- **WHEN** `makeCube({ size: 0 })` or `makePlane({ segments: 1.5 })` is
  called
- **THEN** the call throws `RangeError`

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
