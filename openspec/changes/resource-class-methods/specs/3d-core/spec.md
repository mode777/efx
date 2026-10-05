# Spec Delta

## MODIFIED Requirements

### Requirement: Mesh upload and lifecycle
`efx.graphics.createMesh(meshData)` SHALL upload a live MeshData's **every** surface
CPU → GPU into one Mesh (native-backed class, ADR 0011/0013: deterministic
`destroy()`, idempotent, GC-finalizer backstop, display-list references keep
it alive until playback completes). The Mesh SHALL expose the read-only
query property `surfaceCount` (equal to the MeshData's at upload time;
throws `TypeError` when destroyed). A Mesh is a copy: later changes to the
source MeshData object MUST NOT affect the Mesh. Passing a non-MeshData or a
destroyed MeshData SHALL throw `TypeError`. Surface material bindings
carried by the MeshData (from F4) SHALL carry over to the Mesh at upload;
from F4, `Mesh.setSurfaceMaterial(surfaceIndex, mat)` SHALL rebind
one surface's material after upload (the Godot `surface_set_material`
analog: `mat` is a JS-managed object snapshotted at call time; an index out
of range throws `RangeError`); a surface without a bound material SHALL
render with an engine default material. The former free function
`efx.graphics.setMeshSurfaceMaterial` SHALL NOT exist (hard cut, no alias).
Meshes are never slot-based; scripts
manage Mesh objects directly (ADR 0011 resource model — the roadmap's
"mesh slots" phrase is realized as resource objects, per the pinned js-api
taxonomy).

#### Scenario: All surfaces upload
- **WHEN** `createMesh` is called with a 3-surface MeshData
- **THEN** the Mesh's `surfaceCount` is 3 and drawing it renders all three
  surfaces

#### Scenario: Upload copies
- **WHEN** a Mesh is created from a MeshData and the MeshData is then
  destroyed
- **THEN** the Mesh keeps rendering identically (destroyed-use rules apply
  only to the destroyed object)

#### Scenario: Surface material is rebound through the Mesh
- **WHEN** a script calls `mesh.setSurfaceMaterial(0, material)` after upload
- **THEN** surface 0 renders with a snapshot of `material`, and the former
  `efx.graphics.setMeshSurfaceMaterial` is `undefined`

#### Scenario: Destroyed mesh is safe
- **WHEN** `destroy()` is called on a Mesh twice and a `drawMesh` references
  it after the first call
- **THEN** the second `destroy()` is a no-op and the draw throws `TypeError`
