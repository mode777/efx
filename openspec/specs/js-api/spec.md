# js-api

## Purpose

Defines the contract for the engine's script-facing JavaScript API: one global
namespace, the two-layer (C-implemented / pure-JS) structure, the resource
memory model with fixed limits, and the normative developer-facing reference
document that catalogs every public API function by delivery milestone.

## Requirements

### Requirement: Single global API namespace
All engine-provided script functions SHALL be exposed as members of one
well-known global namespace object (the `efx` object established by F1),
available to every script without imports or setup. Scripts SHALL access
engine functionality only through this namespace and standard ES6 built-ins;
the reference document SHALL state this rule. This covers both C-implemented
functions and engine-provided high-level JS functions. The CommonJS module
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
- **WHEN** a script reaches input through `efx.keyboard`, `efx.mouse`, or `efx.window`
- **THEN** those objects are members of the single `efx` namespace object and
  add no free global

#### Scenario: Module facilities are not engine globals
- **WHEN** a script inspects the `efx` namespace and the global scope outside a
  module's scope
- **THEN** `require`, `module`, and `exports` are absent from both; they exist
  only inside a module's own scope

### Requirement: Two-layer API with strict layering
The script API SHALL consist of exactly two layers: low/mid-level functions
implemented in C/C++ and registered through the engine binding, and
high-level convenience functions implemented in pure ES6. High-level
functions MUST be implemented using only the public low/mid-level API and
standard ES6 built-ins — they MUST NOT use private bindings or host
facilities that are not part of the public API. Every API function in the
reference SHALL be tagged with its layer.

A low/mid-level function MAY validate and normalize its arguments in the
engine-bundled pure-ES6 layer before its C/C++ implementation runs. This
argument handling SHALL be written once and shared by every runtime, and it
SHALL reach the native implementation only through an engine-internal binding
object that is neither a member of the `efx` namespace nor a global. Such a
function remains a low/mid-level, C-implemented function: its observable
behavior, layer tag and reference entry are unchanged, and the shared argument
handling SHALL NOT be callable by scripts on its own.

#### Scenario: High-level function built on public API
- **WHEN** a high-level convenience function (e.g. a model or text drawer) is
  implemented
- **THEN** it calls only documented public API functions and standard ES6

#### Scenario: Layer tag present
- **WHEN** a function entry is read in the API reference
- **THEN** its entry marks it as either C-implemented or pure-JS

#### Scenario: Shared argument handling stays private
- **WHEN** a script enumerates the `efx` namespace and the global scope
- **THEN** no engine-internal binding object or argument validator used by a
  C-implemented function is reachable from either

#### Scenario: Shared validation keeps the layer tag
- **WHEN** a C-implemented function validates its option object in the shared
  pure-ES6 layer before entering native code
- **THEN** its API reference entry is still tagged C-implemented and its
  documented signature, defaults and errors are unchanged

### Requirement: Resource classification and fixed limits

Every engine resource type that scripts can create or reference SHALL be
classified in the reference as exactly one of: JS-managed (plain script
objects, garbage collected), native-backed class (an opaque JS object
wrapping a native handle with an explicit `destroy()` release method; such
a class MAY additionally expose documented read-only query properties, which
MUST be listed in the reference — the instances are Texture's `width` and
`height`, MeshData's and Mesh's read-only `surfaceCount` delivered by F3,
RenderTarget's `width` and `height` delivered by F5a, Font's `size`,
`lineHeight`, `ascent`, and `descent` delivered by F8, ParticleSystem's
`count` delivered by F11, and the physics
`Body`'s `position`, `velocity`, `contacts`, and `transform` and `Character`'s
`position`, `velocity`, and `onFloor` delivered by F12), or slot-based
(a fixed pre-allocated bank of indexed resources).
The native-backed classes SHALL be exactly: MeshData, ImageData, Mesh,
Texture, RenderTarget, FontData, Font, ParticleSystem, Body, Character,
AudioData, AudioStream, and Audio;
skins, skeletons,
and animation
clips are
implicit Mesh payload — loaded with the mesh and posed by the script
(`efx.poseMesh`) — and are not script resources; extending the class list
requires a `js-api` delta. A live RenderTarget SHALL be accepted wherever a
live Texture is accepted (quad drawing, material channel `map`s, and
`alphaMask`), referenced directly by handle with identical validation and
error behavior — the engine SHALL NOT create or expose any alias Texture
object for a RenderTarget. Phong **materials** (the parameter objects bound
per mesh surface from F4a) SHALL be classified as JS-managed: the engine
snapshots their channel values at binding time and they hold no native handle
or `destroy()`. A JS-managed material MAY carry per-channel maps that
reference native-backed `Texture` or `RenderTarget` objects (F4b; RenderTargets
accepted from F5a); the engine snapshots the texture handles at binding time
and **retains** the referenced resources while the material stays bound, so
the material does not own them and its
classification and release contract are unchanged. Post-effect chain entries
and their option bags (F5b) SHALL be classified JS-managed: plain objects
snapshotted at `setPostEffects` call time, holding no native handle and no
`destroy()`. A `ParticleSystem` SHALL be a native-backed class (F11) whose
configuration is plain value state snapshotted by the engine; it SHALL retain
the `Texture` or `RenderTarget` it draws with until the system is destroyed.
The audio classes SHALL be native-backed (F14): `AudioData` holds fully-decoded
PCM and exposes no query properties; `AudioStream` holds a compressed resource
plus an incremental decoder and exposes no query properties; `Audio` is one
playback handle with a read-only `playing` and `paused` and read-write
`volume`, `pan`, `pitch`, and `loop`, plus `stop`, `pause`, `resume`, and
`destroy()`. The engine's fixed playback bank is engine-owned and SHALL NOT be
a script-visible slot bank; scripts hold per-playback handles, not indices.
Every resource requiring
native storage MUST be a
native-backed class — released deterministically by its `destroy()`,
reclaimed by its GC finalizer if the script never calls it, and finalized
at runtime teardown — unless its count is fixed by design, in which case it
is slot-based. The runtime MUST factor native allocation sizes (CPU and GPU)
into GC pressure and MUST run collection at frame end, bounding
unreferenced native waste to roughly one frame. Resources recorded into the
display list MUST stay alive until playback completes, and a `Texture` or
`RenderTarget` referenced by a bound material map MUST stay alive until that
binding is released. The reference SHALL
document the engine's fixed limits: 4 point lights, 1 directional light,
1 camera, 16 surfaces per mesh (F3), a post-effect chain of at most
8 entries (F5b), at most 65536 particles per particle system (F11), a cap of
4 concurrent streamed sources and 32 playback voices (F14);
lights are the only slot bank.

#### Scenario: Fixed limits stated

- **WHEN** the reference document's limits section is read
- **THEN** it states 4 point lights, 1 directional light, 1 camera, 16 surfaces per mesh, the 8-entry post-effect chain cap, the 65536-particle system cap, the concurrent streamed-source cap, and 32 playback voices, matching vision.md and the 3d-core, post-fx, particles, and audio capabilities

#### Scenario: Unreleased native resource is reclaimed

- **WHEN** a script creates textures in a loop and never calls `destroy()` on them
- **THEN** the native sizes drive GC pressure, finalizers reclaim the objects within roughly a frame of them becoming unreachable, and nothing leaks at runtime shutdown

#### Scenario: Destroyed resource is safe

- **WHEN** a script calls `destroy()` on a resource that the display list recorded earlier in the same frame
- **THEN** the native release is deferred until playback completes, and subsequent use of the destroyed resource throws

#### Scenario: Query properties are documented per class

- **WHEN** the reference document's native-backed class entries are read
- **THEN** the Texture and RenderTarget entries list the read-only `width` and `height`, the MeshData and Mesh entries list the read-only `surfaceCount`, the Font entry lists the read-only `size`, `lineHeight`, `ascent`, and `descent`, the ParticleSystem entry lists the read-only `count`, the Body entry lists `position`, `velocity`, `contacts`, and `transform`, the Character entry lists `position`, `velocity`, and `onFloor`, the Audio entry lists the read-only `playing` and `paused` and the read-write `volume`, `pan`, `pitch`, and `loop`, the AudioData, AudioStream, and FontData entries list none, and every other entry states that it has none

#### Scenario: Render targets are accepted wherever textures are

- **WHEN** the reference document's render-target and texture entries are read
- **THEN** they state that a live RenderTarget is accepted wherever a live Texture is accepted — `drawQuad`, material `map`s, `alphaMask` — with identical error behavior, and that no alias Texture object exists for a target

#### Scenario: Materials are classified JS-managed

- **WHEN** the reference document's material entries are read
- **THEN** materials are stated to be plain JS objects with no native handle and no `destroy()`, and lights are stated to be the only slot-based bank

#### Scenario: Material maps reference native textures

- **WHEN** a material with a map is bound and the reference document's material entry is read
- **THEN** it states that the map references a native-backed `Texture` or `RenderTarget` that the engine retains while bound, and that the material itself remains JS-managed with no `destroy()`

#### Scenario: Post-effect entries are classified JS-managed

- **WHEN** the reference document's post-effect entries are read
- **THEN** chain entries and option bags are stated to be plain JS objects snapshotted at call time, with no native handle and no `destroy()`, and the native effect passes are stated to be engine-owned (never script-visible)

#### Scenario: Particle system is a native-backed class

- **WHEN** the reference document's particle entries are read
- **THEN** `ParticleSystem` is listed as a native-backed class with a `count` query property, a `destroy()` release, and a retained texture, and its configuration is stated to be plain value state snapshotted by the engine

#### Scenario: Audio classes are classified native-backed

- **WHEN** the reference document's audio entries are read
- **THEN** `AudioData`, `AudioStream`, and `Audio` are stated to be native-backed classes with an idempotent `destroy()` and a GC-finalizer backstop, the fixed playback bank is stated to be engine-owned (not a script slot bank), and the fixed-limits table lists the concurrent streamed-source cap and 32 playback voices

#### Scenario: Resource without a classification

- **WHEN** a change proposes exposing a new resource type to scripts without classifying it as JS-managed, native-backed class, or slot-based
- **THEN** the change is incomplete and MUST NOT update the API reference

#### Scenario: Physics classes are classified native-backed

- **WHEN** the reference document's physics entries are read
- **THEN** `Body` and `Character` are stated to be native-backed classes with an idempotent `destroy()` that the world holds while live (an unreferenced one keeps simulating until `destroy()` or `efx.physics.clear()`), and the fixed-limits table is unchanged (physics uses dynamic allocation with a documented soft guidance, not a fixed cap)

### Requirement: Explicit lifecycle hook registration
The engine SHALL expose `efx.registerUpdateHook(fn)` and
`efx.registerRenderHook(fn)` as public C-implemented functions on the single
`efx` namespace, on every runtime binding, with identical names, signatures,
semantics, and error behavior. Each call SHALL require `fn` to be a function
and SHALL throw `TypeError` otherwise. Registration SHALL append to the
respective hook list; hooks SHALL stack and run in registration order, with
all update hooks (each receiving `dt`, seconds since the previous frame)
before all render hooks (called with no arguments), once per frame. Each
registration SHALL return an unsubscribe function; calling it SHALL remove
that registration and SHALL be idempotent. An uncaught exception in any hook
SHALL stop the run with the non-zero exit-code contract. The F1 global
`update`/`render` functions SHALL remain supported as load-time sugar: when
defined at the end of evaluating `main.js`, they SHALL be registered in load
order, after any hooks registered during evaluation, and scripts that use only
the globals SHALL keep working (their update callback now also receives `dt`).

#### Scenario: Hooks stack in registration order
- **WHEN** a script registers two update hooks and one render hook, then a
  frame runs
- **THEN** both update hooks run in registration order, each receiving `dt`,
  before the render hook runs

#### Scenario: Unsubscribe removes a hook
- **WHEN** a script registers a hook and then calls the returned unsubscribe
  function
- **THEN** the hook no longer runs on subsequent frames, and calling the
  unsubscribe function again is a no-op

#### Scenario: Update hooks receive frame time
- **WHEN** an update hook runs
- **THEN** its first argument is a finite number of seconds since the previous
  frame

#### Scenario: Global hooks are load-time sugar
- **WHEN** a `main.js` defines global `update` and `render` functions and
  registers no explicit hooks
- **THEN** both globals are registered after evaluation and run once per frame
  in load order, with no behavior change other than the global `update`
  receiving `dt`

#### Scenario: Non-function registration is rejected
- **WHEN** a script calls `efx.registerUpdateHook` or
  `efx.registerRenderHook` with a non-function value
- **THEN** the call throws `TypeError` and registers nothing

#### Scenario: Hook exception stops the run
- **WHEN** any registered hook throws an uncaught exception
- **THEN** the frame loop stops, the error is reported on stderr/error
  channel, and the player exits non-zero

### Requirement: Normative API reference document

The project SHALL maintain `docs/js-api.md` as the normative, developer-facing
reference of the entire script API. It SHALL contain an entry for every public
API function with a signature sketch, a description, its layer tag, and the
roadmap milestone (F1–F14) that delivers it. Entries for functions whose
milestone has not passed its verification gate SHALL be explicitly marked
provisional. The document SHALL also document the lifecycle model — loading `main.js`
as the implicit init, with the `efx` namespace and engine API ready before it
executes (the rendering surface is initialized when the frame loop starts and
is not script-visible at load time), plus explicit, stacking hook registration
(`registerUpdateHook` / `registerRenderHook`, update hooks receiving `dt`,
unsubscribe returned, F1 globals as load-time sugar) — and how API errors
surface (exceptions, exit codes). It SHALL document the CommonJS module model
(F10): the module format and synchronous `require` resolution, module caching
and cycles, `module.exports`/`exports`, the JSON-module form, the restricted
resolver and unsupported specifiers, the module-shaped `update`/`render`
exports, and the unchanged no-browser/Node-dependency rule. It SHALL document
the gamepad namespace (F13): the pad bank and `count`/`get`, the pad view and
its query methods and read-only properties, the semantic button/axis name
sets, the canonical axis range and trigger threshold, the raw fallback, and
the error behavior. It SHALL document the audio namespace (F14): the
`efx.audio` entry points, the `AudioData`/`AudioStream`/`Audio` classes and
their properties, the static-versus-streamed source model, the decoded-PCM-only
rule (no sequenced/modular formats), the no-device and web-unlock behavior, and
the fixed limits. Any change that
adds, modifies, or removes a public API function
MUST update the document in the same change.

#### Scenario: Callable-today vs planned is distinguishable

- **WHEN** a reader opens the reference
- **THEN** the F1 functions (`efx.log`, `efx.quit`, `efx.args`,
  `efx.registerUpdateHook`, `efx.registerRenderHook`) are presented as current
  behavior, and later-milestone entries are marked provisional

#### Scenario: Milestone change updates the reference

- **WHEN** a feature change adds or changes an API function
- **THEN** the same change contains the matching `docs/js-api.md` update with
  the function's signature, layer, and milestone tags

#### Scenario: Input namespaces are documented

- **WHEN** the reference is read after this change
- **THEN** it catalogs `efx.keyboard`, `efx.mouse`, and `efx.window` with
  their query functions, event registrations, read-only properties, the
  key/button name set, the surface-pixel coordinate rule, and the F9 milestone
  tag

#### Scenario: Module model is documented

- **WHEN** the reference is read after this change
- **THEN** it documents the CommonJS module format, the synchronous resolver
  and its supported/unsupported specifier forms, module caching and cycles,
  JSON modules, the module-shaped entry hooks, and the F10 milestone tag, and
  states that Node/npm compatibility is not provided

#### Scenario: Particle, billboard, and sprite API is documented

- **WHEN** the reference is read after this change
- **THEN** it catalogs `drawBillboard`, `drawSprites`, `createParticleSystem`,
  and `drawParticles` with their options, error behavior, the `ParticleSystem`
  class and its lifecycle, the `facing` render modes, and the F11 milestone
  tag

#### Scenario: Gamepad namespace is documented

- **WHEN** the reference is read after this change
- **THEN** it catalogs `efx.gamepad` with its `count`/`get`, the pad view's
  query methods and read-only properties, the semantic button/axis name sets,
  the canonical range and trigger threshold, the raw fallback, and the F13
  milestone tag

#### Scenario: Audio namespace is documented

- **WHEN** the reference is read after this change
- **THEN** it catalogs `efx.audio` with `loadAudioData`, `loadAudioStream`,
  `playAudio`, the master output gain, and `resume`, the
  `AudioData`/`AudioStream`/`Audio` classes and their properties, the
  static-versus-streamed source model, the decoded-PCM-only rule, the no-device
  and web-unlock behavior, the fixed limits, and the F14 milestone tag

#### Scenario: Catalog derived from vision

- **WHEN** the document's function catalog is checked against vision.md
- **THEN** every capability vision.md names for the consumer API (2D quads,
  meshes, vertex colors, cameras, lights, Phong materials with maps, alpha
  masks, blending modes, render targets, post FX, resource loading,
  keyboard/mouse input query and events, gamepad input, audio playback, script
  modules, skinning/animation, high-level text drawing) has a corresponding
  catalog entry or an explicitly noted open question

### Requirement: glTF mesh import API

The script API SHALL provide a glTF import function that returns a `MeshData`
with surfaces, converted and bound materials, and imported textures. The
function is C-implemented and synchronous from the script's point of view; the
resulting `MeshData` is the existing native-backed class, releasable with
`destroy()`, and `createMesh(meshData)` uploads it. The optional selection
object SHALL accept a mesh index or name and SHALL reject unknown fields with
`TypeError`. A load, format, or selection failure SHALL throw a standard ES6
`Error`. The reference document and the gallery type document SHALL be updated
in the same change, and the provisional `loadMesh` entry SHALL be removed.

#### Scenario: Import returns MeshData
- **WHEN** a script calls the import function on a glTF asset
- **THEN** it receives a `MeshData` whose surfaces carry imported geometry and
  whose materials are bound with imported textures

#### Scenario: Import then upload
- **WHEN** a script passes the imported `MeshData` to `createMesh`
- **THEN** the mesh uploads and draws with its imported materials

#### Scenario: Mesh selector validated
- **WHEN** the selection object names an unknown mesh or contains an unknown
  field
- **THEN** the call throws (`Error` for an unknown mesh, `TypeError` for an
  unknown field) and returns no resource

#### Scenario: No loadMesh convenience
- **WHEN** the API reference is read after this change
- **THEN** it catalogs the import function and does not catalog a separate
  `loadMesh`

### Requirement: Skinned mesh data and implicit rig payload

`createMeshData` surfaces SHALL accept optional `joints` and `weights`
attributes for skinned meshes (four influences per vertex, glTF-style), with
the same count as the surface's positions. The skeleton and animation clips
associated with an imported skinned asset SHALL remain implicit `MeshData`/`Mesh`
payload — no separate script resource and no read-only clip or joint query
property — while posing is exposed through `efx.poseMesh` and the `skinned`
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
- **THEN** they catalog the joints/weights surface attributes, `poseMesh`, and
  the `skinned` draw option, and no skeleton/clip resource, clip/joint query
  property, or playback function

### Requirement: Resource loading and texture composition

The script API SHALL provide a resource-loading layer that reads files from
the resource root by relative path and returns engine resources. Each function
SHALL be tagged with its layer in the reference: `loadText` and `loadImage`
are C-implemented loaders. There SHALL be no separate texture loader: a
texture is created by composing the public API,
`createTexture(loadImage(path), opts?)`, matching the mesh flow where
`createMesh` consumes `loadMeshData`. Loading SHALL be synchronous from the
script's point of view on every target. A missing, unreadable, or undecodable
resource SHALL throw a standard ES6 `Error`; a malformed path argument SHALL
throw `TypeError`. The reference document (`docs/js-api.md`) and the gallery
type document (`gallery/src/api/efx.d.ts`) SHALL be updated in the same change
that delivers these functions.

#### Scenario: loadText returns decoded text
- **WHEN** a script calls `efx.loadText(path)` for a text resource in the root
- **THEN** it receives the file's contents as a string

#### Scenario: loadImage returns ImageData
- **WHEN** a script calls `efx.loadImage(path)` for a PNG or JPEG in the root
- **THEN** it receives an `ImageData` with read-only pixel dimensions and
  decoded RGBA pixels, releasable with `destroy()`

#### Scenario: Texture creation composes loadImage and createTexture
- **WHEN** a script calls `efx.createTexture(efx.loadImage(path), opts?)`
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
MUST agree with that reference document.

#### Scenario: Batch form does not require shorthand fields

- **WHEN** `efx.createMeshData({ surfaces: [surface, surface] })` is
  type-checked
- **THEN** it compiles without supplying top-level `positions` or other
  shorthand attributes

#### Scenario: Mixing construction forms is rejected

- **WHEN** `efx.createMeshData({ surfaces: [surface], positions })` combines
  the batch bag and the shorthand fields in one call
- **THEN** the type document reports a compile-time error

#### Scenario: drawMesh takes a positional mesh

- **WHEN** `efx.drawMesh(mesh, { transform, color, skinned })` and
  `efx.drawMesh(mesh)` are type-checked
- **THEN** both compile, and the former option bag holds only
  `transform`/`color`/`skinned` (a `mesh` field in the bag is rejected)

#### Scenario: Posing API is typed

- **WHEN** `efx.poseMesh(mesh, { clip: 'Walk', time: 1 })` and
  `efx.poseMesh(mesh, [{ clip: 0, time: 1, weight: 0.5 }])` are type-checked
- **THEN** both compile, the sample `clip` accepts a name or index, and an
  unknown sample field is rejected

#### Scenario: Primitive material option is typed

- **WHEN** `efx.makeCube({ size: 1, material })` is type-checked with a
  material object
- **THEN** it compiles and the material argument is accepted as a material
  object or `null`

#### Scenario: Type document agrees with the reference

- **WHEN** a script-facing API change updates `docs/js-api.md`
- **THEN** the same change updates `gallery/src/api/efx.d.ts` so every
  cataloged function has a matching declaration

### Requirement: Input namespace API

The script API SHALL expose keyboard and mouse input as sub-namespaces of the
single `efx` object — `efx.keyboard`, `efx.mouse`, and `efx.window` — with no
new free globals. Every entry SHALL be C-implemented and SHALL have identical
names, signatures, semantics, and error behavior across the desktop and web
bindings. Input exposes **no resource types**: it adds no native-backed class,
no `destroy()`, and no slot bank, so the native-backed class list and the
fixed-limits table are unchanged.

`efx.keyboard` SHALL provide:
- `isDown(key)`, `isPressed(key)`, `isReleased(key)` — current level, press
  edge, and release edge, each returning a boolean.
- `onDown(fn)`, `onUp(fn)`, `onChar(fn)` — register a callback and return an
  unsubscribe function.

`efx.mouse` SHALL provide:
- `isDown(button)`, `isPressed(button)`, `isReleased(button)` — level, press
  edge, and release edge, each returning a boolean.
- `onDown(fn)`, `onUp(fn)`, `onMove(fn)`, `onWheel(fn)` — register a callback
  and return an unsubscribe function.
- Read-only properties `position` → `[x, y]`, `x`, `y`, `delta` → `[dx, dy]`,
  and `wheel` → `[dx, dy]`, reported in surface pixels.

`efx.window` SHALL provide read-only properties `size` → `[width, height]`,
`width`, `height`, and `dpiScale`.

Event callbacks SHALL receive a single plain JS object — never a host or DOM
event — holding only engine-provided primitives: `{ key, repeat, mods }` for
keyboard down, `{ key, mods }` for keyboard up, `{ char }` for character
input, `{ button, x, y, mods }` for mouse down/up, `{ x, y, dx, dy }` for
mouse move, and `{ dx, dy }` for wheel. Key names and mouse button names SHALL
be engine-owned string constants from a documented set. Registration SHALL
require a function and SHALL throw `TypeError` otherwise; each registration
SHALL return an unsubscribe function with the same idempotent semantics as the
lifecycle hooks. A query with an unknown key or button name SHALL throw
`TypeError`. The namespaces SHALL be documented in `docs/js-api.md` and typed
in the gallery type document, both updated in the same change.

#### Scenario: Query and event are both available
- **WHEN** a script calls `efx.keyboard.isDown('space')` and registers `efx.keyboard.onDown`
- **THEN** the query reflects current state and the callback fires on key-down before the next update hook

#### Scenario: Unknown key or button rejected
- **WHEN** a script calls `efx.keyboard.isDown('notakey')` or `efx.mouse.isDown('side')`
- **THEN** the call throws `TypeError`

#### Scenario: Unsubscribe removes an input callback
- **WHEN** a script registers an input callback and calls the returned unsubscribe function
- **THEN** the callback no longer fires, and calling unsubscribe again is a no-op

#### Scenario: Event objects are plain data
- **WHEN** an input callback receives its argument
- **THEN** it is a plain JS object holding only engine-provided primitives, with no host/DOM object and no methods

#### Scenario: Mouse position is a read-only property
- **WHEN** a script reads `efx.mouse.position` or `efx.mouse.x`
- **THEN** it receives the current pointer position in surface pixels, and assigning to the property has no effect

#### Scenario: No resources added
- **WHEN** the API reference's resource classes and fixed limits are read after this change
- **THEN** they are unchanged: input adds no native-backed class, no `destroy()`, and no slot bank

### Requirement: Font and text API

The script API SHALL expose font loading, font creation, text drawing, and
text measurement as C-implemented members of the single `efx` namespace, with
identical names, signatures, semantics, and error behavior across the desktop
and web bindings:

- `efx.loadFontData(path)` → a native-backed `FontData` resource (the parsed
  font, no GPU resource), released by `destroy()`.
- `efx.createFont(fontData, opts)` → a native-backed `Font` that bakes a
  fixed glyph atlas at the requested size and optional baked outline/shadow
  effects; released by `destroy()`; read-only `size`, `lineHeight`, `ascent`,
  `descent`.
- `efx.drawText(text, font, x, y, opts?)` → lays out and draws the text as
  display-list quads and returns the laid-out bounds
  `{ width, height, lines }`.
- `efx.measureText(text, font, opts?)` → returns the same bounds without
  drawing.

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
- **THEN** it catalogs `loadFontData`, `createFont`, `drawText`, and `measureText`, each tagged C-implemented and F8, and does not catalog a `loadFont` convenience

#### Scenario: Text drawing is mid-level C
- **WHEN** a reviewer checks the layer of the text entries
- **THEN** `drawText` and `measureText` are tagged C-implemented mid-level functions, distinct from the pure-JS high-level layer

#### Scenario: No rich text or 3D text
- **WHEN** the API reference's font/text section is read
- **THEN** it documents wrapping and the four horizontal and three vertical alignment modes, and states that rich text, 3D text, and script-visible glyph metrics are not provided

#### Scenario: Type document is updated in the same change
- **WHEN** this change updates `docs/js-api.md`
- **THEN** `gallery/src/api/efx.d.ts` (and its type-test) declare `FontData`, `Font`, `drawText`, and `measureText` consistently with the reference

### Requirement: Billboard, sprite-batch, and particle API

The script API SHALL expose world-space billboard drawing, batched 2D sprite
drawing, and CPU particle systems as C-implemented members of the single `efx`
namespace, with identical names, signatures, semantics, and error behavior
across the desktop and web bindings:

- `efx.drawBillboard(pos, opts)` → records one world-space textured quad at a
  3D position, oriented by the engine from the recorded 3D camera. `opts`
  carries `texture`, `size`, `color`, `sourceRect`, `rotation`, `facing`
  (`'view'` default or `'y'`), and `depthTest`, per the `billboards`
  capability.
- `efx.drawSprites(texture, sprites)` → records one 2D textured quad per entry
  with `drawQuad` semantics, per the `2d-layer` capability.
- `efx.createParticleSystem(opts)` → a native-backed `ParticleSystem`.
- `efx.drawParticles(sys)` → records one particle batch for a live system.

`ParticleSystem` SHALL be a native-backed class exposing a read-only `count`,
a read-write `speedScale`, an `emit(n)` burst, `start`/`stop`/`pause`/`reset`,
a `set(opts)` partial reconfiguration, and `destroy()` with a GC finalizer
backstop. `drawSprites` SHALL be 2D-only; `drawBillboard` and `drawParticles`
SHALL use the 3D camera. The `js-api` reference (`docs/js-api.md`) and the
gallery type document (`gallery/src/api/efx.d.ts`) SHALL be updated in the same
change, and the particle pool limit and the `ParticleSystem` class SHALL be
reflected in the fixed-limits table and the native-backed class list.

#### Scenario: New entries are cataloged

- **WHEN** the API reference is read after this change
- **THEN** it catalogs `drawBillboard`, `drawSprites`, `createParticleSystem`,
  and `drawParticles`, each tagged C-implemented and F11

#### Scenario: Billboard is a 3D primitive

- **WHEN** a script calls `drawBillboard(pos, { texture })` under a 3D camera
- **THEN** the quad is placed and oriented in world space from the recorded 3D
  camera, with no camera state supplied by the script

#### Scenario: Sprite batch is 2D-only

- **WHEN** a script calls `drawSprites(tex, [{ x, y }])`
- **THEN** the sprites are recorded as 2D quads in the current 2D frame and are
  not depth-tested or 3D-oriented

#### Scenario: Particle system is exposed

- **WHEN** a script calls `createParticleSystem(opts)` and reads the returned
  object
- **THEN** it is a `ParticleSystem` with a read-only `count` and the documented
  methods, and `drawParticles` accepts it

#### Scenario: Reference and type document are updated

- **WHEN** this change updates `docs/js-api.md`
- **THEN** `gallery/src/api/efx.d.ts` declares `drawBillboard`, `drawSprites`,
  `ParticleSystem`, `createParticleSystem`, and `drawParticles` consistently
  with the reference

### Requirement: Physics namespace API

The script API SHALL expose collision, character movement, and linear impulse
dynamics through a single `efx.physics` sub-namespace of the `efx` object,
adding no free globals. Every entry SHALL be C-implemented and SHALL have
identical names, signatures, semantics, and error behavior across the desktop
and web bindings. The sub-namespace SHALL provide:

- world configuration: read/write `gravity` (a `[x, y, z]` vector, default
  `[0, -9.81, 0]`) and `iterations` (a positive integer, default `8`);
- `step(dt)` — advance the dynamic simulation by `dt` seconds (script-owned;
  the engine SHALL NOT step the world itself);
- `clear()` — remove every collider;
- `createBody(opts)`, `createCharacter(opts)`, and `createStaticMesh(mesh,
  opts?)` — resource factories returning native-backed classes;
- `raycast`, `overlap`, and `shapeCast` — spatial queries, defined by the
  `physics-queries` capability.

`Body` and `Character` SHALL be classified as native-backed classes under the
resource-classification requirement. `Body` SHALL expose `position`,
`velocity`, `contacts`, and a `transform` (a flat 16-number column-major
translation matrix, directly usable by `drawMesh`), `applyImpulse`, and
`applyForce`; `Character` SHALL expose `position`, `velocity`, `onFloor`, and
`moveAndSlide`. Shapes SHALL be plain JS option bags (sphere, box, capsule,
mesh) accepted by bodies and by queries. Validation SHALL follow the engine's
convention: unknown fields, unknown shape/body kinds, and wrong types throw
`TypeError`; out-of-range numeric values throw `RangeError`. The physics
classes SHALL be documented in `docs/js-api.md` and typed in the gallery type
document (`gallery/src/api/efx.d.ts`), both updated in the same change, with
their entries tagged with milestone F12. The `docs/js-api.md` resource,
lifecycle, and error sections SHALL cover the new sub-namespace, and the type
document SHALL reject invalid call shapes (unknown option fields, mixing
static/dynamic-only options).

#### Scenario: Physics namespace is reachable without setup

- **WHEN** a script calls `efx.physics.step(1/60)` and `efx.physics.raycast`
  without imports
- **THEN** both succeed on every target, and no additional free global exists

#### Scenario: World configuration is validated

- **WHEN** `efx.physics.gravity` is set to a non-3-number value or
  `efx.physics.iterations` to a non-positive integer
- **THEN** the assignment throws (`TypeError` for the shape, `RangeError` for
  the range) and the previous value remains in effect

#### Scenario: Factories return classified resources

- **WHEN** `createBody` and `createCharacter` are called with valid options
- **THEN** they return `Body` and `Character` instances whose class entries in
  the reference state the native-backed classification and the documented
  read-only properties

#### Scenario: Body transform feeds drawMesh

- **WHEN** a dynamic body's `transform` is passed as the `transform` option of
  `drawMesh`
- **THEN** the mesh draws at the body's world position

#### Scenario: Reference and type document are updated

- **WHEN** this change lands
- **THEN** `docs/js-api.md` catalogs every `efx.physics` entry and the
  `Body`/`Character` classes with their signatures, layer tags, and the F12
  milestone, and `gallery/src/api/efx.d.ts` declares the same surface,
  including the new native-backed classes

#### Scenario: Invalid options are rejected

- **WHEN** `createBody` receives an unknown option field or a shape of unknown
  type
- **THEN** the call throws `TypeError` and creates nothing

### Requirement: Gamepad namespace API

The script API SHALL expose gamepad input as the sub-namespace `efx.gamepad`
of the single `efx` object, with no new free globals. Every entry SHALL be
C-implemented and SHALL have identical names, signatures, semantics, and error
behavior across the desktop and web bindings. Gamepad exposes **no resource
types**: pads are a fixed, engine-owned bank reported by index; scripts SHALL
NOT create, destroy, or own a pad, and the native-backed class list is
unchanged.

`efx.gamepad` SHALL provide:
- `count` — the number of connected pads.
- `get(index)` — the pad view for slot `index`, or `null` when none is
  connected.
- `onConnect(fn)` and `onDisconnect(fn)` — register a callback receiving the
  pad view and return an unsubscribe function.

A pad view SHALL provide:
- `connected` — whether the slot currently has a pad.
- `name` — the device name.
- `mapped` — whether a semantic mapping was found.
- `isDown(button)`, `isPressed(button)`, `isReleased(button)` — level, press
  edge, and release edge for a semantic button name, returning a boolean.
- `axis(axis)` — the normalized value of a semantic axis name.
- `rawButton(i)` and `rawAxis(i)` — the raw device values by index, for
  unmapped pads.

Semantic button names SHALL be the engine-owned set `south`, `east`, `west`,
`north`, `leftShoulder`, `rightShoulder`, `leftTrigger`, `rightTrigger`,
`back`, `start`, `guide`, `leftStick`, `rightStick`, `dpadUp`, `dpadDown`,
`dpadLeft`, `dpadRight`. Semantic axis names SHALL be `leftX`, `leftY`,
`rightX`, `rightY`, `leftTrigger`, `rightTrigger`. Axis values SHALL use one
canonical documented range on every target, and each trigger's digital button
SHALL derive from its axis using a documented threshold. A query with an
unknown button or axis name SHALL throw `TypeError`; registration SHALL
require a function and SHALL throw `TypeError` otherwise, and each
registration SHALL return an unsubscribe function with the same idempotent
semantics as the lifecycle hooks. Event callbacks SHALL receive a single
plain JS object — never a host or DOM object. The namespace SHALL be
documented in `docs/js-api.md` and typed in the gallery type document, both
updated in the same change.

#### Scenario: Pad queries and events are available
- **WHEN** a script reads `efx.gamepad.count`, calls `efx.gamepad.get(0).axis('leftX')`, and registers `efx.gamepad.onConnect`
- **THEN** the queries reflect current state and the callback fires when a pad connects

#### Scenario: Unknown semantic name is rejected
- **WHEN** a script calls `pad.isDown('notabutton')` or `pad.axis('leftZ')`
- **THEN** the call throws `TypeError`

#### Scenario: Unsubscribe removes a gamepad callback
- **WHEN** a script registers a connect or disconnect callback and calls the returned unsubscribe function
- **THEN** the callback no longer fires, and calling unsubscribe again is a no-op

#### Scenario: Event objects are plain data
- **WHEN** a gamepad callback receives its argument
- **THEN** it is a plain JS object holding only engine-provided primitives, with no host/DOM object and no methods

#### Scenario: No resources added
- **WHEN** the API reference's resource classes and fixed limits are read after this change
- **THEN** pads are a fixed engine-owned bank reported by index with no `create`, no `destroy`, and no new native-backed class

### Requirement: Audio namespace API

The script API SHALL expose audio playback as the sub-namespace `efx.audio` of
the single `efx` object, with no new free globals. Loading SHALL be separate
from playback (load first, then play). The author-facing entry points SHALL let
a script load a static or streamed source, start it through one playback verb,
and control the running playback through a handle — with no channel, bus,
buffer, or voice-allocation argument. Every entry SHALL have identical names,
signatures, semantics, and error behavior across the desktop and web bindings.

`efx.audio` SHALL provide:
- `loadAudioData(path)` — decode a WAV or MP3 resource into a fully-decoded
  `AudioData`.
- `loadAudioStream(path)` — open a WAV or MP3 resource as a streamed
  `AudioStream` that decodes incrementally.
- `playAudio(source, opts?)` — start an `AudioData` or `AudioStream` and
  return an `Audio` handle; `opts` MAY include `volume`, `pan`, `pitch`, and
  `loop` as initial values only.
- `volume` — a read-write master output gain scaling all playback.
- `resume()` — unlock/resume audio after a user gesture on web; a no-op on
  desktop.

An `AudioData` SHALL be a native-backed class holding decoded PCM with an
explicit `destroy()` and a GC-finalizer backstop, and no query properties. An
`AudioStream` SHALL be a native-backed class holding a compressed resource and
an incremental decoder with an explicit `destroy()` and no query properties;
starting it SHALL create a playhead independent of other playheads of the same
stream. An `Audio` SHALL be a native-backed playback handle with a read-only
`playing` and `paused`, read-write `volume`, `pan`, `pitch`, and `loop`, and
`stop()`, `pause()`, `resume()`, and `destroy()`. `playAudio` SHALL accept only
an `AudioData` or `AudioStream`; passing anything else SHALL throw `TypeError`.
A load of a malformed or unsupported resource SHALL throw `Error`; a bad
argument type SHALL throw `TypeError`; a negative `volume` or master gain SHALL
throw `RangeError`. The namespace SHALL be documented in `docs/js-api.md` and
typed in the gallery type document, both updated in the same change.

#### Scenario: Author plays music and effects

- **WHEN** a script calls `efx.audio.playAudio(efx.audio.loadAudioData('hit.wav'))` and `efx.audio.playAudio(efx.audio.loadAudioStream('theme.mp3'), { loop: true })`
- **THEN** the static sound and the streamed track play together, with no channel, bus, or voice argument from the script

#### Scenario: Handles control playback

- **WHEN** a script changes `volume`, `pitch`, `pan`, or `loop` on an `Audio` handle, or calls `pause()`/`resume()`/`stop()`
- **THEN** the change takes effect on that playback and the handle's read-only `playing`/`paused` reflect reality

#### Scenario: Same static source plays overlapping handles

- **WHEN** a script calls `playAudio` twice with the same `AudioData`
- **THEN** two independent `Audio` handles play simultaneously

#### Scenario: Master gain controls all playback

- **WHEN** a script sets `efx.audio.volume`
- **THEN** all playing sources scale to that gain

#### Scenario: Bad arguments are rejected

- **WHEN** a script passes a non-string path, a non-source to `playAudio`, or an invalid option type
- **THEN** the call throws `TypeError`, loading a malformed resource throws `Error`, and a negative volume or master gain throws `RangeError`

#### Scenario: Audio resource classes are native-backed

- **WHEN** the API reference's resource classes are read after this change
- **THEN** `AudioData`, `AudioStream`, and `Audio` are listed as native-backed classes with `destroy()`, and their documented read-only and read-write properties match the implementation

### Requirement: Identical argument errors on every runtime
For every engine API call that throws because an argument is invalid, or
because a requested resource cannot be read or decoded, the desktop runtime
and the web runtime SHALL throw an error of the same class with the identical
message text. A number-typed argument or option field SHALL accept only values
whose type is number. Any other type — including strings that contain digits
and booleans — SHALL throw `TypeError`. Non-finite numbers and out-of-range
values keep their documented error class.

#### Scenario: Same message on both runtimes
- **WHEN** the same script performs an invalid call (for example
  `efx.setLight(7, { … })` with a slot outside 0..3) on the desktop player and
  in the web player
- **THEN** both throw an error of the same class whose `message` is
  byte-identical

#### Scenario: Missing resource reports identically
- **WHEN** a script loads an image, glTF asset or audio file that does not
  exist in the resource root, on each runtime
- **THEN** both runtimes throw the same error class with the identical message

#### Scenario: Numeric string is rejected everywhere
- **WHEN** a script passes the string `'0.5'` where a number-typed option is
  expected (for example a physics body's `mass`)
- **THEN** both runtimes throw `TypeError` and no resource is created

#### Scenario: Error catalog has no known divergences
- **WHEN** the error-catalog script is run through both runtimes and the
  outputs are compared
- **THEN** the outputs are byte-identical and the catalog lists no known
  divergences
