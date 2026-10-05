# Spec Delta

## MODIFIED Requirements

### Requirement: 3D camera
`efx.graphics.setCamera3D(pos, target, fov, opts?)` SHALL configure the engine's single 3D camera. `pos` and `target` are
`[x, y, z]` world points (the eye position and the looked-at point) and `fov` is
the **vertical** field of view in **degrees**; the optional trailing `opts`
bag carries `near` and `far`, the depth
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
Calling `setCamera3D` with a missing or non-array `pos`/`target`, a
non-number `fov`/`near`/`far`, or an unknown bag field SHALL throw
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
- **WHEN** `setCamera3D(pos, target, fov)` is called without a trailing bag (or with `near`/`far` omitted)
- **THEN** the depth range is 0.1 to 100 and the draw succeeds

#### Scenario: Malformed camera options throw
- **WHEN** `setCamera3D` is called with a missing `pos`, a non-number `fov`, or an unknown bag field
- **THEN** the call throws `TypeError` and the previously set camera state remains in effect

### Requirement: Multi-surface mesh data

`efx.graphics.createMeshData(surfaces, materials?)` SHALL build a CPU-side MeshData (native-backed
class, ADR 0011/0013) holding 1..16 **surfaces**. The first positional
argument SHALL be the surface list (`surfaces`), and the optional second
positional argument SHALL be the parallel `materials` array (one entry per
surface — a material object or `null` for the engine default; see the
`lighting` capability). There SHALL be no single-surface shorthand form: a
single-surface mesh passes a one-element surface list. A **surface** is one
Godot-style surface / glTF primitive — a set of
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
the reference). When present, `materials` MUST have exactly one entry per
surface (wrong length SHALL throw `RangeError`; invalid entries SHALL throw
`TypeError`); it is bound to its surface at creation time and carried over at
`createMesh`. MeshData SHALL expose the read-only query property
`surfaceCount` (the number of surfaces; throws `TypeError` when destroyed).
`destroy()` releases the native storage deterministically and is idempotent;
using a destroyed MeshData SHALL throw.

#### Scenario: Batch construction creates multiple surfaces

- **WHEN** `createMeshData([s0, s1])` is called with two valid
  surfaces
- **THEN** the returned MeshData's `surfaceCount` is 2 and each surface
  retains its own attribute arrays and indices

#### Scenario: Single-surface shorthand

- **WHEN** `createMeshData([{ positions, colors }])` is called with valid
  arrays (a one-element surface list)
- **THEN** the result has one surface and `surfaceCount` is 1, and the former
  bare surface-bag shorthand form is rejected with `TypeError`

#### Scenario: Validation errors

- **WHEN** a surface's `normals` length does not match its vertex count, an
  `indices` value equals the vertex count, an empty `surfaces` array is
  passed, or a 17th surface is passed
- **THEN** `createMeshData` throws `RangeError` and records nothing

#### Scenario: Unknown fields and types throw

- **WHEN** a surface contains an unknown field, or an
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
