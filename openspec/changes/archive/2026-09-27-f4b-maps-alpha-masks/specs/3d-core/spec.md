# Spec Delta

## MODIFIED Requirements

### Requirement: Whole-mesh drawing with depth

`efx.drawMesh(opts)` SHALL record one draw for the whole mesh from the bag
`{ mesh, transform?, color? }`: `mesh` is required and MUST be a live Mesh
(nothing, a non-Mesh, or a destroyed Mesh throws `TypeError`);
`transform?` is a flat array (or typed array) of exactly 16 finite numbers —
a column-major 4×4 matrix, default identity (a wrong length throws
`RangeError`, non-number elements throw `TypeError`); `color?` is a
`[r, g, b, a]` tint, default opaque white. Unknown option fields SHALL throw
`TypeError`. Playback SHALL draw every surface in surface order under the
recorded camera, with the depth test enabled and depth writing on: a
nearer surface occludes a farther one regardless of record order, and
equal-depth fragments resolve by record order (deterministic). Per-surface
shading SHALL be the `lighting` capability's F4 lit result — the F4a Phong
equation **including the F4b per-channel maps and alpha mask** — with albedo
equal to the tint multiplied by the surface's vertex color where the
`colors` attribute is present, or the tint alone otherwise. The surface
`uvs` attribute (validated and stored since F3) SHALL be consumed by that
shading as the map/alpha-mask texture coordinate; a surface without `uvs`
uses the `(0, 0)` default, so shading stays defined for every mesh. 2D quad
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

#### Scenario: Validation errors

- **WHEN** `drawMesh` is called with no `mesh`, a 15-element `transform`, or
  an unknown option field
- **THEN** the call throws `TypeError` / `RangeError` / `TypeError`
  respectively and records nothing
