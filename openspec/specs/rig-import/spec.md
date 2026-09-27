# rig-import Specification

## Purpose

Defines how glTF skin, skeleton, and animation-clip data is imported into the
engine's mesh resources — per-surface joints/weights attributes plus an opaque
rig payload carried from `MeshData` into `Mesh` — with no script-facing
playback, which is F7's concern.

## Requirements

### Requirement: Skinned surface attributes from glTF

When an imported glTF primitive carries `JOINTS_0`/`WEIGHTS_0` attributes, the
importer SHALL populate the corresponding `MeshData` surface's joints/weights
attributes with four influences per vertex, in the same vertex count as the
surface's positions. A primitive that has joints but no weights, or mismatched
counts, SHALL fail the import with an error.

#### Scenario: Skin attributes imported
- **WHEN** a script imports a glTF mesh whose primitives carry
  `JOINTS_0`/`WEIGHTS_0`
- **THEN** each surface's joints/weights attributes hold four influences per
  vertex matching the primitive's vertex count

#### Scenario: Non-skinned asset unchanged
- **WHEN** a script imports a glTF mesh with no skin attributes
- **THEN** the surfaces import as static geometry, exactly as in F6b

#### Scenario: Malformed skin attributes error
- **WHEN** a primitive's joints and weights counts do not match its vertices
- **THEN** the import fails with an error

### Requirement: Skeleton payload

When the imported mesh is bound to a glTF skin, the importer SHALL bundle the
skin's joint hierarchy and inverse bind matrices into the returned `MeshData`
as an opaque rig payload, carried into the `Mesh` by `createMesh`. A mesh
without a skin SHALL carry no rig payload.

#### Scenario: Skin bundled into the mesh payload
- **WHEN** a script imports a skinned asset and creates the mesh
- **THEN** the resulting `Mesh` carries the skeleton (joints and inverse bind
  matrices) internally, with no separate script-held resource

#### Scenario: Static asset has no rig
- **WHEN** a script imports an asset with no skin
- **THEN** the mesh carries no rig payload and behaves exactly as an F6b mesh

### Requirement: Animation clip payload and interpolation

The importer SHALL bundle every glTF `animations[]` entry into the mesh as an
opaque clip payload — its channels (target node path and interpolation) and
keyframe samplers. LINEAR and STEP samplers SHALL be imported exactly;
CUBICSPLINE samplers SHALL be approximated as LINEAR with tangents discarded.
An asset with no animations SHALL carry no clips.

#### Scenario: Clips bundled
- **WHEN** a script imports a glTF asset with animation clips
- **THEN** the mesh carries those clips internally, referenceable by the
  engine in F7 (name or index)

#### Scenario: Cubic spline approximated
- **WHEN** a clip uses CUBICSPLINE interpolation
- **THEN** it imports and samples linearly, with tangents discarded, and the
  import does not fail

#### Scenario: Clip-less asset
- **WHEN** a script imports an asset with no animations
- **THEN** the mesh carries no clips

### Requirement: Rig payload is opaque

Imported joints/weights, skeleton, and clips SHALL NOT add any script-facing
function or read-only query property; the rig is reachable only through the
engine's future posing API (F7). The mesh remains the existing native-backed
class with `destroy()` and its documented `surfaceCount`.

#### Scenario: No new script surface
- **WHEN** the API reference and type document are read after this change
- **THEN** no clip/joint query function or property is cataloged, and the
  joints/weights surface attributes are the only new script-visible data

### Requirement: Rig import lifetime

The rig payload SHALL be released with the mesh that owns it — `MeshData`
before upload, `Mesh` after — and SHALL NOT outlive either, so imported rigs
follow the existing native-backed resource lifecycle.

#### Scenario: Destroy releases the rig
- **WHEN** a script destroys the mesh or MeshData that carries an imported rig
- **THEN** the rig payload is released with it and later use throws
