# 3d-core

## Purpose

Defines the 3D core behavior delivered by milestone F3: the perspective
camera and its separation from the 2D projection frame, multi-surface mesh
data resources (Godot-style surfaces), GPU mesh upload and lifecycle,
whole-mesh depth-tested drawing, procedural primitives with pinned defaults,
and the engine-bundled pure-JS math layer.

## Requirements

### Requirement: 3D camera
`efx.graphics.setCamera3D(opts)` SHALL configure the engine's single 3D camera from an
option object `{ pos, target, fov, near?, far? }`: `pos` and `target` are
`[x, y, z]` world points (the eye position and the looked-at point), `fov` is
the **vertical** field of view in **degrees**, `near` and `far` are the depth
range in world units with defaults 0.1 and 100. The up vector SHALL be
`[0, 1, 0]`. The 3D camera SHALL be set, never created, and there SHALL be
exactly one (vision.md fixed limits). The 3D camera SHALL be a projection
state separate from the F2 2D projection frame: `setCamera3D` SHALL affect
only 3D draws (mesh records), while 2D draws (`drawQuad`, including draws
whose texture argument is a RenderTarget) continue to render under the 2D
camera state established by the most recent `setCamera2D` (or the F2 default
when never called), leaving F2 behavior unchanged. The projection aspect
SHALL derive from the active rendering surface's extent — the window, or the
active RenderTarget while a begin/end pair is recording. Like all recorded
state (ADR 0019), the camera SHALL be
value-snapshotted at record time: a mesh draw records the 3D camera state in
effect when the draw is recorded and MUST NOT observe later camera changes.
Calling `setCamera3D` with a malformed bag (missing or non-array `pos`/
`target`, non-number `fov`/`near`/`far`, unknown fields) SHALL throw
`TypeError` and change nothing.

#### Scenario: Perspective projection is observable
- **WHEN** two equal meshes are drawn at different distances from the camera eye along its view axis with the same `drawMesh` parameters
- **THEN** the nearer mesh appears larger in the frame, and a wider `fov` renders a visibly larger field of the scene

#### Scenario: 2D draws are unaffected by the 3D camera
- **WHEN** `setCamera3D` is active and a `drawQuad` is recorded, including one sampling a RenderTarget
- **THEN** the quad renders under the most recent `setCamera2D` state (or the F2 default camera when `setCamera2D` was never called), and the committed F2 golden output is unchanged

#### Scenario: Aspect follows the rendering surface
- **WHEN** the same 3D scene is recorded to the window and into a RenderTarget whose extent has a different aspect ratio, and both are shown
- **THEN** each render uses its own surface's aspect — neither is stretched relative to its own surface

#### Scenario: Camera is value-snapshotted per record
- **WHEN** a mesh is drawn, then `setCamera3D` moves the camera, all in one render hook
- **THEN** playback renders that mesh with the camera state at its record time

#### Scenario: Defaults for near and far
- **WHEN** `setCamera3D({ pos, target, fov })` is called without `near`/`far`
- **THEN** the depth range is 0.1 to 100 and the draw succeeds

#### Scenario: Malformed camera options throw
- **WHEN** `setCamera3D` is called with a missing `pos`, a non-number `fov`, or an unknown field
- **THEN** the call throws `TypeError` and the previously set camera state remains in effect

### Requirement: Multi-surface mesh data

`efx.graphics.createMeshData(data)` SHALL build a CPU-side MeshData (native-backed
class, ADR 0011/0013) holding 1..16 **surfaces**. Two construction forms
SHALL be accepted: a batch bag `{ surfaces: [surface, ...] }`, or a
single-surface shorthand `{ positions, normals?, uvs?, colors?, indices? }`
(equivalent to a one-element `surfaces` array). Passing both `surfaces` and
`positions` SHALL throw `TypeError`; passing neither SHALL throw `TypeError`.
A **surface** is one Godot-style surface / glTF primitive — a set of
attribute arrays plus optional indices:

- `positions` — required, a flat array (or typed array) of finite numbers,
  length a multiple of 3 (xyz per vertex) and greater than 0;
- `normals?` — flat xyz array with exactly the same vertex count as
  `positions`;
- `uvs?` — flat uv array with exactly the same vertex count as `positions`;
- `colors?` — flat rgba array (normalized floats, the F2 color convention)
  with exactly the same vertex count as `positions`;
- `joints?` — flat array of four joint indices per vertex, with exactly the
  same vertex count as `positions` (glTF `JOINTS_0`, four influences);
  requires `weights`;
- `weights?` — flat array of four joint weights per vertex, with exactly the
  same vertex count as `positions` (glTF `WEIGHTS_0`, four influences);
  requires `joints`;
- `indices?` — flat array of non-negative integers: a triangle list whose
  length is a multiple of 3 and whose every value is less than the surface's
  vertex count.

When `indices` is omitted, the vertex count MUST be a multiple of 3
(non-indexed triangle list). A wrong attribute count, a non-multiple-of-3
`positions`/`indices` length, an out-of-range index, a mismatched or unpaired
`joints`/`weights` attribute, an empty `surfaces`
array, or a surface count above 16 SHALL throw `RangeError`; wrong element
types SHALL throw `TypeError`; unknown fields SHALL throw `TypeError`. The
fixed limit is **16 surfaces per mesh** (vision.md fixed limits, recorded in
the reference). Each surface carries an optional material binding; from F4a
the data bag accepts a parallel `materials` array (one entry per surface — a
material object or `null` for the engine default; see the `lighting`
capability), bound to its surface at creation time and carried over at
`createMesh`. MeshData SHALL expose the read-only query property
`surfaceCount` (the number of surfaces; throws `TypeError` when destroyed).
`destroy()` releases the native storage deterministically and is idempotent;
using a destroyed MeshData SHALL throw.

#### Scenario: Batch construction creates multiple surfaces

- **WHEN** `createMeshData({ surfaces: [s0, s1] })` is called with two valid
  surfaces
- **THEN** the returned MeshData's `surfaceCount` is 2 and each surface
  retains its own attribute arrays and indices

#### Scenario: Single-surface shorthand

- **WHEN** `createMeshData({ positions, colors })` is called with valid
  arrays
- **THEN** the result is identical to a one-element `surfaces` array
  containing that surface, and `surfaceCount` is 1

#### Scenario: Validation errors

- **WHEN** a surface's `normals` length does not match its vertex count, an
  `indices` value equals the vertex count, an empty `surfaces` array is
  passed, or a 17th surface is passed
- **THEN** `createMeshData` throws `RangeError` and records nothing

#### Scenario: Unknown fields and types throw

- **WHEN** the data bag or a surface contains an unknown field, or an
  attribute array holds a non-number
- **THEN** the call throws `TypeError`

#### Scenario: Materials are an F4 concept

- **WHEN** `createMeshData` is called with a parallel `materials` array
- **THEN** `materials[i]` binds material `i` (or the engine default when
  `null`) to surface `i` at creation time, and a `materials` array whose
  length does not match the surface count throws `RangeError`

#### Scenario: Skinned surface attributes

- **WHEN** a surface supplies `joints` and `weights` arrays with exactly the
  vertex count of its `positions`
- **THEN** the MeshData retains those attributes and `createMesh` carries
  them onto the mesh; supplying only one of the pair or a mismatched count
  throws `RangeError`

#### Scenario: Query property and destroy

- **WHEN** a script reads `surfaceCount` on a MeshData, destroys it, and
  reads `surfaceCount` again
- **THEN** the first read returns the surface number and the second throws
  `TypeError`

### Requirement: Mesh upload and lifecycle
`efx.graphics.createMesh(meshData)` SHALL upload a live MeshData's **every** surface
CPU → GPU into one Mesh (native-backed class, ADR 0011/0013: deterministic
`destroy()`, idempotent, GC-finalizer backstop, display-list references keep
it alive until playback completes). The Mesh SHALL expose the read-only
query property `surfaceCount` (equal to the MeshData's at upload time;
throws `TypeError` when destroyed). A Mesh is a copy: later changes to the
source MeshData object MUST NOT affect the Mesh. Passing a non-MeshData or a
destroyed MeshData SHALL throw `TypeError`. Surface material bindings
carried by the MeshData (from F4) SHALL carry over to the Mesh at upload;
from F4, `efx.graphics.setMeshSurfaceMaterial(mesh, surfaceIndex, mat)` SHALL rebind
one surface's material after upload (the Godot `surface_set_material`
analog: `mat` is a JS-managed object snapshotted at call time; an index out
of range throws `RangeError`); a surface without a bound material SHALL
render with an engine default material. Meshes are never slot-based; scripts
manage Mesh objects directly (ADR 0011 resource model — the roadmap's
"mesh slots" phrase is realized as resource objects, per the pinned js-api
taxonomy).

#### Scenario: All surfaces upload
- **WHEN** `createMesh` is called with a 3-surface MeshData
- **THEN** the Mesh's `surfaceCount` is 3 and drawing it renders all three
  surfaces

#### Scenario: Upload copies
- **WHEN** a Mesh is created from a MeshData and the MeshData is then
  destroyed
- **THEN** the Mesh keeps rendering identically (destroyed-use rules apply
  only to the destroyed object)

#### Scenario: Destroyed mesh is safe
- **WHEN** `destroy()` is called on a Mesh twice and a `drawMesh` references
  it after the first call
- **THEN** the second `destroy()` is a no-op and the draw throws `TypeError`

### Requirement: Whole-mesh drawing with depth

`efx.graphics.drawMesh(mesh, opts?)` SHALL record one draw for the whole mesh, with
`mesh` as a required first positional argument that MUST be a live Mesh
(nothing, a non-Mesh, or a destroyed Mesh throws `TypeError`). `opts?` is an
optional option bag restricted to `{ transform?, color?, skinned? }`: `transform?`
is a flat array (or typed array) of exactly 16 finite numbers — a column-major
4×4 matrix, default identity (a wrong length throws `RangeError`, non-number
elements throw `TypeError`); `color?` is a `[r, g, b, a]` tint, default
opaque white; `skinned?` is a boolean, default `false` — `false`/absent draws
the mesh's bind-pose vertices and `true` draws the current CPU-posed vertices,
and `true` on a mesh without a rig throws `TypeError`. The option bag, when
present, MUST be an object; unknown option fields (including `mesh`, which is no
longer an option) SHALL throw `TypeError`. Playback SHALL draw every surface in
surface order under the recorded camera, with the depth test enabled and depth
writing on: a nearer surface occludes a farther one regardless of record order,
and equal-depth fragments resolve by record order (deterministic). Per-surface
shading SHALL be the `lighting` capability's F4 lit result — the F4a Phong
equation **including the F4b per-channel maps and alpha mask** — with albedo
equal to the tint multiplied by the surface's vertex color where the `colors`
attribute is present, or the tint alone otherwise. The surface `uvs`
attribute (validated and stored since F3) SHALL be consumed by that shading
as the map/alpha-mask texture coordinate; a surface without `uvs` uses the
`(0, 0)` default, so shading stays defined for every mesh. 2D quad records
SHALL be untouched by mesh depth (F2 behavior and goldens unchanged). Mesh
draws participate in the per-frame record budget like any record.

#### Scenario: Multi-surface mesh draws all surfaces

- **WHEN** a 2-surface mesh is drawn where the surfaces occupy different
  screen areas
- **THEN** both surfaces appear, in surface order

#### Scenario: Depth test occludes by distance

- **WHEN** a nearer mesh is recorded before a farther mesh that occupies the
  same screen area
- **THEN** the nearer mesh's fragments win the depth test despite the later
  record

#### Scenario: Vertex colors multiply the tint

- **WHEN** a surface with per-vertex colors is drawn with
  `color: [r, g, b, a]`
- **THEN** each fragment's albedo is the vertex color multiplied
  component-wise by the tint (the tint alone where the surface has no vertex
  colors), and that albedo is what the lighting equation shades

#### Scenario: Transform positions the mesh

- **WHEN** the same mesh is drawn twice with different `transform` matrices
  (e.g. translation versus identity)
- **THEN** the two renders appear at different world positions per the
  recorded matrices

#### Scenario: Surface uvs drive bound maps

- **WHEN** two surfaces of the same mesh have different `uvs` and a bound
  `diffuse.map`
- **THEN** each surface samples the map at its own interpolated `uv`, and a
  surface with no `uvs` samples `(0, 0)`

#### Scenario: Mesh is a required positional argument

- **WHEN** `drawMesh` is called without a mesh (no argument), with a non-Mesh
  first argument, or with a destroyed Mesh
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Option bag holds only transform and color

- **WHEN** `drawMesh(mesh, { transform, color })` is called with a valid bag,
  or `drawMesh(mesh, { mesh })` / `drawMesh(mesh, { frobnicate: 1 })` is
  called with an unknown field
- **THEN** the valid bag records the draw (transform/color applied; the
  documented `skinned` option is also accepted), and the unknown field throws
  `TypeError` and records nothing

#### Scenario: Skinned draw on a static mesh rejected

- **WHEN** `drawMesh(mesh, { skinned: true })` is called on a mesh with no
  imported rig
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Validation errors

- **WHEN** `drawMesh(mesh, { transform })` is called with a 15-element
  `transform`
- **THEN** the call throws `RangeError` and records nothing

### Requirement: Procedural primitives

The engine-bundled pure-JS primitives `efx.graphics.makeCube(opts?)`,
`efx.graphics.makePlane(opts?)`, and `efx.graphics.makeSphere(opts?)` SHALL each return a
single-surface MeshData (directly usable by `createMesh`), with pinned
defaults so scenes are reproducible:

- `makeCube({ size? = 1, material? })` — an axis-aligned cube centered on the
  origin with side length `size`, outward per-face normals, outward-facing
  (CCW) winding, and per-face uv mapping of the full 0..1 square;
- `makePlane({ size? = 1, segments? = 1, material? })` — a plane in the XZ
  plane facing `+Y`, centered on the origin with side length `size`,
  subdivided into `segments × segments` quads, uv coordinates spanning 0..1;
- `makeSphere({ radius? = 1, segments? = 16, material? })` — a UV sphere
  centered on the origin with radius `radius` and `segments` latitude and
  longitude bands, normals equal to normalized positions, equirectangular uv
  spanning 0..1.

A non-finite or non-positive `size`/`radius` SHALL throw `RangeError`;
`segments` SHALL be a positive integer (`RangeError` otherwise); unknown
fields SHALL throw `TypeError`. Missing option objects take all defaults.

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

#### Scenario: Invalid parameters throw
- **WHEN** `makeCube({ size: 0 })` or `makePlane({ segments: 1.5 })` is
  called
- **THEN** the call throws `RangeError`

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

### Requirement: Script math layer
The engine SHALL bundle pure-JS math helpers on the `efx` object — `efx.mat4`
(`identity`, `perspective(fovY, aspect, near, far)`, `ortho(width, height,
near, far)`, `translate(m, v)`, `rotate(m, deg, axis)`, `scale(m, v)`,
`multiply(a, b)`), `efx.vec3` (`add`, `sub`, `scale(v, s)`, `normalize`,
`cross`, `dot`), and `efx.quat` (`identity`, `fromAxisAngle(deg, axis)`,
`multiply(a, b)`, `toMat4(q)`) — implemented entirely in the `[JS]` layer on
standard ES6 (zero browser/Node dependencies, per the two-layer rule). All
helpers SHALL be pure functions that never mutate their arguments and return
plain JS data: matrices are flat 16-number column-major arrays, vectors are
3-number arrays, angles are **degrees** (ADR 0010: script math is plain JS
data; the API never uses radians). Matrix composition SHALL follow the
column-major convention `multiply(a, b)` computes `a·b` (b applies to the
vector first), and `rotate(m, deg, axis)` computes `m·R(deg, axis)` — a
right-handed rotation, counter-clockwise about `axis` looking down the axis
toward the origin, matching the F2 degree convention's 3D counterpart. The
helpers MUST produce values consistent with the engine's own camera math so
script-built transforms and `setCamera3D` compose predictably.

#### Scenario: Perspective matrix is correct
- **WHEN** a math unit test computes `efx.mat4.perspective(60, 4/3, 0.1, 100)`
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
- **WHEN** `efx.mat4.rotate(identity, 90, [0, 1, 0])` is applied to
  `[1, 0, 0]` (w = 1)
- **THEN** the result is approximately `[0, 0, -1]` — a quarter turn taken
  as 90 degrees, not radians
