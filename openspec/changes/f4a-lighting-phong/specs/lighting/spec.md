# Spec Delta

## Purpose

Defines the F4a fixed-function lighting model: the four point-light slots and
the single directional light, the four-channel Phong material object bound per
mesh surface, the exact lit shading result for meshes, and the CPU reference
that verifies the lighting math.

## ADDED Requirements

### Requirement: Point light bank

`efx.setLight(slot, opts)` SHALL configure one of exactly **four** fixed
point-light slots (vision.md fixed limits). `slot` SHALL be an integer `0..3`;
any other value SHALL throw `RangeError` and change nothing. `opts` SHALL be
`null` (disable the slot) or a bag `{ pos, color, range? }`:

- `pos` — required `[x, y, z]` world-space position;
- `color` — required `[r, g, b, a]` normalized floats (the F2 color
  convention); the alpha component SHALL be ignored by lighting;
- `range` — optional attenuation radius: a finite number `>= 0`, default `0`
  meaning **no attenuation** (the light reaches everywhere at full strength).

A malformed bag (missing or non-array `pos`/`color`, wrong element count,
non-number elements, a negative or non-finite `range`, or an unknown field)
SHALL throw `TypeError` and change nothing. Setting a slot SHALL replace that
slot's previous value. All four slots SHALL start disabled, and lights are a
pre-allocated bank — the only slot-based resource (ADR 0011).

#### Scenario: Set and replace a slot

- **WHEN** `setLight(0, A)` is called and later `setLight(0, B)`
- **THEN** slot 0 holds B's values, slots 1–3 are unchanged, and the color
  alpha has no effect on the rendered light

#### Scenario: Lights are disabled by default and `null` disables

- **WHEN** a script sets a light and later calls `setLight(slot, null)`, or
  never sets any light
- **THEN** the slot contributes nothing to shading

#### Scenario: Invalid slot or malformed bag throws

- **WHEN** `setLight(4, opts)` or `setLight(0, { pos: [0, 0], color: [1, 1, 1, 1] })`
  is called
- **THEN** the call throws `RangeError` / `TypeError` respectively and the
  slot's previous state is unchanged

### Requirement: Directional light

`efx.setDirectionalLight(opts)` SHALL configure the engine's **single**
directional light (vision.md fixed limits). `opts` SHALL be `null` (disable
the light, the startup default) or a bag `{ dir, color }`:

- `dir` — required `[x, y, z]` **direction the light travels**, so the
  direction toward the light used in shading is `normalize(-dir)`;
- `color` — required `[r, g, b, a]` normalized floats; alpha ignored.

A malformed bag (missing or non-array/non-3-element `dir`, zero-length `dir`,
missing/wrong `color`, unknown field) SHALL throw `TypeError` and change
nothing. The directional light contributes no attenuation.

#### Scenario: Directional light illuminates one side

- **WHEN** a lit surface is drawn with `setDirectionalLight({ dir, color })`
  and the same surface is drawn with the light disabled
- **THEN** the lit frame is brighter, with the faces perpendicular to
  `-dir` brightest and faces facing away unlit

#### Scenario: Malformed directional light throws

- **WHEN** `setDirectionalLight({ color: [1, 1, 1, 1] })` or
  `setDirectionalLight({ dir: [0, 0, 0], color: [1, 1, 1, 1] })` is called
- **THEN** the call throws `TypeError` and the previous light state remains

### Requirement: Phong material model

A material is a **JS-managed** plain object (ADR 0011; no native class, no
`destroy()`) with four optional Phong channels. Channel values are
snapshotted by the engine when the material is bound — later mutation of the
script object MUST NOT change the bound material. Omitted channels or fields
take documented defaults:

- `ambient: { color }` — default black `[0, 0, 0, 1]`;
- `diffuse: { color }` — default white `[1, 1, 1, 1]`;
- `specular: { color, shininess? }` — default black `[0, 0, 0, 1]`,
  `shininess` default `32`;
- `emissive: { color }` — default black `[0, 0, 0, 1]`.

Each channel `color` SHALL be a `[r, g, b, a]` array of normalized floats;
the alpha component SHALL be ignored by F4a shading (masking arrives in F4b).
`shininess` SHALL be a finite number `> 0`. A non-object material, an unknown
field, a wrong-typed or wrong-length `color`, or a non-number `shininess`
SHALL throw `TypeError`; a non-positive or non-finite `shininess` SHALL throw
`RangeError`; the call that was passed the material SHALL record nothing.
Maps and `alphaMask` are **not** accepted fields in F4a and throw
`TypeError` (they arrive in F4b).

#### Scenario: Omitted channels take defaults

- **WHEN** `setMeshSurfaceMaterial(mesh, 0, {})` is called
- **THEN** the surface uses the default white-diffuse Phong material

#### Scenario: Specular channel accepts color and shininess

- **WHEN** a material sets `specular: { color: [1, 1, 1, 1], shininess: 64 }`
- **THEN** the surface's specular highlight is sharper than with the default
  shininess of 32

#### Scenario: Material object is snapshotted

- **WHEN** a material is bound, its script object is then mutated, and the
  frame renders
- **THEN** the surface renders with the values at binding time

#### Scenario: Invalid material throws

- **WHEN** a material has a map field, a channel color with three elements,
  or `specular: { shininess: 0 }`
- **THEN** the binding call throws `TypeError` / `RangeError` and the
  surface's previous binding is unchanged

### Requirement: Per-surface material binding

Materials SHALL bind to mesh **surfaces**, never to global engine state
(ADR 0024). `efx.setMeshSurfaceMaterial(mesh, surfaceIndex, mat)` SHALL bind a
snapshot of `mat` to surface `surfaceIndex` of a live `mesh`; `mat` SHALL be
a material object or `null` (bind the engine default material). `mesh` MUST be
a live Mesh (`TypeError` otherwise), `surfaceIndex` an integer in
`0..surfaceCount-1` (`RangeError` otherwise), and `mat` an object or `null`
(`TypeError` otherwise). The call changes only that surface's binding.

`efx.createMeshData(data)` SHALL accept a parallel `materials` array on the
batch form and the single-surface shorthand: `materials[i]` (a material object
or `null` for the default) becomes surface `i`'s initial binding. When present,
`materials` MUST have exactly one entry per surface (wrong length SHALL throw
`RangeError`; invalid entries SHALL throw `TypeError`). The `materials` field
is no longer rejected as unknown (F3 behavior superseded). A surface with no
binding, or bound to `null`, SHALL render with the engine **default material**
— white diffuse Phong with no maps and no emissive — and surface bindings
SHALL carry over unchanged at `createMesh`.

#### Scenario: Bind and rebind a surface

- **WHEN** `setMeshSurfaceMaterial(mesh, 1, A)` is followed by
  `setMeshSurfaceMaterial(mesh, 1, B)`
- **THEN** surface 1 renders with B and the mesh's other surfaces are
  unchanged

#### Scenario: Materials bind at creation

- **WHEN** `createMeshData({ surfaces: [s0, s1], materials: [A, null] })` is
  used to create a mesh
- **THEN** surface 0 renders with material A and surface 1 renders with the
  default material

#### Scenario: Out-of-range index or wrong length throws

- **WHEN** `setMeshSurfaceMaterial(mesh, 2, mat)` is called on a 2-surface
  mesh, or `materials` has fewer/more entries than `surfaces`
- **THEN** the call throws `RangeError` and changes nothing

### Requirement: F4a lit shading

`drawMesh` playback SHALL shade each fragment with the F4a Phong equation,
evaluated per fragment from the surface normal interpolated across the
triangle and normalized. The surface **albedo** SHALL be the surface's vertex
color (default opaque white) multiplied component-wise by the `drawMesh` tint
(default opaque white). With `N` the unit world normal, `V` the unit direction
to the camera, and each enabled light contributing:

- **ambient** = `material.ambient.color × albedo` (flat, independent of
  lights);
- **diffuse** = `material.diffuse.color × albedo × Σ (lightColor.rgb × max(N·L, 0) × atten)`;
- **specular** = `material.specular.color × Σ (lightColor.rgb × max(N·H, 0)^shininess × atten)`
  where `H = normalize(L + V)` (Blinn-Phong);
- **emissive** = `material.emissive.color` (added directly, **not** modulated
  by the albedo).

The final fragment color SHALL be `ambient + diffuse + specular + emissive`
with the surface alpha equal to the albedo alpha, clamped to `[0, 1]` per
channel; there is no HDR or tonemapping. For a point light, `L` is the unit
direction from the fragment to the light's world position, `d` is the
distance, and attenuation is `atten = 1` when `range` is `0`, else
`atten = clamp(1 - d / range, 0, 1)`. For the directional light, `L` is the
unit direction toward the light (`normalize(-dir)`) and `atten = 1`. A surface
without normals SHALL use the F3 default normal `(0, 0, 1)` in object space.
`uvs` SHALL NOT affect F4a shading. 2D quad records SHALL be unaffected and
the committed F2 goldens SHALL remain valid. With every light disabled, a
default-material surface renders black except for its emissive term.

#### Scenario: Ambient alone is flat

- **WHEN** a surface with `ambient: { color: [0.5, 0.5, 0.5, 1] }` is drawn
  with every light disabled
- **THEN** every fragment is a flat half-grey scaled by the albedo

#### Scenario: Diffuse depends on the normal

- **WHEN** a cube is lit by one point light and drawn
- **THEN** the face most facing the light is brightest and faces facing away
  are unlit

#### Scenario: Specular highlight follows the camera

- **WHEN** a sphere with a specular material is lit by a point light and the
  camera moves so the reflection direction aligns with a surface point
- **THEN** a highlight appears on that surface point

#### Scenario: Point light attenuation falls to zero at range

- **WHEN** equal-distance fragments are lit by a point light with a finite
  `range`
- **THEN** a fragment at distance `>= range` receives no light from it and a
  fragment at distance `0` receives full strength

#### Scenario: Emissive is not modulated

- **WHEN** a surface with an emissive color and a black albedo is drawn with a
  tint
- **THEN** the emissive contribution is unchanged by the albedo and tint

#### Scenario: Default material with no lights is black

- **WHEN** an unbound surface is drawn with all lights disabled
- **THEN** it renders black (no ambient, diffuse, specular, or emissive term)

### Requirement: Lighting CPU reference

The repository SHALL contain a CPU implementation of the F4a Phong equation
independent of the GPU path, and unit tests SHALL assert its values for
analytic configurations — ambient-only, a surface perpendicular to a light
(diffuse maximum), the specular peak, attenuation at exactly `range`,
directional-only lighting, and emissive-only. These tests SHALL run headless
on all four targets as part of the standard suite, and at least one committed
golden scene SHALL confirm the rendered GPU result agrees with the reference.

#### Scenario: CPU reference is analytically correct

- **WHEN** the CPU reference shades a surface with a normal directly facing a
  unit-color light and a white diffuse material
- **THEN** the diffuse term equals the light contribution and a surface
  facing away yields zero diffuse

#### Scenario: GPU agrees with the reference

- **WHEN** a golden scene renders a surface under a known light and material
  configuration
- **THEN** the captured pixels match the value computed by the CPU reference
  within the golden tolerance
