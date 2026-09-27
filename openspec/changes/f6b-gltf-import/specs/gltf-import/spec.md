# Spec Delta

## Purpose

Defines how the engine imports glTF 2.0 static assets — geometry, materials,
and textures — through the resource provider into engine `MeshData` and
`Texture` resources, including the pinned container/reference profile,
primitive-to-surface mapping, and PBR-to-Phong material conversion.

## ADDED Requirements

### Requirement: glTF container and reference support

The importer SHALL load glTF 2.0 assets in both the binary `.glb` container and
the text `.gltf` container, resolved through the resource provider. Buffers and
images referenced by a `.gltf` asset SHALL be supported when they are external
files, data-URIs, or — for `.glb` — buffer views. External references SHALL
resolve relative to the asset's path within the resource root. A malformed,
truncated, or otherwise unreadable asset SHALL fail to import with an error.

#### Scenario: Load a self-contained GLB
- **WHEN** a script imports a `.glb` asset
- **THEN** its geometry, materials, and embedded images are imported

#### Scenario: Load a GLTF with external files
- **WHEN** a script imports a `.gltf` asset whose buffer and images are
  external files next to it in the resource root
- **THEN** those files are resolved and imported

#### Scenario: Data-URI references
- **WHEN** a `.gltf` asset embeds a buffer or image as a data-URI
- **THEN** the importer decodes it without an external file

#### Scenario: Corrupt asset errors
- **WHEN** a script imports a file that is not valid glTF 2.0
- **THEN** the import fails with an error and returns no resource

### Requirement: Mesh selection and primitive-to-surface mapping

`loadMeshData(path, opts?)` SHALL import exactly one glTF mesh, selected by
`opts.mesh` as an index or a name, defaulting to the first mesh. Each of the
selected mesh's primitives SHALL become one `MeshData` surface, in primitive
order, subject to the fixed 1..16 surface cap. A selection that names no mesh,
or a mesh whose primitive count exceeds the surface cap, SHALL fail with an
error. Geometry attributes (positions, normals, texture coordinates, colors)
and indices SHALL be imported per primitive where present.

#### Scenario: Default mesh
- **WHEN** a script imports an asset without `opts.mesh`
- **THEN** the first mesh's primitives become the surfaces

#### Scenario: Select by index or name
- **WHEN** a script passes `opts.mesh` as a mesh index or mesh name
- **THEN** that mesh's primitives are imported

#### Scenario: Unknown mesh errors
- **WHEN** `opts.mesh` names an index or name that does not exist
- **THEN** the import fails with an error

#### Scenario: Surface cap enforced
- **WHEN** the selected mesh has more primitives than the fixed surface cap
- **THEN** the import fails with an error rather than truncating

### Requirement: PBR-to-Phong material conversion

Each imported primitive's glTF material SHALL be converted to the engine's
Phong material and bound to the corresponding surface. The conversion SHALL
map base color to diffuse, emissive to emissive, metallic to the specular
color, and roughness to shininess. A material's alpha mode of `MASK` SHALL
produce an alpha mask; occlusion and normal textures SHALL be ignored. A
primitive with no material SHALL use the engine default material.

#### Scenario: Base color and emissive
- **WHEN** a glTF material defines base color and emissive factors or textures
- **THEN** the bound Phong material's diffuse and emissive channels carry them

#### Scenario: Metallic and roughness
- **WHEN** a glTF material defines metallic and roughness factors
- **THEN** the bound Phong material's specular color and shininess are derived
  from them respectively

#### Scenario: Alpha-mask material
- **WHEN** a glTF material uses `alphaMode: MASK`
- **THEN** the bound material carries an alpha mask that discards fragments
  below the mask threshold

#### Scenario: Unlit or absent material
- **WHEN** a primitive has no glTF material
- **THEN** its surface renders with the engine default material

### Requirement: Texture import with samplers

Images referenced by imported materials SHALL be decoded and uploaded as engine
`Texture`s, then bound as the corresponding material channel's map. Each
texture's wrap and filter SHALL follow its glTF sampler where the engine
supports the setting; absent samplers SHALL use the engine defaults. A texture
used by multiple materials or channels SHALL be imported consistently, and
images that cannot be decoded SHALL fail the import with an error.

#### Scenario: Sampler honored
- **WHEN** a glTF texture uses a repeat or clamp wrap mode and a nearest or
  linear filter
- **THEN** the imported `Texture` uses that wrap and filter

#### Scenario: Base color texture bound
- **WHEN** a material references a base-color texture
- **THEN** the imported surface's diffuse map samples that texture

#### Scenario: Undecodable referenced image errors
- **WHEN** a material references an image that cannot be decoded
- **THEN** the import fails with an error

### Requirement: glTF feature policy

The importer SHALL ignore morph targets. An asset whose `extensionsRequired`
names an extension the engine does not support SHALL fail to import with an
error; an asset that declares unknown extensions as optional SHALL be imported
with those extensions ignored. Node and scene transforms SHALL NOT be applied
to imported geometry.

#### Scenario: Morph targets ignored
- **WHEN** a mesh declares morph targets
- **THEN** the base geometry imports and the morph targets are ignored

#### Scenario: Unsupported required extension errors
- **WHEN** an asset's `extensionsRequired` names an unsupported extension
- **THEN** the import fails with an error

#### Scenario: Node transform not applied
- **WHEN** the selected mesh is attached to a node with a non-identity
  transform
- **THEN** the imported vertices are as authored, and the transform is the
  script's to apply at draw time

### Requirement: Imported texture lifetime

A `MeshData` returned by the importer SHALL retain the `Texture`s it created
until it is destroyed or consumed by `createMesh`, so that imported materials
keep rendering without the script separately holding every texture.

#### Scenario: Textures survive the import call
- **WHEN** a script imports an asset and immediately calls
  `createMesh(meshData)` without keeping the returned texture references
- **THEN** the imported materials still sample their textures
