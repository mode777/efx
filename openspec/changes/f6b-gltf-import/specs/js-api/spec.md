# Spec Delta

## ADDED Requirements

### Requirement: glTF mesh import API

The script API SHALL provide a glTF import function that returns a `MeshData`
with surfaces, converted and bound materials, and imported textures. The
function is C-implemented and synchronous from the script's point of view; the
resulting `MeshData` is the existing native-backed class, releasable with
`destroy()`, and `createMesh(meshData)` uploads it. The optional selection
object SHALL accept a mesh index or name and SHALL reject unknown fields with
`TypeError`. A load, format, or selection failure SHALL throw a standard ES6
`Error`. The reference document and the gallery type document SHALL be updated
in the same change, and the provisional `loadMesh` entry SHALL be removed.

#### Scenario: Import returns MeshData
- **WHEN** a script calls the import function on a glTF asset
- **THEN** it receives a `MeshData` whose surfaces carry imported geometry and
  whose materials are bound with imported textures

#### Scenario: Import then upload
- **WHEN** a script passes the imported `MeshData` to `createMesh`
- **THEN** the mesh uploads and draws with its imported materials

#### Scenario: Mesh selector validated
- **WHEN** the selection object names an unknown mesh or contains an unknown
  field
- **THEN** the call throws (`Error` for an unknown mesh, `TypeError` for an
  unknown field) and returns no resource

#### Scenario: No loadMesh convenience
- **WHEN** the API reference is read after this change
- **THEN** it catalogs the import function and does not catalog a separate
  `loadMesh`
