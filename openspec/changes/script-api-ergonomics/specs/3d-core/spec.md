# Spec Delta: 3d-core

## MODIFIED Requirements

### Requirement: Script math layer
The engine SHALL bundle math helpers on the `efx` object — `efx.math.mat4`
(`identity`, `perspective(fovY, aspect, near, far)`, `ortho(width, height,
near, far)`, `translate(m, v)`, `rotate(m, deg, axis)`, `scale(m, v)`,
`multiply(a, b)`, `transform(pos, yawDeg?, pitchDeg?, rollDeg?, scale?)`),
`efx.math.vec3` (`add`, `sub`, `scale(v, s)`, `normalize`, `cross`, `dot`),
`efx.math.quat` (`identity`, `fromAxisAngle(deg, axis)`, `multiply(a, b)`,
`toMat4(q)`), and the scalar helpers `efx.math.clamp(v, lo, hi)` and
`efx.math.lerp(a, b, t)`. The scalar helpers and every pre-existing member
SHALL be implemented entirely in the `[JS]` layer on standard ES6 (zero
browser/Node dependencies, per the two-layer rule); `mat4.transform` SHALL be
native-backed through the standard marshal-only wire with its option
validation in the shared prelude (ADR 0049). Every helper SHALL be pure from
the script's perspective: none SHALL mutate its arguments, and each SHALL
return plain JS data — matrices are flat 16-number column-major arrays,
vectors are 3-number arrays, angles are **degrees** (ADR 0010: script math is
plain JS data; the API never uses radians). `mat4.transform` SHALL compose
`T(pos) · Ry(yaw) · Rx(pitch) · Rz(roll) · S(scale)` — yaw about `+Y`, pitch
about `+X`, roll about `+Z`, applied to the vector in that order — with
`yawDeg`/`pitchDeg`/`rollDeg` defaulting to `0` and `scale` accepting a
positive number (uniform) or a `[x, y, z]` array of positive numbers
(default `1`); a non-finite angle or position component SHALL throw
`TypeError`/`RangeError` as for the other composition helpers. The three
sub-namespaces SHALL live in the `efx.math` sub-namespace and SHALL NOT also
exist as root members of `efx` (hard cut, no aliases). Matrix composition
SHALL follow the column-major convention `multiply(a, b)` computes `a·b` (b
applies to the vector first), and `rotate(m, deg, axis)` computes
`m·R(deg, axis)` — a right-handed rotation, counter-clockwise about `axis`
looking down the axis toward the origin, matching the F2 degree convention's
3D counterpart. The helpers MUST produce values consistent with the engine's
own camera math so script-built transforms and `setCamera3D` compose
predictably; `mat4.transform` SHALL produce byte-identical results on the
desktop and web runtimes.

#### Scenario: Perspective matrix is correct
- **WHEN** a math unit test computes `efx.math.mat4.perspective(60, 4/3, 0.1, 100)`
  and checks selected entries against the expected perspective values
- **THEN** the entries match (degrees-to-tan conversion, aspect on the x
  axis, near/far depth mapping)

#### Scenario: Multiplication order
- **WHEN** `multiply(translate(m, t), rotate(m2, deg, axis))` transforms a
  point
- **THEN** the rotation applies to the point first, then the translation —
  `v' = T · R · v`

#### Scenario: Pure functions
- **WHEN** a helper such as `translate(m, v)` is called
- **THEN** the input matrix `m` is unchanged and the result is a new plain
  array

#### Scenario: Degrees everywhere
- **WHEN** `efx.math.mat4.rotate(identity, 90, [0, 1, 0])` is applied to
  `[1, 0, 0]` (w = 1)
- **THEN** the result is approximately `[0, 0, -1]` — a quarter turn taken
  as 90 degrees, not radians

#### Scenario: No root math aliases
- **WHEN** the math helpers are read after the move
- **THEN** `efx.math.mat4`, `efx.math.vec3`, and `efx.math.quat` expose the
  same functions with the same behavior as the former root helpers, and
  `efx.mat4`, `efx.vec3`, and `efx.quat` no longer exist

#### Scenario: Transform composes in fixed order
- **WHEN** `mat4.transform([0, 0, 1], 90)` is applied to the local origin
- **THEN** the result translates by `[0, 0, 1]`, and with the yaw applied
  before pitch before roll a point transformed by the matrix matches the
  hand-composed `multiply(translate(identity, pos), multiply(rotate(identity,
  yaw, [0,1,0]), multiply(rotate(identity, pitch, [1,0,0]),
  multiply(rotate(identity, roll, [0,0,1]), scale(identity, s)))))`

#### Scenario: Transform validates like the other composition helpers
- **WHEN** `mat4.transform` is called with a non-array position, a
  non-finite angle, a non-positive scale, or an unknown-shaped scale array
- **THEN** the call throws (`TypeError` for wrong types, `RangeError` for
  out-of-range numbers) on both runtimes with identical messages

#### Scenario: Scalar helpers
- **WHEN** `efx.math.clamp(5, 0, 3)` and `efx.math.lerp(0, 10, 0.25)` are
  called
- **THEN** the results are `3` and `2.5`, arguments are never mutated, and
  `clamp`/`lerp` exist only under `efx.math`

### Requirement: Procedural primitives

The engine-bundled pure-JS primitives `efx.graphics.makeCube(opts?)`,
`efx.graphics.makePlane(opts?)`, and `efx.graphics.makeSphere(opts?)` SHALL each return a
single-surface MeshData (directly usable by `createMesh`), with pinned
defaults so scenes are reproducible:

- `makeCube({ size? = 1, inverted? = false, material? })` — an axis-aligned
  cube centered on the origin with side length `size`, per-face normals,
  outward-facing (CCW) winding, and per-face uv mapping of the full 0..1
  square; when `inverted` is `true` the per-face normals point inward and the
  winding is reversed so the inside of the cube is front-facing;
- `makePlane({ size? = 1, segments? = 1, uvs? = 1, material? })` — a plane in
  the XZ plane facing `+Y`, centered on the origin with side length `size`,
  subdivided into `segments × segments` quads, uv coordinates spanning
  0..1 by default; when `uvs` is a positive number `u` the uv coordinates
  span 0..u in both axes (tiling), and when `uvs` is a `[u, v]` pair of
  positive numbers they span 0..u and 0..v respectively;
- `makeSphere({ radius? = 1, segments? = 16, inverted? = false, material? })` —
  a UV sphere centered on the origin with radius `radius` and `segments`
  latitude and longitude bands, normals equal to normalized positions,
  equirectangular uv spanning 0..1; when `inverted` is `true` the normals
  point inward and the winding is reversed so the inside of the sphere is
  front-facing.

A non-finite or non-positive `size`/`radius` SHALL throw `RangeError`;
`segments` SHALL be a positive integer (`RangeError` otherwise); a
non-positive or non-finite `uvs` value (either component of the array form)
SHALL throw `RangeError`; a non-boolean
`inverted` SHALL throw `TypeError`; unknown fields SHALL throw `TypeError`.
Missing option objects take all defaults.

When `material` is present it SHALL be bound to the primitive's single
surface at MeshData creation, exactly as `createMeshData`'s parallel
`materials` array binds a material to its surface: the material is a
JS-managed object snapshotted at call time, the binding carries onto the
Mesh at `createMesh`, and an omitted or `null` `material` selects the engine
default material. Material validation SHALL reuse the existing binding
rules (unknown material/channel fields and non-`null`/`undefined` values
that are not material objects throw `TypeError`).

#### Scenario: Defaults produce a usable mesh
- **WHEN** `efx.graphics.makeCube()` is called with no arguments
- **THEN** the result is a single-surface MeshData with `surfaceCount` 1
  that uploads and draws like any other MeshData

#### Scenario: Parameter overrides
- **WHEN** `makeSphere({ radius: 2, segments: 24 })` is called
- **THEN** the surface's positions lie on a radius-2 sphere and the vertex
  count follows the 24-band layout deterministically

#### Scenario: Inverted primitive faces inward
- **WHEN** `makeCube({ inverted: true })` or `makeSphere({ inverted: true })`
  is created and drawn with the camera inside it
- **THEN** the inside surface renders (the winding is reversed and the normals
  point inward), where the non-inverted primitive would be back-face culled

#### Scenario: Invalid parameters throw
- **WHEN** `makeCube({ size: 0 })`, `makePlane({ segments: 1.5 })`, or
  `makeCube({ inverted: 1 })` is called
- **THEN** the call throws (`RangeError` for the numeric bounds,
  `TypeError` for the non-boolean)

#### Scenario: Primitive binds its material at creation
- **WHEN** `makeCube({ size: 1, material })` is called and the result is
  passed to `createMesh`
- **THEN** the primitive's single surface carries `material` (snapshotted at
  call time) and the Mesh renders it, while an omitted or `null` `material`
  renders with the engine default material

#### Scenario: Invalid material throws
- **WHEN** `makePlane({ material: 42 })` is called with a value that is not
  a material object
- **THEN** the call throws `TypeError`

#### Scenario: Plane uvs tile the unit square
- **WHEN** `makePlane({ uvs: 6 })` (or `makePlane({ uvs: [6, 3] })`) is
  created and drawn with a tiled texture
- **THEN** the uv coordinates span 0..6 in both axes (0..6 and 0..3 for the
  array form), repeating the texture 6×6 (6×3) across the plane, and the
  default call still spans 0..1

## ADDED Requirements

### Requirement: Screen-to-world ray
`efx.graphics.screenRay(x, y)` SHALL return a plain JS object
`{ origin, direction }` describing the world-space ray through the given
screen point under the current 3D camera state: `origin` is the camera eye
position and `direction` is the normalized direction through the point, as
if the point were placed on the near plane. `x` and `y` SHALL be interpreted
in the same coordinate space mouse input reports — frame pixels under the
active 2D frame (scale-only mapping), falling back to surface pixels when no
2D frame was ever set — so `screenRay(...efx.mouse.position)` is the
canonical picking call. The ray SHALL be a pure query over the live camera
state — the most recently set 3D camera applies, not any per-record
snapshot — and it SHALL record nothing into the display list. `screenRay`
SHALL be
C-implemented and SHALL have identical names, signatures, semantics, and
error behavior across the desktop and web bindings. Calling `screenRay`
before any `setCamera3D` SHALL throw `Error` (no 3D camera); a non-finite or
wrongly-typed `x`/`y` SHALL throw `TypeError`/`RangeError` per the engine's
argument convention; the returned object SHALL be plain data (fresh arrays,
no aliases into engine state).

#### Scenario: Ray through a frame point matches drawn geometry
- **WHEN** a 3D camera looks at a known world point and a script computes the
  ray through the frame position where that point projects (the frame
  center, when the camera targets it)
- **THEN** the ray's direction passes through the world point (the point lies
  on the ray's line within a small tolerance)

#### Scenario: Frame-space default composes with mouse input
- **WHEN** a 2D frame `[640, 360]` is active and a script calls
  `screenRay(...efx.mouse.position)`
- **THEN** the ray is computed for the same screen point the pointer occupies,
  and `screenRay(320, 180)` with a centered camera yields the camera's
  forward direction

#### Scenario: Origin and normalized direction
- **WHEN** `screenRay(x, y)` is called for any valid point
- **THEN** `origin` equals the current 3D camera eye position and
  `direction` has unit length, and mutating the returned arrays does not
  affect the engine

#### Scenario: Errors
- **WHEN** `screenRay` is called before any `setCamera3D`, or with a
  non-number or non-finite coordinate
- **THEN** the call throws (`Error` for the missing camera,
  `TypeError`/`RangeError` for the bad coordinate), identically on both
  runtimes
