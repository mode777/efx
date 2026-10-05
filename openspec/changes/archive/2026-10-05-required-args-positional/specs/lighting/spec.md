# Spec Delta

## MODIFIED Requirements

### Requirement: Per-surface material binding

Materials SHALL bind to mesh **surfaces**, never to global engine state
(ADR 0024). `efx.graphics.setMeshSurfaceMaterial(mesh, surfaceIndex, mat)` SHALL bind a
snapshot of `mat` to surface `surfaceIndex` of a live `mesh`; `mat` SHALL be
a material object or `null` (bind the engine default material). `mesh` MUST be
a live Mesh (`TypeError` otherwise), `surfaceIndex` an integer in
`0..surfaceCount-1` (`RangeError` otherwise), and `mat` an object or `null`
(`TypeError` otherwise). The call changes only that surface's binding.

`efx.graphics.createMeshData(surfaces, materials?)` SHALL accept an optional positional
`materials` array parallel to `surfaces`: `materials[i]` (a material object
or `null` for the default) becomes surface `i`'s initial binding. When present,
`materials` MUST have exactly one entry per surface (wrong length SHALL throw
`RangeError`; invalid entries SHALL throw `TypeError`). A surface with no
binding, or bound to `null`, SHALL render with the engine **default material**
— white diffuse Phong with no maps and no emissive — and surface bindings
SHALL carry over unchanged at `createMesh`.

#### Scenario: Bind and rebind a surface

- **WHEN** `setMeshSurfaceMaterial(mesh, 1, A)` is followed by
  `setMeshSurfaceMaterial(mesh, 1, B)`
- **THEN** surface 1 renders with B and the mesh's other surfaces are
  unchanged

#### Scenario: Materials bind at creation

- **WHEN** `createMeshData([s0, s1], [A, null])` is
  used to create a mesh
- **THEN** surface 0 renders with material A and surface 1 renders with the
  default material

#### Scenario: Out-of-range index or wrong length throws

- **WHEN** `setMeshSurfaceMaterial(mesh, 2, mat)` is called on a 2-surface
  mesh, or `materials` has fewer/more entries than `surfaces`
- **THEN** the call throws `RangeError` and changes nothing
