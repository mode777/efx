# EFX JavaScript API — design guidelines

**Role.** This document defines how the script-facing API is designed and
extended: the conventions, layering, resource model, lifecycle, and error
semantics every API addition must follow. It is **not** a per-function
catalog.

**Where the reference lives.** The complete per-symbol reference is generated
from the TypeScript declaration `gallery/src/api/efx.d.ts`. Its committed
Markdown rendering is [`docs/api/`](api/README.md) and its HTML rendering is
published on the gallery site under `/api`. Never hand-write per-function
catalog entries here — update the declaration and regenerate the reference.

**How the reference is organized.** The declaration is also the source of the
reference's information architecture, through TSDoc tags and the pinned
TypeDoc config:

- The reference leads with the `efx` global (`@group Start Here`) and groups
  the rest of the surface into domains (`Graphics`, `Math`, `Input`,
  `Physics`, `Audio`, `IO`, `Values`, `Configuration`, `Events`, `Enums`,
  `Results`); `groupOrder` in `gallery/typedoc.json` fixes the order and
  `gallery/typedoc.markdown.json` renders each group as a table. Every new
  top-level symbol carries a `@group`.
- A **single-use, per-operation option bag** is tagged `@hidden @inline`:
  it gets no index entry or page, and its fields render inline in the
  operation that accepts it. Reusable value records and the configuration
  bags that are referenced from more than one place keep their own pages.
- The `Efx` interface is `@hidden @inline`; the `efx` variable page expands
  it. New symbols reference `efx`'s members (for example `EfxGraphics`),
  never the hidden `Efx` interface.
- Module-scoped authoring facilities (`require`, `module`, `exports`,
  `__dirname`, `__filename`) are `@hidden`: they exist for the type checker,
  not the public reference.
- A curated landing page (`gallery/src/api/README.md`) is merged as the root
  of both renderings. It introduces the API; it is not a per-symbol catalog.

See `vision.md` for product goals and `openspec/specs/` for required
behavior.

## Overview

- **One namespace.** All engine-provided functions — C-implemented and
  pure-JS high-level — live on a single global object `efx`, available to
  every script without imports or setup. Scripts reach engine functionality
  only through `efx` and standard ES6 built-ins. The namespace is organized
  into domain sub-namespaces (ADR 0050, ADR 0051): graphics drawing, state,
  and resources live under `efx.graphics`, the pure-JS math helpers under
  `efx.math`, the resource loaders under `efx.io`, the named color constants
  under `efx.color`, and further domains such as `efx.keyboard`,
  `efx.physics`, and `efx.gamepad` are members of that one object;
  sub-namespaces add no free globals, and the root keeps only the
  runtime/lifecycle facilities (`log`, `quit`, the read-only `args`
  property, and hook registration). The CommonJS facilities `require`,
  `module`, and `exports` are module-scoped authoring facilities, never
  members of `efx` and never free globals.
- **Two layers.** Every API function belongs to exactly one of two layers:
  - `[C]` — low/mid-level functions implemented in C/C++ and registered
    through the engine binding (`drawQuad`, `drawMesh`,
    `setLight`, `drawText`, …).
  - `[JS]` — high-level conveniences implemented in pure ES6
    (`makeCube`/`makePlane`/`makeSphere`/`makeCapsule`, the `efx.math.mat4` /
    `efx.math.vec3` / `efx.math.quat` helpers, the `efx.color` constants). A
    `[JS]` function MUST be built only on
    the public `[C]` API and standard ES6 — never on private bindings or host
    facilities.
  The layer is an implementation concern: the generated reference is
  end-user facing and does not tag entries by layer.
- **Shared argument handling (ADR 0049).** A `[C]` function MAY validate and
  normalize its option bag in the engine-bundled pure-ES6 layer
  (`src/prelude/prelude.js`) before its C implementation runs. That argument
  handling is written **once** and shared by every runtime; it reaches the
  native implementation only through an engine-internal binding object passed
  to the prelude wrapper, which is neither a member of `efx` nor a global and
  is never callable by scripts on its own. Such a function remains a `[C]`
  function: its observable behavior, defaults, and errors are unchanged.
  Hot per-frame draw and query calls (`drawQuad`, `drawSprites`,
  `drawBillboard`, `drawMesh`, `drawText`/`Font.measure`, the input queries)
  keep their native validation.
- **Runtime binding per platform.** Every `[C]` entry is implemented once in
  C and exposed through the platform's binding — on desktop the embedded
  quickjs binding, on Emscripten the native bridge to the page's JS engine.
  Names, signatures, semantics, errors, and resource lifecycle are identical
  across both bindings: the same script text runs on every target, and game
  scripts never see host globals (`window`, `document`, `process`, …).
- **Immediate mode, deferred rendering.** `draw*` calls record into an
  internal display list that the renderer may re-order; there is no explicit
  flush in the API.

## Conventions

- **Naming.** camelCase, verb-first. `set*`/`get*` configure engine state,
  `draw*` record into the display list, `make*` build data in JS,
  `load*`/`create*` fetch or upload resources and return resource objects.
  An operation whose **subject is a native-backed class instance** is a
  **method on that class** — `mesh.pose(pose)`,
  `mesh.setSurfaceMaterial(surfaceIndex, mat)`, `font.measure(text, opts?)` —
  not a free `efx.graphics` function that re-takes the instance as its first
  argument (ADR 0055). `efx.graphics` holds constructors, factories, and
  stateless operations.
- **Render state vs per-object options.** A `set*` setter configures the
  **frame-local** default for draws that do not carry their own value; a draw
  option or material field **overrides** it for that object only. The engine
  resets such a default at the start of each frame, so a script must set it
  inside the render hook (or pass the override). `setBlendMode` is the blend
  render state: it applies to quads and sprite batches, mesh surfaces whose
  material has no `blend`, and billboards, and is overridden by
  `DrawQuadOptions.blend`, `DrawSpritesOptions.blend`, `Material.blend`, and
  `DrawBillboardOptions.blend`. A particle system with no configured `blend`
  inherits the state at `drawParticles` time. Clear color, cameras, and lights
  remain plain state that persists until changed.
- **Material appearance flags.** `Material.unlit` (default `false`) bypasses
  the lighting equation: the surface shows its `diffuse` color × its `diffuse`
  map × the albedo with no light contribution, while `alphaMask` and `blend`
  still apply; it is snapshotted with the rest of the material. `DrawMeshOptions.depthWrite`
  (default `true`) is per-draw render state: `false` depth-tests without
  writing depth, so a camera-locked sky recorded first is neither occluded by
  nor occluding the scene. The `makeCube`/`makeSphere` `inverted` flag builds
  inward-facing geometry for such skies.
- **Resources.** Loaders and creators return opaque resource objects (see
  [Resource & memory model](#resource--memory-model)); `res.destroy()`
  releases deterministically and GC is the backstop. Native-backed classes
  are otherwise fully opaque except for documented read-only query
  properties and the documented operations that are methods on the class
  (ADR 0055).
- **Parameters.** One rule governs every public function: **required inputs
  are positional arguments and all optional inputs are fields of one trailing
  options bag**. A call with no optional inputs omits the bag entirely. Every
  field of an options bag is optional, and a bag never carries a required
  input. A lone optional input MAY be passed positionally instead of in a bag.
  Required fields are permitted inside a **record/value** argument — a shape,
  a light descriptor, a pose sample, a source rectangle, or a batch element —
  because such an argument is itself a required positional value, not the
  function's options bag. Argument order is consistent: the subject (the thing
  drawn, created, or queried) first, then required resources/selectors, then
  required scalars (2D `x`, `y`) or vectors (3D position), then the options
  bag — e.g. `drawQuad(texture, x, y, opts?)`, `createBody(shape, opts?)`,
  `createFont(fontData, size, opts?)`, `createParticleSystem(texture, max,
  lifetime, opts?)`. Optionality is explicit:
  - a `?` on the bag itself (`opts?`) means the whole object may be omitted —
    every field then takes its documented default;
  - every field of an options bag is optional;
  - inside a record/value argument, an unmarked field is required and a `?`
    field is optional (optional only when a default is documented).
- **Option-object validation.** A missing or wrongly-typed required field
  throws `TypeError`; unknown fields throw `TypeError` (typo protection).
  A number-typed option field accepts **only** values whose type is number —
  numeric strings and booleans throw `TypeError` on every runtime (ADR 0049);
  non-finite numbers and out-of-range values keep their documented error
  class. Every runtime throws the **same error class with the same message**
  for the same invalid call or failed load. Bags documented as "null
  disables" (e.g. post FX) accept `null` as an explicit off switch. A call
  that validates a list validates **eagerly and atomically**: on throw, the
  previous state is unchanged.
- **Units.** Angles in **degrees** (radians never appear in the API), time in
  **seconds**, positions and sizes in world units.
- **Colors.** `[r, g, b, a]` arrays of normalized floats in `0..1`
  (e.g. `[1, 0.5, 0, 1]`). Channel alpha is ignored by shading. The
  `efx.color` namespace provides frozen named constants — the CSS basic 16
  plus `transparent` — for the common colors; they are ordinary `Color`
  values usable anywhere a color array is accepted.
- **Errors.** Invalid input throws standard ES6 errors (`TypeError` for wrong
  types, `RangeError` for out-of-range slots/indices). An uncaught exception
  stops the run with a non-zero exit code and the error on stderr — in
  `--script` mode and in both frame hooks alike.
- **Dependencies.** Scripts use only the `efx` namespace and standard ES6.
  Browser and Node.js APIs are unavailable — not even transitively.

## Lifecycle model

The entry script is `main.js` at the resource root (a directory or zip).
**Loading `main.js` is the implicit init**: the `efx` namespace and every
engine function are ready before the script executes, and in run modes with a
rendering surface the window, GPU context, and engine pipelines are
initialized first — so top-level code may create or sample engine-owned GPU
resources such as `efx.graphics.whiteTexture`. (`--script` and the web Node
harness run without a rendering surface, by design.) Top-level code is where
setup happens. There is no separate `init()` hook.

Frame callbacks are registered explicitly and stack in registration order:

```js
const offUpdate = efx.registerUpdateHook(dt => { /* dt: seconds */ });
const offRender = efx.registerRenderHook(() => { /* ... */ });
offUpdate(); // optional unsubscribe
```

- Both functions require a function argument; anything else throws
  `TypeError`. Each returns an **unsubscribe function**; calling it removes
  that registration and is idempotent.
- Update hooks run before render hooks, once per frame, in registration
  order. An exception in any hook (or at load time) stops the run and exits
  non-zero.
- **Global `update`/`render` remain supported as load-time sugar:** if the
  script defines them, the engine registers them after evaluation in load
  order, so they run after any hooks registered during evaluation. The entry
  is evaluated as a CommonJS module, so `module.exports.update` /
  `module.exports.render` are accepted as the module-shaped equivalent, and a
  hook present in both forms is registered once.

## Script modules model (CommonJS)

Every script file under the resource root is a **CommonJS module**, and the
entry `main.js` is itself a module. `require(path)` loads a module
**synchronously** through the resource provider (directory or zip) and returns
its `module.exports`; it never returns a promise. Evaluating a module exposes
`require`, `module`, and `exports` to that module's own scope (`exports`
initially aliases `module.exports`).

- **Resolution.** A specifier is relative to the requiring module (`./`,
  `../`) or root-relative (e.g. `lib/math.js`). Resolution tries the exact
  path and then a deterministic `.js` fallback; `.json` files load as parsed
  JSON modules. Paths use forward slashes and obey the resource root's escape
  rules: a specifier that normalizes outside the root fails and reads nothing
  outside it. Bare package names (`lodash`), `node_modules`, `package.json`,
  directory-index resolution, and native addons are **not** supported and
  fail loudly rather than guessing.
- **Caching and cycles.** A module is cached by its resolved path: repeated
  `require`s return the same `module.exports` object and the body runs once. A
  circular require receives the other module's partially populated `exports`
  instead of recursing.
- **`__esModule` interop.** The `module.exports`/`exports` alias and the
  `__esModule` marker are honored so the TypeScript/Babel `commonjs` transform
  loads correctly.
- **JSON modules.** `require('data/config.json')` returns the parsed JSON
  value and participates in the module cache; invalid JSON throws.
- **Entry hooks.** Functions defined at the entry module's top level (or on
  `module.exports`) are registered after evaluation, in load order and after
  any explicitly registered hooks; a hook present in both forms registers
  once.
- **Unsupported forms.** Static `import`/`export`, dynamic `import()`, and
  `import.meta` are not executed by the engine — they fail. ESM is a
  **source** format: TypeScript compiles `import`/`export` to CommonJS
  (`module: commonjs`, `target: es2015+`, `esModuleInterop: true`,
  `verbatimModuleSyntax: true`) before packaging, and the engine executes the
  emitted `require` form.
- **No Node/npm environment.** Node built-ins (`fs`, `path`, `process`,
  `Buffer`, …) are not provided, host globals stay shadowed on web, and
  npm-package compatibility is an explicit non-goal.

## Resource & memory model

Every resource type scripts can create or reference is classified exactly one
way — the rule that keeps a GC'd language from leaking unmanaged memory.
Dynamic-count resource types are opaque **native-backed classes**; only the
fixed light bank is slot-based.

| Class | Meaning | Release path |
|---|---|---|
| **JS-managed** | Plain data objects; garbage collected | Drop the reference |
| **Native-backed class** | Opaque object wrapping a native handle — read-only query properties only where documented; GC finalizer backstop | `res.destroy()` (primary), GC / shutdown (backstop) |
| **Slot-based** | Fixed pre-allocated bank of indexed resources | Overwrite the slot |

| Resource | Contents | Class | Side | Notes |
|---|---|---|---|---|
| MeshData | 1..16 surfaces, each with its own attribute arrays + optional indices (Godot surface / glTF primitive); skinned meshes add `joints`/`weights` per surface | Native class | CPU | `createMeshData` / `loadMeshData`; read-only `surfaceCount` |
| ImageData | Raw pixels + size + format | Native class | CPU | `createImageData` / `loadImage`; read-only `width` / `height` (throw `TypeError` when destroyed) |
| Mesh | GPU mesh (all surfaces uploaded); skinned meshes carry the skeleton and clips internally; per-surface material binding slot | Native class | GPU | `createMesh(meshData)`; read-only `surfaceCount`; `pose(pose)` and `setSurfaceMaterial(surfaceIndex, mat)` methods |
| Texture | GPU texture | Native class | GPU | `createTexture(imageData, opts?)` (`wrap`/`filter`/`mipmaps`); read-only `width` / `height`; `efx.graphics.whiteTexture` is an engine-owned instance (destroy throws) |
| RenderTarget | GPU render target (color + depth attachments, env-default formats) | Native class | GPU | `createRenderTarget(width, height)` (1..4096 per side); read-only `width` / `height`; a live RenderTarget is accepted **wherever a live Texture is** — `drawQuad`, material `map`s, `alphaMask` — with no alias Texture object |
| Materials (Phong parameter objects) | — | JS-managed | — | Bound per surface via the `Mesh.setSurfaceMaterial` method / the `materials` array; per-channel `map`s and `alphaMask` reference native-backed `Texture`s the engine retains while bound; `unlit: true` bypasses lighting |
| Post-effect chain entries | `{ effect, ...options, mix? }` option bags | JS-managed | — | Snapshotted at `setPostEffects` call time; no native handle and no `destroy()` |
| FontData | Parsed TrueType/OpenType font (CPU, no GPU resource) | Native class | CPU | `loadFontData(path)`; no query properties |
| Font | Fixed baked glyph atlas (RGBA8 Texture) + layout metrics | Native class | GPU | `createFont(fontData, size, opts?)`; read-only `size`/`lineHeight`/`ascent`/`descent`; `measure(text, opts?)` method |
| ParticleSystem | CPU-simulated pool + emitter configuration (engine-owned) | Native class | CPU | `createParticleSystem(texture, max, lifetime, opts?)`; read-only `count`; read-write `speedScale`; retains its texture until destroyed |
| Body | One collision collider in the single physics world | Native class | CPU | `efx.physics.createBody` / `createStaticMesh`; read-only `position`/`transform`/`contacts`; read-write `velocity` |
| Character | Kinematic vertical-capsule character controller | Native class | CPU | `efx.physics.createCharacter`; read-only `position`/`onFloor`; read-write `velocity` |
| AudioData | Fully-decoded PCM (WAV/MP3) | Native class | CPU | `loadAudioData(path)`; no query properties |
| AudioStream | A streamed WAV/MP3 resource (decoded per playhead) | Native class | CPU | `loadAudioStream(path)`; no query properties |
| Audio | One playing audio handle | Native class | CPU | `playAudio(source)`; read-only `playing`/`paused`; read-write `volume`/`pan`/`pitch`/`loop`; `stop`/`pause`/`resume`; dropping the handle does not stop the sound |
| Lights | — | Slot-based | — | 4 point slots + 1 directional (fixed) |

**Resource lifecycle rules:**

- `destroy()` is deterministic and idempotent; using a destroyed resource
  throws.
- Native byte cost counts toward GC pressure and the player collects at frame
  end — unreferenced native resources are reclaimed within roughly a frame
  even if the script never calls `destroy()`. Exception: a live `Body` or
  `Character` is held by the physics world, so it keeps simulating while
  unreferenced and is released only by `destroy()` or `efx.physics.clear()`.
- Resources recorded into the display list stay alive until playback
  finishes; `destroy()` during a frame defers the native release to frame
  end.
- A `Texture` or `RenderTarget` referenced by a bound material map stays
  alive until that binding is released — rebind the surface without the map,
  bind `null`, or destroy the owning mesh.
- Everything still alive at shutdown is finalized by runtime teardown —
  scripts cannot leak past process exit.

**Fixed limits** (not configurable):

| Limit | Value |
|---|---|
| Point lights | 4 |
| Directional lights | 1 |
| Cameras | 1 3D camera (set, never created); the 2D projection frame is a separate projection state |
| Surfaces per mesh | 16 |
| Post-effect chain | 8 entries |
| Particles per system | 65536 |
| Render-target size | 4096 per side (positive integers) |
| Connected gamepads | 4 (fixed engine-owned bank reported by index) |
| Concurrent streamed sources | 4 (fixed cap; deterministic policy when exceeded) |
| Playback voices | 32 (fixed engine-owned bank; deterministic steal when all busy) |

Physics colliders are **dynamic-count** (no fixed cap): the world grows with
the script, with soft guidance rather than a hard maximum.

## Gamepad model

Gamepad input is the sub-namespace `efx.gamepad`. Pads are a **fixed
engine-owned bank** of four slots reported by index; gamepad adds **no
resource type** — no `create`, no `destroy`, no native-backed class.

- `efx.gamepad.count` is the number of connected pads; `get(index)` returns a
  pad view or `null` when the slot is empty; `onConnect`/`onDisconnect`
  return idempotent unsubscribe functions.
- **Semantic button names:** `south`, `east`, `west`, `north`,
  `leftShoulder`, `rightShoulder`, `leftTrigger`, `rightTrigger`, `back`,
  `start`, `guide`, `leftStick`, `rightStick`, `dpadUp`, `dpadDown`,
  `dpadLeft`, `dpadRight`. **Semantic axis names:** `leftX`, `leftY`,
  `rightX`, `rightY`, `leftTrigger`, `rightTrigger`.
- **Canonical ranges.** Stick axes are −1..1 and trigger axes are 0..1 on
  every target. Each trigger also reports as a digital button derived from its
  axis at the documented threshold **0.5**. Analog face-button values are
  reported as digital.
- **Semantics.** State is sampled once per frame at frame begin, so
  `isPressed`/`isReleased` are one-frame edges exactly like keyboard/mouse. A
  pad connected before the first frame is reported connected on the first
  frame. Unplugging a pad clears its state.
- **Normalization.** A device is normalized through the SDL game-controller
  mapping database by its GUID (with a permissive fallback); a browser pad
  reporting `mapping === 'standard'` normalizes directly by the standard
  layout. A device with no mapping is still reported `connected` with
  `mapped === false`, and its `rawButton`/`rawAxis` values are readable by
  index.
- **Errors.** `get` requires a numeric index; registrations require a
  function; a query with an unknown button or axis name throws `TypeError`.

## Audio model

Audio playback is the sub-namespace `efx.audio`. The engine owns **all**
mixing: scripts never see channels, buses, buffers, or voice allocation. There
are two source kinds, loaded separately from playback: `AudioData` holds
fully-decoded PCM and can back many overlapping playheads; `AudioStream` decodes
a long resource incrementally. One `playAudio(source, opts?)` verb accepts
either and returns a single `Audio` handle; `AudioData`/`AudioStream`/`Audio`
are native-backed classes.

- **Formats.** WAV (integer PCM / float) and MP3 from the resource root. Only
  **decoded PCM** is played — sequenced/modular formats (PS2/PSF, tracker
  modules, MIDI) are not supported and are rejected.
- **Loading vs playing.** `loadAudioData(path)` returns immutable PCM, so one
  buffer can be played many times at once. `loadAudioStream(path)` returns a
  streamed resource; each `playAudio` opens its own decoder and ~1 s ring, so
  the same stream can be started more than once. A playing handle retains its
  source.
- **Playback handles.** `playAudio` returns an `Audio` handle with read-only
  `playing`/`paused`, read-write `volume`/`pan`/`pitch`/`loop`, and
  `stop`/`pause`/`resume`. Options passed at start are initial values only: a
  fade is a sequence of handle `volume` writes in `update`. Dropping a handle
  never cuts the sound short; `stop()` does.
- **Bank and caps.** A fixed bank of 32 playback voices is mixed; the number of
  simultaneously *streaming* voices is capped at 4. When the bank (or the
  stream cap) is full the engine applies a deterministic policy (steal the
  quietest non-looping voice, oldest first); if every candidate is looping the
  new request returns `null`.
- **Master gain.** `efx.audio.volume` is a read-write master output gain
  applied to all playback — the only grouping control. There are no per-source
  buses or channels.
- **Mixing.** Sources are resampled from their file rate to the device rate
  with linear interpolation; `pitch` is a playback-rate multiplier over that
  conversion. `pan` is a linear stereo pan in `[-1, 1]`.
- **No device.** With no audio device (headless CI) the player runs silently:
  `playAudio` still returns a handle, but it reports not playing and produces no
  sound, and nothing crashes. On web, audio stays locked until a user gesture;
  the first input unlocks it and pending playbacks begin, with
  `efx.audio.resume()` as the explicit path.
- **Errors.** A non-string path, an invalid source, or unknown option throws
  `TypeError`; an unreadable or undecodable resource throws `Error`; a negative
  `volume`/master gain or non-positive `pitch` throws `RangeError`.

## Adding to the API

Every API addition or change follows the same path. The declaration is the
single source of truth, so the reference can never drift from it.

1. **Update the declaration** `gallery/src/api/efx.d.ts`:
   - full TSDoc: a summary, `@param` for every argument, `@returns` for every
     non-void result, `@example` where it helps, and documented defaults,
     constraints, and error behavior;
   - types precise enough to reject invalid calls at compile time (literal
     unions for fixed string sets, exclusive unions for alternative call
     forms, fixed-length tuples for vectors/matrices);
   - reference tags: a `@group` on every new top-level symbol, and `@hidden
     @inline` on a new single-use per-operation option bag (a bag referenced
     from more than one place keeps its page and belongs in `Configuration`).
2. **Regenerate the committed reference:** `npm --prefix gallery run
   docs:markdown` and commit `docs/api/`. The drift guard
   (`npm --prefix gallery run docs:check`) fails the Pages build if the
   committed reference is stale.
3. **Update the gallery type test** `gallery/src/api/efx.type-test.ts` when
   the change adds call forms that should be proven to type-check (and
   `@ts-expect-error` cases for invalid ones).
4. **Add a `js-api` spec delta** describing the required behavior of the
   addition, and update this guidelines document if the change settles or
   amends a **design rule** (conventions, layering, resource model, limits).
5. **Implement the binding** (ADR 0049): write the option-bag validator
   **once**, in the shared prelude (`src/prelude/prelude.js`), and keep the
   per-runtime natives marshal-only — they unpack the normalized form and
   never re-validate. Regenerate `prelude.h` after every prelude edit
   (`python3 tools/gen_prelude.py`; CI fails on drift). Hot paths keep their
   native validation unless new ADR 0049-budget measurements say otherwise.
   Three cross-runtime parity traps:
   - The **web** runtime keeps its own copies of some validators
     (`src/web/js/core.js`, `src/web/js/particles.js`) that are *not* the
     shared prelude. A new option field needs the same check there **with the
     same error message**. `tests/scripts/s_error_catalog.js` pins the exact
     text per runtime, so add a case for the new field and let the desktop/web
     compare catch divergence. Watch the helpers: the shared `__efxPartEnum`
     throws "`<what>` has an unknown value", which is **not** the native
     "unknown blend mode" — add a dedicated helper when a message must match.
     To exercise a method receiver guard portably, call
     `instance.method.call({}, ...)`; a bare `({}).method(...)` yields a
     host-specific "not a function" message that is not pinned.
   - The **material** object is marshalled as a fixed float block that grows
     with the API (17 → 18 when `blend` was added). Update every copy
     together: prelude `__efxMaterialWire`, web `core.js` `__efxMaterial`,
     `api_3d.c` `wire_mat_from_block`, `web/bridge_render3d.c`
     `bridge_mat_from_wire`, and the web `createMeshData` stride in
     `src/web/js/audio.js`.
   - **Method receiver checks run before argument checks.** The web
     `__efxResourceClass` wrapper applies the class liveness guard
     (`live(this)`) before the method body, so the native entry must resolve
     the receiver from `this_val` first and throw the class guard message
     (`expected a Mesh` / `expected a Font` / `using a destroyed resource`) —
     not the old free-function argument text — or the runtimes diverge.
6. **Do not** add internal roadmap-milestone tags or per-entry layer tags to
   the reference. The API is end-user facing.

A change that adds, modifies, or removes a public API function MUST update the
declaration and regenerate the reference in the same change.

## Vision traceability

Every consumer-API property named in `vision.md` maps to a documented symbol
in the generated reference (or to an open question below):

| vision.md property | Documented as |
|---|---|
| 2D drawing via quads | `efx.graphics.drawQuad`, `DrawQuadOptions` |
| Additive and subtractive blending modes | `efx.graphics.setBlendMode` plus per-object `blend` overrides (`DrawQuadOptions`, `DrawSpritesOptions`, `Material`, `DrawBillboardOptions`, particle `blend`) |
| 1 camera fixed | `efx.graphics.setCamera2D`, `efx.graphics.setCamera3D`, fixed limits |
| Rendering meshes | `efx.graphics.createMesh` / `efx.graphics.drawMesh` |
| Vertex colours | `MeshSurfaceData.colors`, `DrawMeshOptions.color` |
| Matrix math | `efx.math.mat4` / `efx.math.vec3` / `efx.math.quat` |
| Procedural primitives | `makeCube` / `makePlane` / `makeSphere` / `makeCapsule` |
| 4 point lights, 1 directional light | `efx.graphics.setLight`, `efx.graphics.setDirectionalLight`, fixed limits |
| Phong material system, 4 channels + maps | `Mesh.setSurfaceMaterial`, `Material` |
| Alpha masks | `Material.alphaMask` |
| Rendering to textures | `efx.graphics.createRenderTarget` / `efx.graphics.beginRenderTarget` |
| Simple post processing | `efx.graphics.setPostEffects`, `efx.graphics.setRenderScale` |
| Resource folder / zip root (`res://`-like) | `efx.io.loadText` / `efx.io.loadData` / `efx.graphics.loadImage` / `efx.graphics.loadMeshData` |
| REPL console mode | the `--repl` run mode (no new API) |
| Skinning and animations | `Mesh.pose`, `DrawMeshOptions.skinned` |
| PS2-era particle effects | `efx.graphics.createParticleSystem` / `efx.graphics.drawParticles` |
| World-space sprites / billboards | `efx.graphics.drawBillboard` |
| Batched 2D sprite drawing | `efx.graphics.drawSprites` |
| Collision / character controller / dynamics | `efx.physics` |
| Raycasts / line-of-sight / picking | `efx.physics.raycast` / `overlap` / `shapeCast` |
| Keyboard/mouse input query + events | `efx.keyboard` / `efx.mouse` / `efx.window` |
| Gamepad input query + events | `efx.gamepad` |
| Audio playback (streamed + decoded) | `efx.audio` (`loadAudioData` / `loadAudioStream` / `playAudio`) |
| Script modules (TypeScript `import`) | the CommonJS `require` model |
| Text / fonts | `efx.graphics.loadFontData` / `createFont` / `drawText` / `Font.measure` |
| Callbacks for update and rendering | `efx.registerUpdateHook` / `registerRenderHook` |
| Low/mid C + high-level JS layering | Two layers (Overview) |
| No browser/Node dependencies | Conventions (Dependencies) |
| Handles or pre-allocated slots for unmanaged resources | Resource & memory model |
| Fixed-function pipeline (no consumer shaders) | Engine-internal; no API entry |
| Immediate-mode API with re-orderable display list | Overview |
| Single-binary player for resource folders | Player runtime (not this API) |

## Open questions

Flagged gaps and deferred decisions — recorded here rather than inventing API
for them:

- **Procedural rigs** — skins/skeletons/clips are imported only; constructing
  a rig procedurally has no path yet. Deferred until a concrete need appears.
- **Stateful playback helper** — a play/pause/blend convenience as pure JS
  over `Mesh.pose`, not engine state.
- **REPL introspection helpers** — whether the console mode needs extra `efx`
  functions beyond the interactive namespace.
