# Spec Delta

## ADDED Requirements

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

- **WHEN** `efx.drawMesh(mesh, { transform, color })` and
  `efx.drawMesh(mesh)` are type-checked
- **THEN** both compile, and the former option bag holds only
  `transform`/`color` (a `mesh` field in the bag is rejected)

#### Scenario: Primitive material option is typed

- **WHEN** `efx.makeCube({ size: 1, material })` is type-checked with a
  material object
- **THEN** it compiles and the material argument is accepted as a material
  object or `null`

#### Scenario: Type document agrees with the reference

- **WHEN** a script-facing API change updates `docs/js-api.md`
- **THEN** the same change updates `gallery/src/api/efx.d.ts` so every
  cataloged function has a matching declaration
