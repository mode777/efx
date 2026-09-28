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
RenderTarget's `width` and `height` delivered by F5a, and Font's `size`,
`lineHeight`, `ascent`, and `descent` delivered by F8), or slot-based
(a fixed pre-allocated bank of indexed resources).
The native-backed classes SHALL be exactly: MeshData, ImageData, Mesh,
Texture, RenderTarget, FontData, and Font; skins, skeletons, and animation
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
`destroy()`. Every resource requiring
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
1 camera, 16 surfaces per mesh (F3), and a post-effect chain of at most
8 entries (F5b); lights are the only slot bank.

#### Scenario: Fixed limits stated

- **WHEN** the reference document's limits section is read
- **THEN** it states 4 point lights, 1 directional light, 1 camera, 16 surfaces per mesh, and the 8-entry post-effect chain cap, matching vision.md and the 3d-core and post-fx capabilities

#### Scenario: Unreleased native resource is reclaimed

- **WHEN** a script creates textures in a loop and never calls `destroy()` on them
- **THEN** the native sizes drive GC pressure, finalizers reclaim the objects within roughly a frame of them becoming unreachable, and nothing leaks at runtime shutdown

#### Scenario: Destroyed resource is safe

- **WHEN** a script calls `destroy()` on a resource that the display list recorded earlier in the same frame
- **THEN** the native release is deferred until playback completes, and subsequent use of the destroyed resource throws

#### Scenario: Query properties are documented per class

- **WHEN** the reference document's native-backed class entries are read
- **THEN** the Texture and RenderTarget entries list the read-only `width` and `height`, the MeshData and Mesh entries list the read-only `surfaceCount`, the Font entry lists the read-only `size`, `lineHeight`, `ascent`, and `descent`, the FontData entry lists none, and every other entry states that it has none

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

#### Scenario: Resource without a classification

- **WHEN** a change proposes exposing a new resource type to scripts without classifying it as JS-managed, native-backed class, or slot-based
- **THEN** the change is incomplete and MUST NOT update the API reference

## ADDED Requirements

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
