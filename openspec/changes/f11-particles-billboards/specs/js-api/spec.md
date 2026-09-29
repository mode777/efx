# Spec Delta

## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Resource classification and fixed limits

Every engine resource type that scripts can create or reference SHALL be
classified in the reference as exactly one of: JS-managed (plain script
objects, garbage collected), native-backed class (an opaque JS object
wrapping a native handle with an explicit `destroy()` release method; such
a class MAY additionally expose documented read-only query properties, which
MUST be listed in the reference — the instances are Texture's `width` and
`height`, MeshData's and Mesh's read-only `surfaceCount` delivered by F3,
RenderTarget's `width` and `height` delivered by F5a, and Font's `size`,
`lineHeight`, `ascent`, and `descent` delivered by F8, and ParticleSystem's
`count` delivered by F11), or slot-based
(a fixed pre-allocated bank of indexed resources).
The native-backed classes SHALL be exactly: MeshData, ImageData, Mesh,
Texture, RenderTarget, FontData, Font, and ParticleSystem; skins, skeletons,
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
8 entries (F5b), and at most 65536 particles per particle system (F11);
lights are the only slot bank.

#### Scenario: Fixed limits stated

- **WHEN** the reference document's limits section is read
- **THEN** it states 4 point lights, 1 directional light, 1 camera, 16 surfaces per mesh, the 8-entry post-effect chain cap, and the 65536-particle system cap, matching vision.md and the 3d-core, post-fx, and particles capabilities

#### Scenario: Unreleased native resource is reclaimed

- **WHEN** a script creates textures in a loop and never calls `destroy()` on them
- **THEN** the native sizes drive GC pressure, finalizers reclaim the objects within roughly a frame of them becoming unreachable, and nothing leaks at runtime shutdown

#### Scenario: Destroyed resource is safe

- **WHEN** a script calls `destroy()` on a resource that the display list recorded earlier in the same frame
- **THEN** the native release is deferred until playback completes, and subsequent use of the destroyed resource throws

#### Scenario: Query properties are documented per class

- **WHEN** the reference document's native-backed class entries are read
- **THEN** the Texture and RenderTarget entries list the read-only `width` and `height`, the MeshData and Mesh entries list the read-only `surfaceCount`, the Font entry lists the read-only `size`, `lineHeight`, `ascent`, and `descent`, the ParticleSystem entry lists the read-only `count`, the FontData entry lists none, and every other entry states that it has none

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

#### Scenario: Resource without a classification

- **WHEN** a change proposes exposing a new resource type to scripts without classifying it as JS-managed, native-backed class, or slot-based
- **THEN** the change is incomplete and MUST NOT update the API reference

### Requirement: Normative API reference document

The project SHALL maintain `docs/js-api.md` as the normative, developer-facing
reference of the entire script API. It SHALL contain an entry for every public
API function with a signature sketch, a description, its layer tag, and the
roadmap milestone (F1–F11) that delivers it. Entries for functions whose
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
exports, and the unchanged no-browser/Node-dependency rule. Any change that
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

#### Scenario: Catalog derived from vision

- **WHEN** the document's function catalog is checked against vision.md
- **THEN** every capability vision.md names for the consumer API (2D quads,
  meshes, vertex colors, cameras, lights, Phong materials with maps, alpha
  masks, blending modes, render targets, post FX, resource loading,
  keyboard/mouse input query and events, script modules, skinning/animation,
  high-level model and text drawing) has a corresponding catalog entry or an
  explicitly noted open question
