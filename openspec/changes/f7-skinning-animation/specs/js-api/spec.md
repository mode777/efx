# Spec Delta

## MODIFIED Requirements

### Requirement: Skinned mesh data and implicit rig payload

`createMeshData` surfaces SHALL accept optional `joints` and `weights`
attributes for skinned meshes (four influences per vertex, glTF-style), with
the same count as the surface's positions. The skeleton and animation clips
associated with an imported skinned asset SHALL remain implicit `MeshData`/`Mesh`
payload — no separate script resource and no read-only clip or joint query
property — while posing is exposed through `efx.poseMesh` and the `skinned`
`drawMesh` option (F7). The native-backed class list and the `destroy()`
lifecycle are unchanged.

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
- **THEN** they catalog the joints/weights surface attributes, `poseMesh`, and
  the `skinned` draw option, and no skeleton/clip resource, clip/joint query
  property, or playback function

### Requirement: Gallery type document accuracy

The gallery type document `gallery/src/api/efx.d.ts` SHALL accurately
describe the valid call shapes of the public script API: every documented
call form MUST type-check, and invalid calls MUST be rejected at compile
time. It MUST type a function's alternative call forms so that each form's
required fields are required only for that form (for example, the
`createMeshData` batch bag versus its single-surface shorthand) and so that
mixing forms is rejected. It SHALL be updated in the same change as any
script-facing API change, alongside `docs/js-api.md`, and its declarations
MUST agree with that reference document.

#### Scenario: Batch form does not require shorthand fields

- **WHEN** `efx.createMeshData({ surfaces: [surface, surface] })` is
  type-checked
- **THEN** it compiles without supplying top-level `positions` or other
  shorthand attributes

#### Scenario: Mixing construction forms is rejected

- **WHEN** `efx.createMeshData({ surfaces: [surface], positions })` combines
  the batch bag and the shorthand fields in one call
- **THEN** the type document reports a compile-time error

#### Scenario: drawMesh takes a positional mesh

- **WHEN** `efx.drawMesh(mesh, { transform, color, skinned })` and
  `efx.drawMesh(mesh)` are type-checked
- **THEN** both compile, and the former option bag holds only
  `transform`/`color`/`skinned` (a `mesh` field in the bag is rejected)

#### Scenario: Posing API is typed

- **WHEN** `efx.poseMesh(mesh, { clip: 'Walk', time: 1 })` and
  `efx.poseMesh(mesh, [{ clip: 0, time: 1, weight: 0.5 }])` are type-checked
- **THEN** both compile, the sample `clip` accepts a name or index, and an
  unknown sample field is rejected

#### Scenario: Primitive material option is typed

- **WHEN** `efx.makeCube({ size: 1, material })` is type-checked with a
  material object
- **THEN** it compiles and the material argument is accepted as a material
  object or `null`

#### Scenario: Type document agrees with the reference

- **WHEN** a script-facing API change updates `docs/js-api.md`
- **THEN** the same change updates `gallery/src/api/efx.d.ts` so every
  cataloged function has a matching declaration
