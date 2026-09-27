# Spec Delta

## ADDED Requirements

### Requirement: Skinned mesh data and implicit rig payload

`createMeshData` surfaces SHALL accept optional `joints` and `weights`
attributes for skinned meshes (four influences per vertex, glTF-style), with
the same count as the surface's positions. The skeleton and animation clips
associated with an imported skinned asset SHALL be implicit `MeshData`/`Mesh`
payload — no script resource, no function, and no read-only query property is
added; the native-backed class list and the `destroy()` lifecycle are
unchanged. Playback is delivered by F7 (`poseMesh` / `drawMesh` `skinned`).

#### Scenario: Skinned surface accepted
- **WHEN** `createMeshData` receives a surface with `joints` and `weights`
  arrays matching its vertex count
- **THEN** the MeshData is built and `createMesh` carries the attributes and
  any imported rig payload onto the `Mesh`

#### Scenario: Attribute count mismatch rejected
- **WHEN** a surface's `joints` or `weights` count does not match its vertex
  count
- **THEN** `createMeshData` throws `RangeError` and records nothing

#### Scenario: No new rig API
- **WHEN** the API reference and gallery type document are read after this
  change
- **THEN** they catalog the joints/weights surface attributes and no clip or
  joint query function or property
