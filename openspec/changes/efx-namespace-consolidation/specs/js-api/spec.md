# Spec Delta

## ADDED Requirements

### Requirement: Math namespace API
The script API SHALL expose the pure-JS math helpers as the sub-namespace
`efx.math` of the single `efx` object: `efx.math.mat4`, `efx.math.vec3`, and
`efx.math.quat`, with no new free globals. Their member sets and behavior
SHALL be unchanged from the former root helpers (`efx.mat4` / `efx.vec3` /
`efx.quat`): pure functions that never mutate their arguments and return plain
JS data. The names `mat4`, `vec3`, and `quat` SHALL NOT also exist as members
of the `efx` root: the move is a hard cut with no deprecated root aliases and
no compatibility shims. `efx.math` SHALL exist with identical membership and
behavior on every runtime binding (quickjs desktop and the Emscripten web
bridge). The generated reference, the guidelines, and this capability's
sibling requirements SHALL use the `efx.math.*` paths.

#### Scenario: Math helpers are reachable through the sub-namespace
- **WHEN** a script calls `efx.math.mat4.identity()`, `efx.math.vec3.add(...)`,
  or `efx.math.quat.fromAxisAngle(...)` without imports or setup
- **THEN** the call succeeds on every target platform with the exact behavior
  it had at the root before the move

#### Scenario: No root math aliases remain
- **WHEN** a script reads `efx.mat4`, `efx.vec3`, or `efx.quat` and enumerates
  `efx.math`
- **THEN** the three names are absent from the root and `efx.math` holds
  exactly `mat4`, `vec3`, and `quat`

### Requirement: IO namespace API
The script API SHALL expose resource loading as the sub-namespace `efx.io` of
the single `efx` object, with no new free globals. `efx.io.loadText(path)`
SHALL return a resource as a UTF-8 string and `efx.io.loadData(path)` SHALL
return a resource's raw bytes as a fresh `Uint8Array` copy; both SHALL read
synchronously from the resource root by relative path and SHALL obey the
resource root's path rules. The former root member `efx.loadText` SHALL be
removed (hard cut, no alias), and `loadText` SHALL NOT remain at the root.
Both loaders SHALL be C-implemented, SHALL have identical names, signatures,
semantics, and error behavior across the desktop and web bindings, and SHALL
throw `TypeError` for a non-string path and a standard ES6 `Error` for a
missing, unreadable, or escaping path. `efx.io` SHALL add no native-backed
class, no `destroy()`, and no slot bank, so the native-backed class list and
the fixed-limits table are unchanged.

#### Scenario: loadText returns decoded text
- **WHEN** a script calls `efx.io.loadText(path)` for a text resource in the root
- **THEN** it receives the file's contents as a string

#### Scenario: loadData returns raw bytes
- **WHEN** a script calls `efx.io.loadData(path)` for a binary resource in the root
- **THEN** it receives a `Uint8Array` whose length and bytes equal the file's,
  and mutating it does not affect the engine

#### Scenario: No root loadText alias remains
- **WHEN** a script reads `efx.loadText` and enumerates `efx.io`
- **THEN** `loadText` is absent from the root and `efx.io` exposes `loadText`
  and `loadData`

#### Scenario: Loader errors are identical across runtimes
- **WHEN** a script passes a non-string path to `efx.io.loadText` or
  `efx.io.loadData`, or loads a missing resource, on each runtime
- **THEN** both runtimes throw the same error class with the identical message

### Requirement: Color namespace API
The script API SHALL expose named color constants as the namespace `efx.color`
of the single `efx` object, with no new free globals. `efx.color` SHALL contain
exactly the CSS basic-16 names — `aqua`, `black`, `blue`, `fuchsia`, `gray`,
`green`, `lime`, `maroon`, `navy`, `olive`, `purple`, `red`, `silver`, `teal`,
`white`, `yellow` — plus `transparent`, each a `Color` (`[r, g, b, a]`) tuple
of normalized floats in `0..1`. The 128 and 192 sRGB levels SHALL be expressed
as `0.5` and `0.75` respectively (so `gray` is `[0.5, 0.5, 0.5, 1]`, `silver`
is `[0.75, 0.75, 0.75, 1]`, and `green` is `[0, 0.5, 0, 1]`, distinct from
`lime` `[0, 1, 0, 1]`); `transparent` SHALL be `[0, 0, 0, 0]`. Every constant
SHALL be a frozen plain array (`Object.freeze`) so a script cannot mutate
engine state through it. `efx.color` SHALL contain no functions (no
constructors, parsers, or helpers); the constants SHALL be plain JS data
available identically on every runtime.

#### Scenario: Constants are usable wherever a Color is accepted
- **WHEN** a script passes `efx.color.white` to `efx.graphics.setClearColor`
  or a `color:` option, or `efx.color.transparent` as an alpha-zero color
- **THEN** the call behaves exactly as the equivalent inline `[r, g, b, a]`
  literal

#### Scenario: Constants are frozen
- **WHEN** a script attempts to mutate a constant (for example
  `efx.color.red[0] = 0`)
- **THEN** the write does not change the constant's value

#### Scenario: Namespace has no behavior
- **WHEN** a script enumerates `efx.color`
- **THEN** it holds exactly the 17 named `Color` tuples and no functions

## MODIFIED Requirements

### Requirement: Single global API namespace
All engine-provided script functions SHALL be exposed as members of one
well-known global namespace object (the `efx` object established by F1),
available to every script without imports or setup. Scripts SHALL access
engine functionality only through this namespace and standard ES6 built-ins;
the reference document SHALL state this rule. This covers both C-implemented
functions and engine-provided high-level JS functions. Domain functionality
SHALL be organized in sub-namespaces of this one object; the graphics
drawing, state, and resource functions SHALL live in `efx.graphics` (see the
Graphics namespace API requirement), the pure-JS math helpers in `efx.math`,
the resource loaders in `efx.io`, and the named color constants in
`efx.color`. The `efx` root SHALL hold only the runtime/lifecycle facilities —
`log`, `quit`, the read-only `args` property, and `registerUpdateHook` /
`registerRenderHook` — plus the domain sub-namespaces. The CommonJS module
facilities `require`, `module`, and `exports` (F10) are module-scoped
authoring facilities and SHALL NOT be members of the `efx` namespace or free
globals; `require` returns a module's exports and is not an engine API entry.

#### Scenario: Namespace available without setup
- **WHEN** a script calls `efx.log` without any import or setup code
- **THEN** the call succeeds on every target platform

#### Scenario: No scattered engine globals
- **WHEN** a reviewer checks how a script reaches an engine function
- **THEN** every engine-provided function is reachable as a member of the
  single namespace, not as an additional free global

#### Scenario: Sub-namespaces are members of the single namespace
- **WHEN** a script reaches input through `efx.keyboard`, `efx.mouse`, or
  `efx.window`, graphics through `efx.graphics`, math through `efx.math`,
  loading through `efx.io`, or colors through `efx.color`
- **THEN** those objects are members of the single `efx` namespace object and
  add no free global

#### Scenario: Graphics functions are members of the single namespace
- **WHEN** a script reaches graphics functionality through `efx.graphics`
- **THEN** `efx.graphics` is a member of the single `efx` namespace object
  (one level deep) and adds no free global

#### Scenario: Script arguments are a read-only property
- **WHEN** a script reads `efx.args` without calling it
- **THEN** it receives a `string[]` of the host `--script <file> [args…]`
  tail (empty when none were given), `efx.args` is not a function, and
  mutating the returned array does not affect the engine

#### Scenario: Module facilities are not engine globals
- **WHEN** a script inspects the `efx` namespace and the global scope outside a
  module's scope
- **THEN** `require`, `module`, and `exports` are absent from both; they exist
  only inside a module's own scope

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
`measureText`, `poseMesh`, `setBlendMode`, `setCamera2D`, `setCamera3D`,
`setClearColor`, `setDirectionalLight`, `setLight`, `setMeshSurfaceMaterial`,
`setPostEffects`, and `setRenderScale` — plus the engine-owned read-only
`whiteTexture` Texture property. None of these names SHALL also exist as
members of the `efx` root: the move is a hard cut with no deprecated root
aliases and no compatibility shims. The `efx` root SHALL keep only the
runtime/lifecycle members (`log`, `quit`, the `args` property,
`registerUpdateHook`, `registerRenderHook`) and the domain sub-namespaces
(`math`, `io`, `color`, `keyboard`, `mouse`, `window`, `physics`, `gamepad`,
`audio`); `whiteTexture`, `loadText`, and the math helpers are no longer root
members. The sub-namespace SHALL exist on every runtime binding (quickjs
desktop and the Emscripten web bridge) with identical membership, and every
entry SHALL behave identically across them. The generated reference
(`docs/api/` from `gallery/src/api/efx.d.ts`), the guidelines
(`docs/js-api.md`), and this capability's sibling requirements SHALL use the
`efx.graphics.*` paths.

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

#### Scenario: The move changes no observable behavior
- **WHEN** the in-repo script corpus (golden scenes, portable script tests,
  curated samples) is re-pathed from `efx.<fn>` to `efx.graphics.<fn>` (and
  `efx.whiteTexture` to `efx.graphics.whiteTexture`) and run through both
  runtimes
- **THEN** every golden frame stays pixel-identical, the cross-runtime
  error catalog stays byte-identical, and no signature, default, or error
  message changes

### Requirement: Normative API reference document

The project SHALL maintain the script API's per-symbol reference as generated
output, not hand-written prose. The TypeScript declaration
`gallery/src/api/efx.d.ts` SHALL be the single source of truth for the
reference: its TSDoc comments define each symbol's summary, parameters,
return value, defaults, constraints, and examples. The reference SHALL be
produced from that declaration by a pinned generator and published in two
renderings: a Markdown rendering committed under `docs/api/`, and an HTML
rendering published on the gallery site under `/api`.

`docs/js-api.md` SHALL be maintained as the API **design guidelines**. It
SHALL state the design rules future API additions must follow — the single
namespace and its sub-namespace organization, the two-layer structure,
naming and option-bag conventions, units and colors, the error model, the
resource and memory model, the fixed limits, the lifecycle model (loading
`main.js` as the implicit init, explicit stacking hook registration with `dt`
and unsubscribe, and the load-time `update`/`render` sugar), the CommonJS
module model, and the gamepad, audio, math, io, and color namespace models —
and it SHALL direct readers to the generated reference for per-symbol detail
rather than cataloging every function itself.

Any change that adds, modifies, or removes a public API function SHALL update
the declaration in the same change and SHALL regenerate the committed
Markdown reference from it. The generated Markdown SHALL NOT be hand-edited.
The reference SHALL NOT tag entries by internal roadmap milestone, and
entries SHALL NOT be marked provisional.

#### Scenario: Callable-today vs planned is distinguishable
- **WHEN** a reader opens the reference
- **THEN** every documented symbol describes current shipped behavior, and no
  entry is marked provisional

#### Scenario: Milestone change updates the reference
- **WHEN** a feature change adds or changes an API function
- **THEN** the same change updates the declaration and regenerates
  `docs/api/`, with no internal milestone tag added to the reference

#### Scenario: Input namespaces are documented
- **WHEN** the reference is read
- **THEN** it documents `efx.keyboard`, `efx.mouse`, and `efx.window` with
  their query functions, event registrations, read-only properties, the
  key/button name set, and the surface-pixel coordinate rule

#### Scenario: Module model is documented
- **WHEN** the API design guidelines are read
- **THEN** they document the CommonJS module format, the synchronous resolver
  and its supported/unsupported specifier forms, module caching and cycles,
  JSON modules, the module-shaped entry hooks, and that Node/npm
  compatibility is not provided

#### Scenario: Particle, billboard, and sprite API is documented
- **WHEN** the reference is read
- **THEN** it documents `drawBillboard`, `drawSprites`, `createParticleSystem`,
  and `drawParticles` with their options, error behavior, the `ParticleSystem`
  class and its lifecycle, and the `facing` render modes

#### Scenario: Gamepad namespace is documented
- **WHEN** the reference is read
- **THEN** it documents `efx.gamepad` with its `count`/`get`, the pad view's
  query methods and read-only properties, the semantic button/axis name sets,
  the canonical range and trigger threshold, and the raw fallback

#### Scenario: Audio namespace is documented
- **WHEN** the reference is read
- **THEN** it documents `efx.audio` with its entry points, the
  `AudioData`/`AudioStream`/`Audio` classes and their properties, the
  decoded-PCM-only rule, the no-device and web-unlock behavior, and the fixed
  limits

#### Scenario: Math, io, and color namespaces are documented
- **WHEN** the reference is read
- **THEN** it documents `efx.math` with `mat4`/`vec3`/`quat`, `efx.io` with
  `loadText`/`loadData`, and `efx.color` with its 17 named `Color` constants,
  and the guidelines state that `efx.args` is a read-only property

#### Scenario: Catalog derived from vision
- **WHEN** the guidelines' vision traceability is checked against vision.md
- **THEN** every capability vision.md names for the consumer API (2D quads,
  meshes, vertex colors, cameras, lights, Phong materials with maps, alpha
  masks, blending modes, render targets, post FX, resource loading,
  keyboard/mouse input query and events, gamepad input, audio playback,
  script modules, skinning/animation, high-level text drawing) has a
  corresponding documented symbol or an explicitly noted open question

#### Scenario: Reference is generated, not hand-written
- **WHEN** a reader consults the per-symbol API reference
- **THEN** it was generated from `gallery/src/api/efx.d.ts`, the committed
  Markdown lives under `docs/api/`, and the published HTML is served under
  `/api`

#### Scenario: API change updates declaration and reference
- **WHEN** a change adds, modifies, or removes a public API function
- **THEN** the same change updates `gallery/src/api/efx.d.ts` and regenerates
  `docs/api/` from it

#### Scenario: Committed reference cannot go stale
- **WHEN** the committed `docs/api/` is compared with a fresh generation from
  the declaration
- **THEN** any difference fails verification

#### Scenario: Guidelines carry the design rules
- **WHEN** `docs/js-api.md` is read
- **THEN** it states the design rules (namespace organization, two layers,
  conventions, units and colors, errors, resource model, limits, lifecycle,
  modules, and the gamepad/audio/math/io/color namespace models) and points to
  the generated reference for per-symbol detail

#### Scenario: No internal milestone tags
- **WHEN** a symbol entry in the generated reference is read
- **THEN** it carries no roadmap milestone tag and no provisional marker

#### Scenario: Hand-written per-function catalog is retired
- **WHEN** `docs/js-api.md` is searched for per-function signature catalogs
- **THEN** none are present; per-symbol detail lives only in the generated
  reference

### Requirement: Resource loading and texture composition

The script API SHALL provide a resource-loading layer that reads files from
the resource root by relative path and returns engine resources. Each function
SHALL be tagged with its layer in the reference: `efx.io.loadText` and
`efx.io.loadData` — members of the `efx.io` sub-namespace — and
`efx.graphics.loadImage` are C-implemented loaders. There SHALL be no separate
texture loader: a texture is created by composing the public API,
`efx.graphics.createTexture(efx.graphics.loadImage(path), opts?)`, matching the
mesh flow where `createMesh` consumes `loadMeshData`. Loading SHALL be
synchronous from the script's point of view on every target. A missing,
unreadable, or undecodable resource SHALL throw a standard ES6 `Error`; a
malformed path argument SHALL throw `TypeError`. The reference document
(`docs/js-api.md`) and the gallery type document
(`gallery/src/api/efx.d.ts`) SHALL be updated in the same change that delivers
these functions.

#### Scenario: loadText returns decoded text
- **WHEN** a script calls `efx.io.loadText(path)` for a text resource in the root
- **THEN** it receives the file's contents as a string

#### Scenario: loadData returns raw bytes
- **WHEN** a script calls `efx.io.loadData(path)` for a binary resource in the root
- **THEN** it receives a `Uint8Array` copy of the file's bytes, with no
  decoding applied

#### Scenario: loadImage returns ImageData
- **WHEN** a script calls `efx.graphics.loadImage(path)` for a PNG or JPEG in the root
- **THEN** it receives an `ImageData` with read-only pixel dimensions and
  decoded RGBA pixels, releasable with `destroy()`

#### Scenario: Texture creation composes loadImage and createTexture
- **WHEN** a script calls `efx.graphics.createTexture(efx.graphics.loadImage(path), opts?)`
- **THEN** it receives a live `Texture` carrying the image's pixels plus any
  requested sampler and mipmap options, with the same `destroy()` lifecycle

#### Scenario: No texture-loading convenience
- **WHEN** the API reference and gallery type document are read after this
  change
- **THEN** they catalog `loadImage` and `createTexture` and do not catalog a
  `loadTexture` function

#### Scenario: Undecodable or missing resource throws
- **WHEN** a script loads a path that does not exist or is not a decodable
  image (for `loadImage`)
- **THEN** the call throws an `Error` and no resource object is returned

#### Scenario: Malformed argument throws TypeError
- **WHEN** a script passes a non-string path to a load function
- **THEN** the call throws `TypeError`

### Requirement: Gallery type document accuracy

The gallery type document `gallery/src/api/efx.d.ts` SHALL accurately
describe the valid call shapes of the public script API: every documented
call form MUST type-check, and invalid calls MUST be rejected at compile
time. It MUST type a function's alternative call forms so that each form's
required fields are required only for that form (for example, the
`createMeshData` batch bag versus its single-surface shorthand) and so that
mixing forms is rejected. It SHALL be updated in the same change as any
script-facing API change, alongside `docs/js-api.md`, and its declarations
MUST agree with that reference document. Because it is the source of truth
for the generated reference, the committed Markdown reference `docs/api/`
SHALL be regenerated from it in that same change. The document SHALL type the
`efx.math`, `efx.io`, and `efx.color` sub-namespaces and the read-only
`efx.args` property, and SHALL reject the removed root members.

#### Scenario: Batch form does not require shorthand fields

- **WHEN** `efx.graphics.createMeshData({ surfaces: [surface, surface] })` is
  type-checked
- **THEN** it compiles without supplying top-level `positions` or other
  shorthand attributes

#### Scenario: Mixing construction forms is rejected

- **WHEN** `efx.graphics.createMeshData({ surfaces: [surface], positions })` combines
  the batch bag and the shorthand fields in one call
- **THEN** the type document reports a compile-time error

#### Scenario: drawMesh takes a positional mesh

- **WHEN** `efx.graphics.drawMesh(mesh, { transform, color, skinned })` and
  `efx.graphics.drawMesh(mesh)` are type-checked
- **THEN** both compile, and the former option bag holds only
  `transform`/`color`/`skinned` (a `mesh` field in the bag is rejected)

#### Scenario: Posing API is typed

- **WHEN** `efx.graphics.poseMesh(mesh, { clip: 'Walk', time: 1 })` and
  `efx.graphics.poseMesh(mesh, [{ clip: 0, time: 1, weight: 0.5 }])` are type-checked
- **THEN** both compile, the sample `clip` accepts a name or index, and an
  unknown sample field is rejected

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
