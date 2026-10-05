# Spec Delta

## MODIFIED Requirements

### Requirement: Graphics namespace API
The script API SHALL expose the graphics drawing, state, and resource surface
as the sub-namespace `efx.graphics` of the single `efx` object, with no new
free globals. `efx.graphics` SHALL contain exactly these function members,
each with its unchanged name, signature, semantics, defaults, layer tag, and
error behavior: `beginRenderTarget`, `createFont`, `createImageData`,
`createMesh`, `createMeshData`, `createParticleSystem`, `createRenderTarget`,
`createTexture`, `drawBillboard`, `drawMesh`, `drawParticles`, `drawQuad`,
`drawSprites`, `drawText`, `endRenderTarget`, `loadFontData`, `loadImage`,
`loadMeshData`, `makeCapsule`, `makeCube`, `makePlane`, `makeSphere`,
`setBlendMode`, `setCamera2D`, `setCamera3D`, `setClearColor`,
`setDirectionalLight`, `setLight`, `setPostEffects`, and `setRenderScale` —
plus the engine-owned read-only `whiteTexture` Texture property. The
operations on a native-backed class instance SHALL be methods on that class,
not free functions: `poseMesh`, `setMeshSurfaceMaterial`, and `measureText`
SHALL NOT be members of `efx.graphics` (see the Resource operation methods
requirement). None of these names SHALL also exist as members of the `efx`
root: the move is a hard cut with no deprecated root aliases and no
compatibility shims. The `efx` root SHALL keep only the runtime/lifecycle
members (`log`, `quit`, the `args` property, `registerUpdateHook`,
`registerRenderHook`) and the domain sub-namespaces (`math`, `io`, `color`,
`keyboard`, `mouse`, `window`, `physics`, `gamepad`, `audio`); `whiteTexture`,
`loadText`, and the math helpers are no longer root members. The sub-namespace
SHALL exist on every runtime binding (quickjs desktop and the Emscripten web
bridge) with identical membership, and every entry SHALL behave identically
across them. The generated reference (`docs/api/` from
`gallery/src/api/efx.d.ts`), the guidelines (`docs/js-api.md`), and this
capability's sibling requirements SHALL use the `efx.graphics.*` paths.

#### Scenario: Graphics functions are reachable through the sub-namespace
- **WHEN** a script calls `efx.graphics.drawQuad(...)` — and likewise every
  other listed member — without imports or setup
- **THEN** the call succeeds on every target platform with the exact
  behavior it had at the root before the move

#### Scenario: White texture lives in the graphics sub-namespace
- **WHEN** a script reads `efx.graphics.whiteTexture` and `efx.whiteTexture`
- **THEN** the first is the engine-owned 1×1 opaque-white `Texture` (same
  object on every read; `destroy()` throws) and the second is absent

#### Scenario: No root aliases remain
- **WHEN** a script reads any moved name at the root (for example
  `efx.drawQuad`, `efx.makeCube`, or `efx.whiteTexture`) and enumerates
  `efx.graphics`
- **THEN** the moved names are absent from the root, `efx.graphics` holds
  exactly the listed members, and the root exposes only the lifecycle
  facilities and the domain sub-namespaces

#### Scenario: Resource operations are not graphics members
- **WHEN** a script reads `efx.graphics.measureText`, `efx.graphics.poseMesh`,
  and `efx.graphics.setMeshSurfaceMaterial` and enumerates `efx.graphics`
- **THEN** all three are absent from `efx.graphics`, and the equivalent
  operations exist only as `Font.measure`, `Mesh.pose`, and
  `Mesh.setSurfaceMaterial`

#### Scenario: The move changes no observable behavior
- **WHEN** the in-repo script corpus (golden scenes, portable script tests,
  curated samples) is re-pathed from `efx.<fn>` to `efx.graphics.<fn>` (and
  `efx.whiteTexture` to `efx.graphics.whiteTexture`) and run through both
  runtimes
- **THEN** every golden frame stays pixel-identical, the cross-runtime
  error catalog stays byte-identical, and no signature, default, or error
  message changes

### Requirement: Skinned mesh data and implicit rig payload

`createMeshData` surfaces SHALL accept optional `joints` and `weights`
attributes for skinned meshes (four influences per vertex, glTF-style), with
the same count as the surface's positions. The skeleton and animation clips
associated with an imported skinned asset SHALL remain implicit `MeshData`/`Mesh`
payload — no separate script resource and no read-only clip or joint query
property — while posing is exposed through the `Mesh.pose` method and the `skinned`
`drawMesh` option (F7). The native-backed class list and the `destroy()`
lifecycle are unchanged.

#### Scenario: Skinned surface accepted
- **WHEN** `createMeshData` receives a surface with `joints` and `weights`
  arrays matching its vertex count
- **THEN** the MeshData is built and `createMesh` carries the attributes and
  any imported rig payload onto the `Mesh`

#### Scenario: Attribute count mismatch rejected
- **WHEN** a surface's `joints` or `weights` count does not match its vertex
  count
- **THEN** `createMeshData` throws `RangeError` and records nothing

#### Scenario: No new rig API
- **WHEN** the API reference and gallery type document are read after this
  change
- **THEN** they catalog the joints/weights surface attributes, `Mesh.pose`, and
  the `skinned` draw option, and no skeleton/clip resource, clip/joint query
  property, or playback function

### Requirement: Gallery type document accuracy

The gallery type document `gallery/src/api/efx.d.ts` SHALL accurately
describe the valid call shapes of the public script API: every documented
call form MUST type-check, and invalid calls MUST be rejected at compile
time. It SHALL express the argument convention — required inputs positional,
optional inputs in a trailing all-optional bag — so that a required input
cannot be omitted from an options bag and an unknown or optional-only bag
field is rejected. It SHALL type operations on a native-backed class instance
as methods on that class (`Font.measure`, `Mesh.pose`,
`Mesh.setSurfaceMaterial`), with the subject implicit as the receiver and the
method's own required inputs positional. `createMeshData` SHALL type one
positional surface list (`MeshSurfaceData[]`) plus an optional positional
materials array, with no single-surface shorthand form. It SHALL be updated
in the same change as any script-facing API change, alongside
`docs/js-api.md`, and its declarations MUST agree with that reference
document. Because it is the source of truth for the generated reference, the
committed Markdown reference `docs/api/` SHALL be regenerated from it in that
same change. The document SHALL type the `efx.math`, `efx.io`, and `efx.color`
sub-namespaces and the read-only `efx.args` property, and SHALL reject the
removed root members.

#### Scenario: Batch form does not require shorthand fields

- **WHEN** `efx.graphics.createMeshData([surface, surface])` is type-checked
- **THEN** it compiles with the surface list positional and no single-surface
  shorthand attributes

#### Scenario: Mixing construction forms is rejected

- **WHEN** a call supplies the removed single-surface shorthand bag (a bare
  surface object instead of a surface list) alongside or instead of the
  positional surface list
- **THEN** the type document reports a compile-time error

#### Scenario: Required inputs are typed positionally

- **WHEN** `efx.graphics.createRenderTarget(512, 256)` and
  `efx.physics.createBody(shape, { dynamic: true })` are type-checked
- **THEN** both compile, while an empty bag cannot supply the required
  dimensions/`shape` and such a call is rejected

#### Scenario: drawMesh takes a positional mesh

- **WHEN** `efx.graphics.drawMesh(mesh, { transform, color, skinned })` and
  `efx.graphics.drawMesh(mesh)` are type-checked
- **THEN** both compile, and the former option bag holds only
  `transform`/`color`/`skinned` (a `mesh` field in the bag is rejected)

#### Scenario: Posing API is typed

- **WHEN** `mesh.pose({ clip: 'Walk', time: 1 })` and
  `mesh.pose([{ clip: 0, time: 1, weight: 0.5 }])` are type-checked
- **THEN** both compile, the sample `clip` accepts a name or index, and an
  unknown sample field is rejected

#### Scenario: Surface material and measurement methods are typed

- **WHEN** `mesh.setSurfaceMaterial(0, material)` and
  `font.measure('hello', { width: 200 })` are type-checked, and the removed
  `efx.graphics.poseMesh`, `efx.graphics.measureText`, and
  `efx.graphics.setMeshSurfaceMaterial` forms are type-checked
- **THEN** the first two compile and the removed forms are rejected at
  compile time

#### Scenario: Primitive material option is typed

- **WHEN** `efx.graphics.makeCube({ size: 1, material })` is type-checked with a
  material object
- **THEN** it compiles and the material argument is accepted as a material
  object or `null`

#### Scenario: New namespaces and removed roots are typed

- **WHEN** `efx.math.mat4.identity()`, `efx.io.loadData(path)`,
  `efx.color.white`, and reading `efx.args` are type-checked, and
  `efx.mat4`, `efx.loadText`, and `efx.args()` are type-checked
- **THEN** the first four compile and the last three are rejected at compile
  time

#### Scenario: Type document agrees with the reference

- **WHEN** a script-facing API change updates `gallery/src/api/efx.d.ts`
- **THEN** the same change regenerates `docs/api/` so the published reference
  matches the declaration

### Requirement: Font and text API

The script API SHALL expose font loading, font creation, text drawing, and
text measurement as C-implemented facilities with identical names,
signatures, semantics, and error behavior across the desktop and web
bindings. Font loading and creation and text drawing are members of the
`efx.graphics` sub-namespace; measurement is a method on the `Font` it
measures with:

- `efx.graphics.loadFontData(path)` → a native-backed `FontData` resource (the parsed
  font, no GPU resource), released by `destroy()`.
- `efx.graphics.createFont(fontData, size, opts?)` → a native-backed `Font` that bakes a
  fixed glyph atlas at the requested size and optional baked outline/shadow
  effects; released by `destroy()`; read-only `size`, `lineHeight`, `ascent`,
  `descent`.
- `efx.graphics.drawText(text, font, x, y, opts?)` → lays out and draws the text as
  display-list quads and returns the laid-out bounds
  `{ width, height, lines }`.
- `Font.measure(text, opts?)` → returns the same bounds without drawing,
  with the receiver `Font` supplying the metrics.

Text drawing SHALL be a mid-level C facility (like `drawQuad`/`drawMesh`) —
not a pure-JS high-level convenience — and SHALL be 2D-only. Formatting SHALL
be limited to newlines, greedy word wrapping, horizontal alignment
(`left`/`center`/`right`/`justify`), and vertical alignment
(`top`/`middle`/`bottom`); there SHALL be no rich text (per-span styles or
markup), no 3D/world-space text, and no script-visible glyph metrics, atlas,
or shader. The font/text behavior SHALL be defined by the `font-text`
capability. `docs/js-api.md` and the gallery type document
(`gallery/src/api/efx.d.ts`) SHALL be updated in the same change, and the
provisional `loadFont` entry SHALL be removed.

#### Scenario: Font pipeline is exposed
- **WHEN** the API reference is read after this change
- **THEN** it catalogs `loadFontData`, `createFont`, and `drawText` under `efx.graphics` and `Font.measure` on the `Font` class, each tagged C-implemented and F8, and does not catalog a `loadFont` convenience

#### Scenario: Text drawing is mid-level C
- **WHEN** a reviewer checks the layer of the text entries
- **THEN** `drawText` and `Font.measure` are tagged C-implemented mid-level functions, distinct from the pure-JS high-level layer

#### Scenario: No rich text or 3D text
- **WHEN** the API reference's font/text section is read
- **THEN** it documents wrapping and the four horizontal and three vertical alignment modes, and states that rich text, 3D text, and script-visible glyph metrics are not provided

#### Scenario: Type document is updated in the same change
- **WHEN** this change updates `docs/js-api.md`
- **THEN** `gallery/src/api/efx.d.ts` (and its type-test) declare `FontData`, `Font` with `measure`, `drawText`, and no `measureText` consistently with the reference

#### Scenario: createFont takes size positionally
- **WHEN** `createFont` is called with a live `FontData`, a positive `size`, and an optional trailing bag
- **THEN** the font bakes at `size`; a missing or non-positive `size` throws, and the bag carries only optional fields (`glyphs`, `padding`, `filter`, `outline`, `shadow`)

## ADDED Requirements

### Requirement: Resource operation methods

An operation whose subject is a native-backed class instance SHALL be a
method on that class's prototype, not a free function in `efx.graphics`. The
subject SHALL be the receiver (`this`); the method's own arguments SHALL
follow the argument-passing convention with the subject implicit. The script
API SHALL provide:

- `Font.measure(text, opts?)` → the laid-out bounds `{ width, height, lines }`
  for `text` under the receiver `Font`, without recording a draw.
- `Mesh.pose(pose)` → CPU-poses the receiver skinned `Mesh` from a single
  sample `{ clip, time, weight? }` or an array of samples.
- `Mesh.setSurfaceMaterial(surfaceIndex, mat)` → binds a snapshot of `mat`
  (or `null` for the engine default) to surface `surfaceIndex` of the
  receiver `Mesh`.

The corresponding free functions `efx.graphics.measureText`,
`efx.graphics.poseMesh`, and `efx.graphics.setMeshSurfaceMaterial` SHALL NOT
exist (hard cut, no aliases, no deprecation shim). A method invoked with the
wrong receiver type, or on a destroyed receiver, SHALL throw `TypeError`; the
method's argument validation, defaults, and error classes SHALL be unchanged
from the free-function forms. Error messages SHALL remain byte-identical
across the desktop and web bindings: receiver errors use the class liveness
guard (`expected a Mesh`/`expected a Font`/`using a destroyed resource`), and
a message that named the removed free-function argument form SHALL be updated
to the method form, with the cross-runtime error catalog updated in the same
change. The methods SHALL exist with
identical names, signatures, semantics, and error behavior on every runtime
binding (quickjs desktop and the Emscripten web bridge). `efx.graphics` SHALL
hold constructors, factories, and stateless operations; it SHALL NOT re-take
a native-backed class instance as a free-function argument for an operation
on that instance.

#### Scenario: Measure is a Font method
- **WHEN** a script calls `font.measure('hello', { width: 200 })` on a live
  `Font` and then renders a frame
- **THEN** it receives the same `{ width, height, lines }` bounds the former
  `measureText('hello', font, { width: 200 })` returned, and no glyphs are
  drawn

#### Scenario: Pose is a Mesh method
- **WHEN** a script calls `mesh.pose({ clip: 'Walk', time: t })` and
  `mesh.pose([{ clip: 0, time: t, weight: 1 }])` on a live skinned `Mesh`
- **THEN** the receiver's posed buffer reflects the sampled clip, with clip
  name/index lookup, weight normalization, and time wrapping unchanged

#### Scenario: Bind is a Mesh method
- **WHEN** a script calls `mesh.setSurfaceMaterial(1, material)` and then
  `mesh.setSurfaceMaterial(1, null)`
- **THEN** surface 1 is bound to a snapshot of `material` and then reset to
  the engine default, with the receiver's other surfaces unchanged

#### Scenario: No free-function forms remain
- **WHEN** a script reads `efx.graphics.measureText`, `efx.graphics.poseMesh`,
  and `efx.graphics.setMeshSurfaceMaterial`
- **THEN** all three are `undefined`, and the operations are reachable only
  as `Font.measure`, `Mesh.pose`, and `Mesh.setSurfaceMaterial`

#### Scenario: Wrong receiver or destroyed resource is rejected
- **WHEN** `Font.measure`, `Mesh.pose`, or `Mesh.setSurfaceMaterial` is called
  with a receiver that is not a live instance of that class (including a
  destroyed one)
- **THEN** the call throws `TypeError` and performs no operation

#### Scenario: Identical behavior on both runtimes
- **WHEN** the same method calls are run on the desktop player and in the web
  player, including invalid calls
- **THEN** both runtimes return identical results and throw the same error
  class with the byte-identical message
