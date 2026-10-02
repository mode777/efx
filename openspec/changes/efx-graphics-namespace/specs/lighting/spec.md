# Spec Delta

## MODIFIED Requirements

### Requirement: Point light bank

`efx.graphics.setLight(slot, opts)` SHALL configure one of exactly **four** fixed
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

`efx.graphics.setDirectionalLight(opts)` SHALL configure the engine's **single**
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

### Requirement: Per-surface material binding

Materials SHALL bind to mesh **surfaces**, never to global engine state
(ADR 0024). `efx.graphics.setMeshSurfaceMaterial(mesh, surfaceIndex, mat)` SHALL bind a
snapshot of `mat` to surface `surfaceIndex` of a live `mesh`; `mat` SHALL be
a material object or `null` (bind the engine default material). `mesh` MUST be
a live Mesh (`TypeError` otherwise), `surfaceIndex` an integer in
`0..surfaceCount-1` (`RangeError` otherwise), and `mat` an object or `null`
(`TypeError` otherwise). The call changes only that surface's binding.

`efx.graphics.createMeshData(data)` SHALL accept a parallel `materials` array on the
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
