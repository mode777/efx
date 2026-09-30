# Spec Delta

## MODIFIED Requirements

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
