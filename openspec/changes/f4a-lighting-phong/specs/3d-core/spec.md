# Spec Delta

## MODIFIED Requirements

### Requirement: Multi-surface mesh data

`efx.createMeshData(data)` SHALL build a CPU-side MeshData (native-backed
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
- `indices?` — flat array of non-negative integers: a triangle list whose
  length is a multiple of 3 and whose every value is less than the surface's
  vertex count.

When `indices` is omitted, the vertex count MUST be a multiple of 3
(non-indexed triangle list). A wrong attribute count, a non-multiple-of-3
`positions`/`indices` length, an out-of-range index, an empty `surfaces`
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

#### Scenario: Query property and destroy

- **WHEN** a script reads `surfaceCount` on a MeshData, destroys it, and
  reads `surfaceCount` again
- **THEN** the first read returns the surface number and the second throws
  `TypeError`

### Requirement: Whole-mesh drawing with depth

`efx.drawMesh(opts)` SHALL record one draw for the whole mesh from the bag
`{ mesh, transform?, color? }`: `mesh` is required and MUST be a live Mesh
(nothing, a non-Mesh, or a destroyed Mesh throws `TypeError`);
`transform?` is a flat array (or typed array) of exactly 16 finite numbers —
a column-major 4×4 matrix, default identity (a wrong length throws
`RangeError`, non-number elements throw `TypeError`); `color?` is a
`[r, g, b, a]` tint, default opaque white. Unknown option fields SHALL throw
`TypeError`. Playback SHALL draw every surface in surface order under the
recorded camera, with the depth test enabled and depth writing on: a
nearer surface occludes a farther one regardless of record order, and
equal-depth fragments resolve by record order (deterministic). Per-surface
shading SHALL be the F4a lit result (see the `lighting` capability) with
albedo equal to the tint multiplied by the surface's vertex color where the
`colors` attribute is present, or the tint alone otherwise. 2D quad records
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

#### Scenario: Validation errors

- **WHEN** `drawMesh` is called with no `mesh`, a 15-element `transform`, or
  an unknown option field
- **THEN** the call throws `TypeError` / `RangeError` / `TypeError`
  respectively and records nothing
