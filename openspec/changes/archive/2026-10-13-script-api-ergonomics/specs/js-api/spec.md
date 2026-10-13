# Spec Delta: js-api

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
`screenRay`, `setBlendMode`, `setCamera2D`, `setCamera3D`, `setClearColor`,
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
`keyboard`, `mouse`, `window`, `physics`, `gamepad`, `audio`, `time`);
`whiteTexture`, `loadText`, and the math helpers are no longer root members.
The sub-namespace
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

### Requirement: Math namespace API
The script API SHALL expose the math helpers as the sub-namespace
`efx.math` of the single `efx` object: `efx.math.mat4`, `efx.math.vec3`,
`efx.math.quat`, and the scalar helpers `clamp` and `lerp`, with no new free
globals. The pre-existing members SHALL behave identically to the former
root helpers (`efx.mat4` / `efx.vec3` / `efx.quat`): pure functions that
never mutate their arguments and return plain
JS data. `clamp(v, lo, hi)` and `lerp(a, b, t)` are `[JS]`-implemented
scalar helpers; `mat4.transform(pos, yawDeg?, pitchDeg?, rollDeg?, scale?)`
is native-backed through the standard marshal-only wire with prelude
validation (ADR 0049) and returns the same plain JS data as its
`[JS]` siblings, byte-identically on both runtimes. The names `mat4`, `vec3`,
and `quat` SHALL NOT also exist as members
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
- `createBody(shape, opts?)`, `createCharacter(radius, height, opts?)`, and
  `createStaticMesh(mesh, opts?)` — resource factories returning native-backed
  classes; the required collider shape / capsule dimensions are positional and
  the trailing bag carries only optional configuration;
- `raycast`, `overlap`, and `shapeCast` — spatial queries, defined by the
  `physics-queries` capability;
- `debugDraw(opts?)` — render-phase debug visualization of every live
  collider, defined by the `collision-physics` capability.

`Body` and `Character` SHALL be classified as native-backed classes under the
resource-classification requirement. `Body` SHALL expose `position`,
`velocity`, `contacts`, and a `transform` (a flat 16-number column-major
translation matrix, directly usable by `drawMesh`), `applyImpulse`,
`applyForce`, and `setPosition(p)` (teleport, defined by the
`collision-physics` capability); `Character` SHALL expose `position`,
`velocity`, `onFloor`, `moveAndSlide`, and `setPosition(p)` with the same
teleport semantics. Shapes SHALL be plain JS option objects (sphere, box, capsule,
mesh) accepted positionally by bodies and by queries. Validation SHALL follow
the engine's convention: unknown fields, unknown shape/body kinds, and wrong
types throw `TypeError`; out-of-range numeric values throw `RangeError`. The
physics classes SHALL be documented in `docs/js-api.md` and typed in the
gallery type document (`gallery/src/api/efx.d.ts`), both updated in the same
change, with their entries tagged with milestone F12. The `docs/js-api.md`
resource, lifecycle, and error sections SHALL cover the new sub-namespace, and
the type document SHALL reject invalid call shapes (unknown option fields,
mixing static/dynamic-only options).

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

- **WHEN** `createBody(shape, opts?)` and `createCharacter(radius, height, opts?)` are called with valid required inputs
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
  and `wheel` → `[dx, dy]`; position and deltas are reported in **frame
  pixels** (the active 2D frame's coordinate space, identity when no frame
  was ever set — the same space quad drawing uses), while `wheel` remains
  unscaled scroll ticks.

`efx.window` SHALL provide read-only properties `size` → `[width, height]`,
`width`, `height`, and `dpiScale`, reported in surface pixels.

Event callbacks SHALL receive a single plain JS object — never a host or DOM
event — holding only engine-provided primitives: `{ key, repeat, mods }` for
keyboard down, `{ key, mods }` for keyboard up, `{ char }` for character
input, `{ button, x, y, mods }` for mouse down/up, `{ x, y, dx, dy }` for
mouse move, and `{ dx, dy }` for wheel; mouse event coordinates and deltas
use the same frame-pixel space as the read-only properties. Key names and
mouse button names SHALL
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
- **THEN** it receives the current pointer position in frame pixels (surface
  pixels when no 2D frame was ever set), and assigning to the property has no
  effect

#### Scenario: No resources added
- **WHEN** the API reference's resource classes and fixed limits are read after this change
- **THEN** they are unchanged: input adds no native-backed class, no `destroy()`, and no slot bank

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

There SHALL be no namespace-level `resume()` or unlock member: web autoplay
unlock is engine-managed on first input (see the `audio` capability). An
`AudioData` SHALL be a native-backed class holding decoded PCM with an
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

## ADDED Requirements

### Requirement: Color value acceptance
Wherever the script API types a value as a `Color` — draw-call tints, material
channel colors, light colors, clear colors, and the like — the engine SHALL
accept both a 4-component `[r, g, b, a]` array and a 3-component `[r, g, b]`
array; the 3-component form SHALL be treated as fully opaque (alpha `1`).
This tolerance SHALL apply uniformly at the validation layer (shared wire
validators per ADR 0049), so every color-typed argument across the API gains
it at once; all other invalid color values (wrong types, wrong lengths,
non-finite components) keep their existing error classes and messages.
The gallery type document SHALL widen the `Color` type accordingly, and the
generated reference SHALL document the accepted forms.

#### Scenario: Three-component tint is accepted

- **WHEN** a script draws `drawQuad(tex, x, y, { color: [1, 0.5, 0] })`
- **THEN** the quad renders with that tint at full opacity, identically to
  passing `[1, 0.5, 0, 1]`

#### Scenario: Tolerance is uniform across the API

- **WHEN** a 3-component color is passed to a draw-call tint, a material
  channel color, a light color, and the clear color
- **THEN** every call accepts it as opaque, with no per-call exceptions

#### Scenario: Invalid colors keep their errors

- **WHEN** a script passes `[1, 0]` (wrong length), `'red'`, or a `NaN`
  component as a color, on both runtimes
- **THEN** the calls throw their existing error classes with identical
  messages on both runtimes
