# Spec Delta

## MODIFIED Requirements

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
Texture, RenderTarget, FontData, Font, ParticleSystem, Body, and Character;
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
- **THEN** the Texture and RenderTarget entries list the read-only `width` and `height`, the MeshData and Mesh entries list the read-only `surfaceCount`, the Font entry lists the read-only `size`, `lineHeight`, `ascent`, and `descent`, the ParticleSystem entry lists the read-only `count`, the Body entry lists `position`, `velocity`, `contacts`, and `transform`, the Character entry lists `position`, `velocity`, and `onFloor`, the FontData entry lists none, and every other entry states that it has none

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

#### Scenario: Physics classes are classified native-backed

- **WHEN** the reference document's physics entries are read
- **THEN** `Body` and `Character` are stated to be native-backed classes with an idempotent `destroy()` and a GC-finalizer backstop, and the fixed-limits table is unchanged (physics uses dynamic allocation with a documented soft guidance, not a fixed cap)

## ADDED Requirements

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
