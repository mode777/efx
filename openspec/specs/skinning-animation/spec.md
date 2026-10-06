# skinning-animation Specification

## Purpose
Turns an imported glTF rig (joints/weights, skeleton, animation clips) into
visible motion: the script samples clips with `poseMesh` and draws the
CPU-skinned result, with no engine-side playback state.

## Requirements

### Requirement: CPU skinning of imported rigs

A `Mesh` carrying an imported rig SHALL support CPU linear-blend skinning: each
posed vertex position/normal SHALL be the weighted sum of the surface's
joint-influence transforms applied to the retained bind-pose vertex, where the
joint transforms are derived from the skeleton's joint hierarchy, inverse bind
matrices, and the currently sampled clip pose. Posing SHALL NOT alter the
retained bind-pose data. A mesh whose surfaces carry joints/weights but whose
rig resolves no usable joint transform SHALL fall back to the bind pose rather
than fail.

#### Scenario: Posing changes vertices

- **WHEN** a script poses a skinned mesh at two different clip times and draws
  each with `skinned: true`
- **THEN** the two renders differ according to the skinned joint transforms

#### Scenario: Bind-pose data is retained

- **WHEN** a skinned mesh has been posed and is then drawn without
  `skinned: true`
- **THEN** it renders exactly as its unposed bind pose

### Requirement: Script-driven posing with poseMesh

`Mesh.pose(pose)` SHALL CPU-pose a live skinned `Mesh` in place, with the
receiver `Mesh` as the subject. `pose` SHALL be either a single sample
`{ clip, time, weight? }` or an array of such samples (a weighted blend).
`clip` SHALL be a clip name (the glTF `name`, or the stable internal `clipN`
when unnamed) or a clip index. `time` SHALL be in seconds and wrapped modulo
the clip's length. When samples carry `weight`, the weights SHALL be
normalized engine-side before blending; a negative weight SHALL throw. A mesh
with no rig, a non-Mesh or destroyed Mesh, an unknown clip, or an
unknown/mistyped sample field SHALL throw (`TypeError` for types and unknown
fields, `RangeError` for indices/slot ranges). Posing SHALL keep no engine
playback state — the script owns the clock. The former free function
`efx.graphics.poseMesh` SHALL NOT exist (hard cut, no alias).

#### Scenario: Single-clip sample

- **WHEN** a script calls `mesh.pose({ clip: 'Walk', time: t })`
- **THEN** the mesh's posed buffer reflects the clip sampled at `t`

#### Scenario: Clip lookup by name or index

- **WHEN** a script references a clip by its name and by its index
- **THEN** both resolve to the same clip and pose identically

#### Scenario: Weighted blend

- **WHEN** a script passes `[{ clip: 'Walk', time: t, weight: 1 - k },
  { clip: 'Run', time: t, weight: k }]`
- **THEN** the posed result blends the two clips by the normalized weights

#### Scenario: Time wraps

- **WHEN** a script passes a `time` beyond the clip's length
- **THEN** the sample wraps modulo the clip length and posing succeeds

#### Scenario: Negative weight rejected

- **WHEN** a pose sample supplies a negative weight
- **THEN** the call throws and the posed buffer is unchanged

#### Scenario: Rig-less or unknown clip rejected

- **WHEN** `Mesh.pose` is called on a mesh with no rig, on a destroyed/non-Mesh
  value, or with a clip name/index that does not exist
- **THEN** the call throws (`TypeError` for mesh/type problems, the appropriate
  error for the unknown clip) and records no pose

### Requirement: Posed and bind-pose vertex buffers

A skinned `Mesh` SHALL retain its immutable bind-pose vertex buffer and SHALL
own a separate posed vertex buffer that `Mesh.pose` writes. `efx.graphics.drawMesh(mesh,
{ skinned: true })` SHALL draw the posed buffer; a draw without `skinned` (or
with `skinned: false`) SHALL draw the bind-pose buffer. `skinned: true` on a
mesh without a rig SHALL throw `TypeError`. The extra posed buffer's CPU memory
SHALL follow the existing native-backed resource lifecycle and count toward GC
pressure.

#### Scenario: Skinned draw selects the posed buffer

- **WHEN** a mesh has been posed and is drawn once with `skinned: true` and
  once without it
- **THEN** the `skinned: true` draw uses the posed vertices and the other uses
  the bind-pose vertices

#### Scenario: Skinned flag on a static mesh rejected

- **WHEN** `drawMesh(mesh, { skinned: true })` is called on a mesh with no rig
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Posed buffer released with the mesh

- **WHEN** a script destroys a skinned mesh
- **THEN** both the bind-pose and posed buffers are released and later use
  throws

### Requirement: Rig remains implicit and script-facing state is stateless

Posing SHALL NOT add a new native-backed resource class, a separate
skeleton/clip resource, or a read-only rig query property. Skins, skeletons, and
clips SHALL remain implicit `Mesh` payload reachable only through `Mesh.pose`
and the `skinned` draw option. No `play`/`pause`/`blend` engine state or
function SHALL be added; any such convenience would be a pure-JS layer above
`Mesh.pose`.

#### Scenario: No rig resources or queries

- **WHEN** the API reference and gallery type document are read after this
  change
- **THEN** they catalog `Mesh.pose` and the `skinned` draw option and no
  skeleton/clip resource, clip/joint query property, or playback function
