# 3d-core Delta

## MODIFIED Requirements

### Requirement: Multi-surface mesh data

`efx.graphics.createMeshData(surfaces, materials?, opts?)` SHALL build a CPU-side MeshData (native-backed
class, ADR 0011/0013) holding 1..16 **surfaces**. The first positional
argument SHALL be the surface list (`surfaces`), the optional second
positional argument SHALL be the parallel `materials` array (one entry per
surface — a material object or `null` for the engine default; see the
`lighting` capability), and the optional third positional argument SHALL be
the `opts` bag, restricted to `{ bounds? }` (see below). There SHALL be no
single-surface shorthand form: a
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
types SHALL throw `TypeError`; unknown fields SHALL throw `TypeError` (in a
surface, in `materials`, or in `opts`). The
fixed limit is **16 surfaces per mesh** (vision.md fixed limits, recorded in
the reference). When present, `materials` MUST have exactly one entry per
surface (wrong length SHALL throw `RangeError`; invalid entries SHALL throw
`TypeError`); it is bound to its surface at creation time and carried over at
`createMesh`. When present, `opts.bounds` SHALL be an object `{ min, max }`
where `min` and `max` are each a `[x, y, z]` array of finite numbers: it
SHALL replace the engine-computed mesh bounds (see the mesh bounds and
frustum culling requirement) verbatim, and it is carried onto the Mesh at
`createMesh` like the computed bounds. A non-object `opts` or `bounds`, an
unknown `opts` field, or a non-array `min`/`max` SHALL throw `TypeError`; a
wrong `min`/`max` length, a non-number element, or a non-finite value SHALL
throw `RangeError`. MeshData SHALL expose the read-only query property
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

#### Scenario: Bounds override option

- **WHEN** `createMeshData(surfaces, materials, { bounds: { min: [0, 0, 0],
  max: [2, 2, 2] } })` is called, and separately with `bounds` holding a
  2-element `min`, a non-finite value, or `opts` holding an unknown field
- **THEN** the valid form replaces the computed mesh bounds with the given
  box, and the invalid forms throw (`RangeError` for the shape/finite-value
  cases, `TypeError` for the unknown field)

### Requirement: Mesh upload and lifecycle

`efx.graphics.createMesh(meshData)` SHALL upload a live MeshData's **every** surface
CPU → GPU into one Mesh (native-backed class, ADR 0011/0013: deterministic
`destroy()`, idempotent, GC-finalizer backstop, display-list references keep
it alive until playback completes). The Mesh SHALL expose the read-only
query property `surfaceCount` (equal to the MeshData's at upload time;
throws `TypeError` when destroyed). The Mesh SHALL also expose the read-only
query property `bounds` — the mesh bounds carried from the MeshData at
upload time (computed at ingest or overridden; see the mesh bounds and
frustum culling requirement) — as fresh plain data
`{ min, max, center, radius }` (`min`/`max`/`center` are `[x, y, z]` arrays,
`radius` is the bounding-sphere radius of the box; throws `TypeError` when
destroyed; mutating the returned arrays does not affect the engine). A Mesh is a copy: later changes to the
source MeshData object MUST NOT affect the Mesh. Passing a non-MeshData or a
destroyed MeshData SHALL throw `TypeError`. Surface material bindings
carried by the MeshData (from F4) SHALL carry over to the Mesh at upload;
from F4, `Mesh.setSurfaceMaterial(surfaceIndex, mat)` SHALL rebind
one surface's material after upload (the Godot `surface_set_material`
analog: `mat` is a JS-managed object snapshotted at call time; an index out
of range throws `RangeError`); a surface without a bound material SHALL
render with an engine default material. The former free function
`efx.graphics.setMeshSurfaceMaterial` SHALL NOT exist (hard cut, no alias).
Meshes are never slot-based; scripts
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

#### Scenario: Surface material is rebound through the Mesh

- **WHEN** a script calls `mesh.setSurfaceMaterial(0, material)` after upload
- **THEN** surface 0 renders with a snapshot of `material`, and the former
  `efx.graphics.setMeshSurfaceMaterial` is `undefined`

#### Scenario: Destroyed mesh is safe

- **WHEN** `destroy()` is called on a Mesh twice and a `drawMesh` references
  it after the first call
- **THEN** the second `destroy()` is a no-op and the draw throws `TypeError`

#### Scenario: Bounds query property

- **WHEN** a script reads `bounds` on a Mesh created from a MeshData spanning
  `[0, 0, 0]` to `[2, 2, 2]`, and again after destroying the Mesh
- **THEN** the first read returns `{ min: [0, 0, 0], max: [2, 2, 2],
  center: [1, 1, 1], radius: 1.7320… }` as fresh arrays, and the second read
  throws `TypeError`

### Requirement: Whole-mesh drawing with depth

`efx.graphics.drawMesh(mesh, opts?)` SHALL record one draw for the whole mesh, with
`mesh` as a required first positional argument that MUST be a live Mesh
(nothing, a non-Mesh, or a destroyed Mesh throws `TypeError`). `opts?` is an
optional option bag restricted to `{ transform?, color?, skinned?, depthWrite?, cull? }`:
`transform?`
is a flat array (or typed array) of exactly 16 finite numbers — a column-major
4×4 matrix, default identity (a wrong length throws `RangeError`, non-number
elements throw `TypeError`); `color?` is a `[r, g, b, a]` tint, default
opaque white; `skinned?` is a boolean, default `false` — `false`/absent draws
the mesh's bind-pose vertices and `true` draws the current CPU-posed vertices,
and `true` on a mesh without a rig throws `TypeError`; `depthWrite?` is a
boolean, default `true` (a non-boolean throws `TypeError`); `cull?` is a
boolean, default `true` — when `true` the draw MAY be skipped entirely at
record time by engine frustum culling (see the mesh bounds and frustum
culling requirement), and `false` disables that test for the draw (a
non-boolean throws `TypeError`). The option bag, when present, MUST be an
object; unknown option fields (including `mesh`,
which is no longer an option) SHALL throw `TypeError`. Playback SHALL draw
every surface in surface order under the recorded camera, with the depth test
enabled: a nearer surface occludes a farther one regardless of record order,
and equal-depth fragments resolve by record order (deterministic). Depth
**writing** SHALL be enabled when `depthWrite` is `true` and disabled when it
is `false`; with `depthWrite: false` the mesh is depth-tested against the
existing depth buffer but MUST NOT change it, so any geometry recorded after
it (near or far) is not occluded by it. The option SHALL be value-snapshotted
at record time (ADR 0019), like the other draw state. Per-surface
shading SHALL be the `lighting` capability's F4 lit result — the F4a Phong
equation **including the F4b per-channel maps and alpha mask**, or the F4a
**unlit** result when the bound material sets `unlit: true` — with albedo
equal to the tint multiplied by the surface's vertex color where the `colors`
attribute is present, or the tint alone otherwise. The surface `uvs`
attribute (validated and stored since F3) SHALL be consumed by that shading
as the map/alpha-mask texture coordinate; a surface without `uvs` uses the
`(0, 0)` default, so shading stays defined for every mesh. Each surface SHALL
blend using its bound material's `blend` override when the material specifies
one, and otherwise the frame's blend render state (see the `2d-layer` blending
requirement) in effect when the mesh draw was recorded. Both the frame state
and the per-surface material blend SHALL be resolved and value-snapshotted at
record time (ADR 0019): a later `setBlendMode` call or a later
`setMeshSurfaceMaterial` rebind MUST NOT change the recorded draw. 2D quad
records SHALL be untouched by mesh depth (F2 behavior and goldens unchanged).
Mesh draws participate in the per-frame record budget like any record.

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

#### Scenario: Per-surface blend resolves at record time

- **WHEN** a mesh whose surface 0 has `blend: 'additive'` and surface 1 has no
  blend override is drawn while the frame state is `'alpha'`, and the frame
  state is then changed before playback
- **THEN** surface 0 is additive and surface 1 is alpha, using the values in
  effect at record time

#### Scenario: Depth write can be disabled

- **WHEN** a mesh is drawn with `depthWrite: false` and then a farther mesh
  occupying the same screen area is recorded after it
- **THEN** the later, farther mesh renders where it is visible, because the
  first draw wrote no depth

#### Scenario: Disabled depth write still occludes against existing depth

- **WHEN** an opaque mesh is recorded first, then a mesh is drawn with
  `depthWrite: false` at a position behind it
- **THEN** the second mesh is not visible through the first, because it is
  still depth-tested against the existing depth buffer

#### Scenario: Depth write is snapshotted at record time

- **WHEN** a mesh draw is recorded with `depthWrite: false`, and then another
  draw is recorded with the default, and the frame plays back
- **THEN** only the first draw skips depth writing

#### Scenario: Mesh is a required positional argument

- **WHEN** `drawMesh` is called without a mesh (no argument), with a non-Mesh
  first argument, or with a destroyed Mesh
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Option bag holds only transform and color

- **WHEN** `drawMesh(mesh, { transform, color })` is called with a valid bag,
  or `drawMesh(mesh, { mesh })` / `drawMesh(mesh, { frobnicate: 1 })` is
  called with an unknown field
- **THEN** the valid bag records the draw (transform/color applied; the
  documented `skinned`, `depthWrite`, and `cull` options are also accepted),
  and the unknown field throws `TypeError` and records nothing

#### Scenario: Skinned draw on a static mesh rejected

- **WHEN** `drawMesh(mesh, { skinned: true })` is called on a mesh with no
  imported rig
- **THEN** the call throws `TypeError` and records nothing

#### Scenario: Validation errors

- **WHEN** `drawMesh(mesh, { transform })` is called with a 15-element
  `transform`, or `drawMesh(mesh, { depthWrite: 1 })` or
  `drawMesh(mesh, { cull: 1 })` is called
- **THEN** the call throws (`RangeError` for the transform length,
  `TypeError` for the non-booleans) and records nothing

## ADDED Requirements

### Requirement: Mesh bounds and frustum culling

The engine SHALL compute bounds for every MeshData at creation: the
axis-aligned bounding box of all surface vertex `positions` across all
surfaces, stored as `min` and `max`. A `bounds` override supplied to
`createMeshData` SHALL replace the computed box verbatim. The glTF import
path (`loadMeshData`) SHALL compute bounds for its MeshData results the same
way (bind-pose positions). Bounds are value data on the existing
native-backed classes; they add no new resource kind.

At `drawMesh` record time, when the `cull` option is enabled (its default)
and the draw will not use posed buffers (`skinned` is not `true`), the
engine SHALL frustum-cull the draw against the currently active 3D camera —
the same camera state the record snapshots, with the active rendering
surface's aspect (window, or active RenderTarget inside a begin/end pair):
transform the mesh's local bounds box by the draw's model matrix to a
world-space box, and test it against the camera frustum's six planes. The
draw SHALL be skipped — no record pushed — if and only if the world box lies
entirely outside at least one plane; a box that intersects every plane
(straddles any plane) SHALL be recorded. Culling SHALL be fail-open: a
degenerate camera (eye coincident with target) or degenerate bounds
(`min` greater than `max`, as an override may supply) SHALL record the draw,
and any non-finite intermediate value reached inside the engine's test SHALL
likewise fail open (record the draw) rather than cull on undefined math.
JS-reachable non-finite inputs (bounds override values, `transform`
elements) are already rejected by validation before the test — the fail-open
clause governs the engine's decision, not input validation. Culled draws
SHALL consume no
display-list budget and SHALL NOT throw — a culled `drawMesh` call succeeds.

Skinned draws (`skinned: true` on a rig mesh) SHALL never be frustum-culled:
bind-pose bounds do not describe posed geometry, so the test is bypassed
rather than applied to the wrong box. Bounds on skinned meshes describe the
bind pose.

The cull decision SHALL be identical on every runtime binding (quickjs
desktop and the Emscripten web bridge): the same mesh, transform, camera,
and surface must cull or draw identically. Golden output for visible scenes
is unchanged by this requirement — culling only skips draws whose geometry
lies entirely outside the view.

#### Scenario: Bounds are computed at ingest

- **WHEN** MeshData is created from positions spanning a known box (via
  `createMeshData` and, identically, via `loadMeshData`)
- **THEN** the Mesh's `bounds` getter returns that box as `min`/`max` with
  `center` and `radius` derived from it

#### Scenario: Bounds override replaces computed bounds

- **WHEN** MeshData is created with a `bounds` override larger than the
  vertex extents, and the mesh is drawn positioned between the two boxes'
  cull outcomes
- **THEN** culling follows the override box, and the `bounds` getter returns
  the override-derived values

#### Scenario: Fully-outside draw is culled

- **WHEN** a mesh whose world box lies entirely behind the camera plane (or
  fully outside any frustum plane) is drawn with the default `cull`
- **THEN** nothing renders for that draw and the display list holds no
  record for it

#### Scenario: Straddling draw is rendered

- **WHEN** a mesh's world box intersects the frustum boundary — parts
  potentially visible — even when every vertex is outside the view
- **THEN** the draw is recorded and the visible fragments render

#### Scenario: Culled draws do not consume budget

- **WHEN** a scene draws meshes fully outside the view up to and beyond the
  per-frame record budget, with no in-view draws
- **THEN** no budget error is raised for the culled draws, and an in-view
  draw added afterwards still records

#### Scenario: cull false disables the test

- **WHEN** a fully-outside mesh is drawn with `cull: false`
- **THEN** the draw records normally (consuming budget) despite being
  outside the view

#### Scenario: Skinned draws are never culled

- **WHEN** a skinned mesh's bind-pose bounds lie fully outside the view, the
  mesh is posed (`Mesh.pose`) so the posed geometry lands inside the view,
  and the mesh is drawn with `skinned: true`
- **THEN** the posed geometry renders — the draw was not culled by the
  bind-pose box

#### Scenario: Culling fails open

- **WHEN** `drawMesh` is called under a degenerate camera (eye equal to
  target), or with a mesh whose overridden bounds are inverted
  (`min` greater than `max`)
- **THEN** the engine does not cull on the ambiguous state: the draw records
  exactly as it would with culling disabled, and culling never silently
  drops a draw it could not prove fully outside

#### Scenario: Identical culling on every runtime

- **WHEN** the same scene (same meshes, transforms, camera, surface) is run
  through the desktop player and the web player, including fully-outside,
  straddling, and `cull: false` draws
- **THEN** both runtimes record the same draws — the cross-runtime web
  comparison shows no culling-driven differences
