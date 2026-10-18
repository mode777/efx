# Spec Delta

## MODIFIED Requirements

### Requirement: PBR-to-Phong material conversion

Each imported primitive's glTF material SHALL be converted to the engine's
Phong material and bound to the corresponding surface. The conversion SHALL
map base color to diffuse, emissive to emissive, metallic to the specular
color, and roughness to shininess. The glTF material's `alphaMode` SHALL map
onto the engine material's `alphaMode`: `OPAQUE` and absent map to `'opaque'`,
`MASK` maps to `'mask'` and produces an alpha mask, and `BLEND` maps to
`'blend'`. A converted `'blend'` material SHALL carry the base color's alpha
factor in the diffuse channel alpha, so the imported surface composites in the
transparent pass; `'opaque'` and `'mask'` conversions ignore the base color
alpha, matching the engine's output-alpha rule. The fixed `0.5` discard
threshold stands: a glTF `alphaCutoff` other than `0.5` is approximated by
`0.5` (documented limitation). Occlusion and normal textures SHALL be ignored.
A primitive with no material SHALL use the engine default material.

#### Scenario: Base color and emissive
- **WHEN** a glTF material defines base color and emissive factors or textures
- **THEN** the bound Phong material's diffuse and emissive channels carry them

#### Scenario: Metallic and roughness
- **WHEN** a glTF material defines metallic and roughness factors
- **THEN** the bound Phong material's specular color and shininess are derived
  from them respectively

#### Scenario: Alpha-mask material
- **WHEN** a glTF material uses `alphaMode: MASK`
- **THEN** the bound material carries an alpha mask and `alphaMode: 'mask'`,
  discarding fragments below the mask threshold on the full rendering path

#### Scenario: Alpha-blend material
- **WHEN** a glTF material uses `alphaMode: BLEND` with a base color alpha
  below `1`
- **THEN** the bound material maps to `alphaMode: 'blend'`, the base color
  alpha lands in the diffuse channel alpha, and the surface composites in the
  transparent pass with depth writing suppressed

#### Scenario: Unlit or absent material
- **WHEN** a primitive has no glTF material
- **THEN** its surface renders with the engine default material
