# EmotionFX JavaScript API Reference

**Status:** F1 (including explicit lifecycle hook registration), F2, F3, F4a
(lighting + Phong materials on solids/vertex colors), F4b (per-channel
maps + alpha masks), F5a (render targets), F5b (post-effect chain +
render scale), F6a (resource root + text/image loading), F6b (glTF static
import), F6c (glTF rig import), F6d (interactive console run mode —
no new API), F6e (texture creation options), and F7 (CPU skinning +
animation — `poseMesh` and the `skinned` draw option) are implemented
(current behavior). F9 (input — keyboard + mouse query and event API) is
implemented and **provisional** until its four-target gate passes; F8 onward
is a provisional contract — names and
signatures may be reshaped by
the change that delivers them (every API change must update this document in
the same change). See `vision.md` for product goals and
`openspec/specs/feature-roadmap` for the milestone ladder.

## Overview

- **One namespace.** All engine-provided functions — C-implemented and
  pure-JS high-level — live on a single global object `efx`, available to
  every script without imports or setup. Scripts reach engine functionality
  only through `efx` and standard ES6 built-ins.
- **Two layers.** `[C]` entries are implemented in C and registered through
  the engine binding. `[JS]` entries are engine-provided, bundled pure ES6
  built on top of the public `[C]` API — nothing else.
- **Runtime binding per platform.** Every `[C]` entry is implemented once
  in C and exposed to scripts through the platform's binding — on desktop
  the embedded quickjs binding (`C · quickjs`, `src/api/` + `src/runtime/`),
  on Emscripten the native bridge to the page's own JS engine
  (`C · bridge`, `src/web/`; ADR 0022). Names, signatures, semantics,
  errors, and resource lifecycle are identical across both bindings — the
  same script text runs on every target, and game scripts never see host
  globals (`window`, `document`, `process`, …).
- **Entry tags.** Each catalog entry carries its delivering milestone and
  layer:

  ```js
  // F2 · C · current
  efx.drawQuad(x, y, texture, opts?)
  ```

  Entries without a `provisional` marker describe current, shipped behavior.
- **Immediate mode, deferred rendering.** `draw*` calls record into an
  internal display list that the renderer may re-order; there is no explicit
  flush in the API.

## Conventions

Every rule below traces to vision.md or to F1's implemented behavior.

- **Namespace** (F1 implementation): engine functions are members of the
  global `efx` object — `efx.log(...)`, `efx.quit(...)` — never free globals
  and never behind imports. Frame callbacks are registered through
  `efx.registerUpdateHook`/`efx.registerRenderHook`; global `update`/`render`
  functions remain as load-time sugar (see below).
- **Naming**: camelCase, verb-first. `set*`/`get*` configure engine state,
  `draw*` record into the display list, `make*` build data in JS,
  `load*`/`create*` fetch or upload resources and return resource objects.
- **Resources**: loaders and creators return opaque resource objects (the
  resource taxonomy — see [Resource & memory
  model](#resource--memory-model)); `res.destroy()` releases
  deterministically and GC is the backstop. Native-backed classes are
  otherwise fully opaque except for documented read-only query properties —
  Texture's `width`/`height`, ImageData's `width`/`height`, and
  MeshData's/Mesh's `surfaceCount`.
- **Parameters**: hot immediate-mode calls take scalar arguments first
  (`drawQuad(x, y, texture, opts?)`); configuration beyond ~3 values
  goes in a trailing option object. Optionality is explicit at two levels:
  - a `?` on the bag itself (`opts?`) means the whole object may be
    omitted — every field then takes its documented default;
  - a `?` on a field (`range?`) means that field may be omitted — fields
    are optional **only when a default is documented**; unmarked fields
    are required.
- **Option-object validation**: a missing or wrongly-typed required field
  throws `TypeError`; unknown fields throw `TypeError` (typo protection).
  Bags documented as "null disables" (e.g. post FX) accept `null` as an
  explicit off switch.
- **Units**: angles in **degrees** (radians never appear in the API), time in
  **seconds**, positions and sizes in world units.
- **Colors**: `[r, g, b, a]` arrays of normalized floats in `0..1`
  (e.g. `[1, 0.5, 0, 1]`).
- **Errors**: invalid input throws standard ES6 errors (`TypeError` for wrong
  types, `RangeError` for out-of-range slots/indices). An uncaught exception
  stops the run with a non-zero exit code and the error on stderr — in
  `--script` mode and in both frame hooks alike.
- **Dependencies**: scripts use only the `efx` namespace and standard ES6.
  Browser and Node.js APIs are unavailable — not even transitively.

## Lifecycle hooks

The entry script is `main.js` at the resource root (a zip in F6+).
**Loading `main.js` is the implicit init**: the `efx` namespace and every
engine function are ready — the rendering surface is initialized when the
frame loop starts and is not script-visible at load time — *before* the
script executes, and top-level code is where setup happens. There is no
separate `init()` hook.

Frame callbacks are registered explicitly and stack in registration order:

```js
// F1 · C · current (ADR 0016)
const offUpdate = efx.registerUpdateHook(dt => { ... }); // dt: seconds since previous frame
const offRender = efx.registerRenderHook(() => { ... });

offUpdate(); // optional unsubscribe
```

```js
// F1 · C · current
efx.registerUpdateHook(fn)   // fn(dt) — dt: seconds since previous frame (0 on the first frame)
efx.registerRenderHook(fn)   // fn() — no arguments
```

- Both functions require a function argument; anything else throws
  `TypeError`. Each returns an **unsubscribe function**; calling it removes
  that registration and is idempotent.
- Update hooks run before render hooks, once per frame, in registration
  order. An exception in any hook (or at load time) stops the run and exits
  non-zero.
- **Global `update`/`render` remain supported as load-time sugar:** if the
  script defines them, the engine registers them after evaluation in load
  order, so they run after any hooks registered during evaluation. Scripts
  using only the globals keep working unchanged; the only difference is that
  the global `update` now also receives `dt`.
- The REPL (F6) registers and unregisters through the same functions — the
  reason registration, not globals, is the normative model.
- The engine's readiness guarantee covers the script-visible API; moving
  window/GL-context creation ahead of `main.js` evaluation is deferred
  (ADR 0016, amended).

## Resource & memory model

Every resource type scripts can create or reference is classified exactly
one way — the rule that keeps a GC'd language from leaking unmanaged memory
(vision.md). Five dynamic-count resource types are opaque **native-backed
classes**; only the fixed light bank is slot-based (model: ADR 0011,
memory discipline: ADR 0012, glTF data model: ADR 0014, implicit rig
payload + `skinned` flag: ADR 0017, multi-surface mesh data model +
per-surface materials: ADR 0024, per-channel maps + alpha mask + retained
map textures: ADR 0027, glTF rig payload: ADR 0033 — all under
`docs/decisions/`).

| Class | Meaning | Release path |
|---|---|---|
| **JS-managed** | Plain data objects; garbage collected | Drop the reference |
| **Native-backed class** | Opaque object wrapping a native handle — read-only query properties only where documented (Texture: `width`/`height`; ImageData: `width`/`height`; MeshData/Mesh: `surfaceCount`; RenderTarget: `width`/`height`); GC finalizer backstop | `res.destroy()` (primary), GC / shutdown (backstop) |
| **Slot-based** | Fixed pre-allocated bank of indexed resources | Overwrite the slot |

| Resource | Contents | Class | Side | Delivered | Notes |
|---|---|---|---|---|---|
| MeshData | 1..16 surfaces, each with its own attribute arrays + optional indices (Godot surface / glTF primitive; ADR 0024); skinned meshes add `joints`/`weights` per surface (F6c, glTF-style) | Native class | CPU | F3 | `createMeshData` / `loadMeshData` (F6); read-only `surfaceCount` |
| ImageData | Raw pixels + size + format | Native class | CPU | F2 | `createImageData` / `loadImage` (F6a); read-only `width` / `height` (throw `TypeError` when destroyed) |
| Mesh | GPU mesh (all surfaces uploaded); skinned meshes carry the skeleton and clips internally (F6c, ADR 0017/0033); per-surface material binding slot (active from F4a) | Native class | GPU | F3 | `createMesh(meshData)`; `mesh.destroy()`; read-only `surfaceCount` |
| Texture | GPU texture | Native class | GPU | F2 | `createTexture(imageData, opts?)` (`opts.wrap`/`opts.filter`/`opts.mipmaps`, F6b/F6e); `tex.destroy()`; read-only `tex.width` / `tex.height` (texture pixels; throw `TypeError` when destroyed); `efx.whiteTexture` is an engine-owned instance (destroy throws) |
| RenderTarget | GPU render target (color + depth attachments, env-default formats) | Native class | GPU | F5a | `createRenderTarget({ width, height })` (1..4096 per side); `rt.destroy()`; read-only `rt.width` / `rt.height` (target pixels; throw `TypeError` when destroyed); a live RenderTarget is accepted **wherever a live Texture is** — `drawQuad`, material `map`s, `alphaMask` — with identical error behavior; no alias Texture exists for a target (ADR 0028) |
| Materials (Phong parameter objects) | — | JS-managed | — | F4a/F4b | Bound per surface via `efx.setMeshSurfaceMaterial` / the `materials` array (ADR 0024); per-channel `map`s and `alphaMask` reference native-backed `Texture`s the engine retains while bound (F4b, ADR 0027) |
| Post-effect chain entries | `{ effect, ...options, mix? }` option bags | JS-managed | — | F5b | Plain objects snapshotted at `setPostEffects` call time; no native handle and no `destroy()`. The native passes they drive are engine-owned and never script-visible (ADR 0029) |
| Fonts (atlas + quad layout) | — | JS-managed | — | F8 | Pure JS over Texture; passed to `drawText` |
| Lights | — | Slot-based | — | F4a | 4 point slots + 1 directional (fixed) |

**Resource lifecycle rules:**

- `destroy()` is deterministic and idempotent; using a destroyed resource
  throws.
- Native byte cost counts toward GC pressure and the player collects at
  frame end — unreferenced native resources are reclaimed within roughly a
  frame even if the script never calls `destroy()`.
- Resources recorded into the display list stay alive until playback
  finishes; `destroy()` during a frame defers the native release to frame
  end.
- A `Texture` or `RenderTarget` referenced by a bound material map (F4b;
  RenderTargets accepted from F5a) stays alive until
  that binding is released — rebind the surface without the map, bind
  `null`, or destroy the owning mesh (ADR 0027/0028).
- Everything still alive at shutdown is finalized by runtime teardown —
  scripts cannot leak past process exit.

**Fixed limits** (vision.md — not configurable):

| Limit | Value |
|---|---|
| Point lights | 4 |
| Directional lights | 1 |
| Cameras | 1 3D camera (set, never created); the F2 2D projection frame is a separate projection state |
| Surfaces per mesh | 16 |
| Post-effect chain | 8 entries (F5b) |
| Render-target size | 4096 per side (width and height, positive integers; F5a) |

## API catalog

### F1 — Environment & utilities (current)

Implemented in `src/api/api.c` and registered on the `efx` object by
`src/runtime/runtime.c` (desktop, `C · quickjs`); on Emscripten the same
functions come from `src/web/bridge.c` through the native bridge
(`C · bridge`) with identical semantics. These are current behavior, not
provisional.

```js
// F1 · C
efx.log(msg?)
```

Prints `msg` to stdout followed by a newline and flushes. Non-string values
are converted with their standard string representation; a missing argument
prints an empty line.

```js
// F1 · C
efx.quit(code?)
```

Requests engine termination with exit code `code` (default `0`). The call
never returns normally: the engine unwinds the current script execution and
exits with the requested code. Works identically in `--script` mode (the
vehicle for smoke tests) and inside the frame loop.

```js
// F1 · C
efx.args()
```

Returns a `string[]` of the arguments the host passed to the script run
(the `--script <file> [args...]` tail). Empty array when none were given.

```js
// F1 · C · current
efx.registerUpdateHook(fn)   // fn(dt); returns an unsubscribe function
efx.registerRenderHook(fn)   // fn();   returns an unsubscribe function
```

F1 connects frame callbacks through explicit registration (ADR 0016) — see
[Lifecycle hooks](#lifecycle-hooks) for the stacking/`dt`/unsubscribe rules.
The global `update`/`render` functions remain supported as load-time sugar,
so both of these are current:

```js
// main.js — F1 sample (explicit registration)
let frames = 0;

const offUpdate = efx.registerUpdateHook(function (dt) {
    frames++;
    if (frames === 1) {
        efx.log('hello from efx ' + efx.args().join(' '));
    }
    if (frames >= 60) {
        offUpdate();
        efx.quit(0); // exits the player with code 0
    }
});

efx.registerRenderHook(function () {});
```

```js
// main.js — F1 sample (global sugar, still current)
let frames = 0;

function update() {
    frames++;
    if (frames === 1) {
        efx.log('hello from efx ' + efx.args().join(' '));
    }
    if (frames >= 60) {
        efx.quit(0);
    }
}

function render() {}
```

### F2 — 2D drawing (current)

Scope from roadmap F2: `drawQuad`, ortho camera, texture slots, blending
modes, display list (record → playback); golden-image harness first-class
(ADR 0020).

All 2D drawing happens inside a **virtual pixel frame** established by
`setCamera2D` — a projection state of its own; 3D drawing (F3) has its own
camera and the two never mix. Coordinates are frame pixels, origin at the
**top-left**,
y pointing **down**, angles in degrees measured clockwise.

```js
// F2 · C · current — desktop binding `C · quickjs`, web binding `C · bridge`; identical semantics
efx.setClearColor(color)          // [r,g,b,a]; frame clear color (default black)
efx.setCamera2D(opts)             // { frame?, x?, y?, zoom?, rotation? }
efx.createImageData(opts)         // → ImageData; { width, height, pixels, format? = 'rgba8' }
efx.createTexture(imageData, opts?)  // → Texture; uploads CPU → GPU
                                    // opts: { wrap?, filter?, mipmaps? }
efx.drawQuad(x, y, texture, opts?)  // required texture — a live Texture or RenderTarget (F5a); opts below
efx.setBlendMode(mode)            // 'alpha' (default) | 'additive' | 'subtractive'
efx.whiteTexture                  // engine-owned 1×1 white Texture (read-only)
```

**Camera / projection frame** — `setCamera2D({ frame, x, y, zoom,
rotation })`:

- `frame: [width, height]` sets the virtual resolution; every draw
  coordinate is in frame pixels. The frame maps onto the whole window with
  a **stretch** policy (no letterboxing). Omitted → frame equals the
  current window size.
- `x`, `y` name the world point displayed at the **frame center**;
  default: the frame center itself.
- `zoom` (default 1, > 0) and `rotation` (default 0) transform around the
  frame center: zoom 2 shows exactly half the frame's world extent, still
  centered on `x`/`y`.
- Never calling `setCamera2D` gives the default camera: frame = current
  window size, view centered, zoom 1 — pixel coordinates match window
  pixels.
- Camera state applies to draws recorded **after** the call; recorded
  draws never observe later changes (same for blend mode — ADR 0019).

**`drawQuad(x, y, texture, opts?)`** — records one quad:

- `x`, `y` place the quad's **top-left corner** in frame pixels.
- `texture` is **required** — a live Texture (or a live RenderTarget from
  F5a). Solid-color rectangles use
  `efx.whiteTexture` with a tint; `efx.whiteTexture` is engine-owned,
  `destroy()` on it throws `TypeError`.
- **Size derivation** — the quad's size in frame pixels is the first of:
  `opts.size` (`[width, height]`, both finite and > 0), else the drawn
  `sourceRect` region's extent, else the texture's pixel size (1:1 sprite).
  `opts.scale` applies **after** the size is determined — size is pre-scale.
  A size entry ≤ 0, or a zero-extent `sourceRect`, throws `RangeError`.
- `opts.color` — tint `[r,g,b,a]`, default opaque white.
- `opts.rotation` — degrees clockwise, default 0.
- `opts.scale` — uniform factor, default 1 (> 0).
- `opts.sourceRect` — `{ x, y, w, h }` region of the texture in **texture
  pixels**; default: the full texture. Out-of-bounds rects throw
  `RangeError`.
- `opts.origin` — `[px, py]` in quad-local frame pixels (relative to the
  quad's top-left): the **pivot point** for rotation and scale. Default is
  the determined size's center. The origin offset itself is never rotated or
  scaled; an unrotated, unscaled quad always places its top-left at
  `(x, y)` regardless of `origin`.
- Unknown option fields throw `TypeError` (typo protection).

**Resources** — `createImageData({ width, height, pixels, format? })`
builds CPU pixels: `pixels` is a flat array or typed array of RGBA8 bytes,
length exactly `width × height × 4` (else `RangeError`); `format` is
`'rgba8'` (the only format in F2). `createTexture(imageData, { wrap,
filter, mipmaps })` uploads to a GPU Texture — both are opaque
native-backed classes: `destroy()` releases
deterministically, is idempotent, and using a destroyed resource throws.
`wrap` (`'repeat'` default, `'clamp'`, `'mirror'`) and `filter`
(`'linear'` default, `'nearest'`) select the sampler; `mipmaps` (boolean,
default `false`, F6e) builds and uses a full mip chain. A
live Texture also exposes read-only `width` / `height` (its pixel size —
the same values `drawQuad` derives from); reading either on a destroyed
texture throws `TypeError`.

**Display list** — draw calls record into a per-frame list played back
after the render hook returns; there is no flush and no script-visible
inspection. Playback preserves record order (F2 never reorders — ADR
0019/D3); overlapping draws keep painter's order. A per-frame record
budget (~170k quads) is enforced; exceeding it throws `RangeError`.

```js
// main.js — F2 sample
const logo = efx.createTexture(
    efx.createImageData({ width: 64, height: 64, pixels: makeLogoPixels() }));
efx.setClearColor([0.08, 0.09, 0.12, 1]);
efx.setCamera2D({ frame: [640, 480] }); // virtual 640×480 frame, view centered

function render() { // global hook (or efx.registerRenderHook(fn))
    efx.setBlendMode('alpha');
    efx.drawQuad(64, 64, logo, { size: [128, 128] }); // textured sprite, stretched 2x
    efx.setBlendMode('additive');
    efx.drawQuad(224, 96, efx.whiteTexture, { size: [64, 64], color: [1, 0.5, 0, 1] });
    efx.setBlendMode('alpha');
}
```

### F3 — 3D core (current)

Scope from roadmap F3: camera, mesh slots, `drawMesh`, matrix math, depth
test, vertex colors, procedural primitives. The mesh data model is
**multi-surface, Godot-style** (ADR 0024): a mesh holds 1..16 surfaces,
each with its own attribute arrays and — from F4a — its own material.

```js
// F3 · C · current — desktop binding `C · quickjs`, web binding `C · bridge`;
// identical semantics
efx.setCamera3D(opts)      // { pos, target, fov, near? = 0.1, far? = 100 }
                           // fov: vertical, degrees; up is +Y; the one 3D camera
efx.createMeshData(data)   // → MeshData; multi-surface, below
efx.createMesh(meshData)   // → Mesh; uploads ALL surfaces CPU → GPU
efx.drawMesh(mesh, opts?)  // { transform?, color? } — whole mesh, depth-tested
```

- `setCamera3D` is a projection state **separate from the 2D frame**:
  3D draws use it; 2D draws keep the `setCamera2D` state (F2 behavior and
  goldens are unchanged). Like all recorded state, the camera is
  value-snapshotted at record time.
- **MeshData** construction — batch form `{ surfaces: [surface, ...] }`
  (1..16 entries) or single-surface shorthand
  `{ positions, normals?, uvs?, colors?, indices? }`; passing both forms
  throws `TypeError`. Each surface is one Godot surface / glTF primitive:
  - `positions` — required flat xyz (array or typed array),
  - `normals?` / `uvs?` / `colors?` — flat arrays matching the vertex
    count (×3 / ×2 / ×4),
  - `joints?` / `weights?` — flat arrays of four joint indices / four
    joint weights per vertex matching the vertex count (×4 / ×4; skinned
    meshes, F6c). An unpaired attribute, a non-4-per-vertex count, or a
    count that does not match `positions` throws `RangeError`; a non-number
    element throws `TypeError`. `joints` values are non-negative integers.
  - `indices?` — triangle list of integers `< vertexCount`; omitted =
    non-indexed (vertex count then divisible by 3).
  Element rules: non-number → `TypeError`, non-finite → `RangeError`;
  count/length/range problems → `RangeError`; unknown fields →
  `TypeError`. From F4a a parallel `materials` array (creation-time surface
  bindings — see the F4a section) is accepted. Read-only query
  property `surfaceCount`; native byte cost counts toward GC pressure
  (ADR 0012).
- **Mesh** is a copy: `createMesh` uploads every surface to the GPU, and
  the source MeshData can be destroyed afterwards. Read-only query
  property `surfaceCount`. Opaque native-backed class (ADR 0011/0013):
  `destroy()` releases deterministically, is idempotent, and use after
  destroy throws. Each surface carries a material binding slot — filled at
  creation from the `materials` array or rebound via
  `setMeshSurfaceMaterial` (F4a); an unbound surface renders with
  the engine default material.
- **`drawMesh(mesh, opts?)`** draws the whole mesh: `mesh` is a **required
  positional argument** and must be a live Mesh (nothing, a non-Mesh, or a
  destroyed Mesh throws `TypeError`). `opts?` is an optional bag restricted
  to `{ transform?, color? }` (an unknown field, including `mesh`, throws
  `TypeError`). It draws
  every surface in surface order under the recorded camera, depth-tested
  against earlier 3D records (equal depth resolves by record order).
  `transform` is a flat column-major 16-number array (default identity;
  wrong length → `RangeError`),   `color` a tint multiplying vertex colors
  (default opaque white). No single-surface draw — split the mesh. From F4a
  the fill is lit Phong (see the F4a section); surface `uvs` (validated and
  stored since F3) are the texture coordinate for F4b's per-channel maps and
  alpha mask, and do not otherwise affect shading. 2D records are
  untouched by mesh depth (painter's order, no depth write).

```js
// F3 · JS · current — pure-JS math helpers, engine-bundled (one source on
// both bindings); plain JS data in/out, degrees, column-major float[16];
// every helper is pure (inputs are never mutated)
efx.mat4.identity()  efx.mat4.perspective(fovY, aspect, near, far)
efx.mat4.ortho(w, h, near, far)  efx.mat4.translate(m, v)
efx.mat4.rotate(m, deg, axis)    efx.mat4.scale(m, v)
efx.mat4.multiply(a, b)          // a·b (b applies to the vector first)
efx.vec3.add(a, b)   efx.vec3.sub(a, b)  efx.vec3.scale(v, s)
efx.vec3.normalize(v) efx.vec3.cross(a, b) efx.vec3.dot(a, b)
efx.quat.identity() efx.quat.fromAxisAngle(deg, axis)
efx.quat.multiply(a, b) efx.quat.toMat4(q)  // consumed by F7
```

```js
// F3 · JS · current — procedural primitives producing single-surface
// MeshData (pinned layouts: cube 24 verts / 36 indices with per-face
// normals + 0..1 uvs; plane on XZ facing +Y, (segments+1)^2 grid; UV
// sphere with normals = normalized positions and equirectangular uvs)
efx.makeCube(opts?)     // { size? = 1, material? }
efx.makePlane(opts?)    // { size? = 1, segments? = 1, material? }
efx.makeSphere(opts?)   // { radius? = 1, segments? = 16, material? }
// size/radius: finite > 0; segments: positive integer; unknown fields throw
// material?: a material object (or null = engine default) bound to the
// single surface at creation, carried to createMesh
```

```js
// main.js — F3 sample (current API)
const cube = efx.createMesh(efx.makeCube({ size: 1 }));
efx.setClearColor([0.08, 0.09, 0.12, 1]);
efx.setCamera3D({ pos: [0, 2, 5], target: [0, 0, 0], fov: 60 });

let yaw = 0;
efx.registerUpdateHook(dt => { yaw += dt * 45; });

efx.registerRenderHook(() => {
    efx.drawMesh(cube, {
        transform: efx.mat4.rotate(efx.mat4.identity(), yaw, [0, 1, 0]),
        color: [0.9, 0.4, 0.2, 1],
    });
});
```

### F4a — Materials & lights (current)

Scope from roadmap F4a: 4 point + 1 directional light; a 4-channel Phong
material (Ambient, Diffuse, Specular, Emissive) on solids and vertex colors.
Per-channel maps and alpha masks extend the same material object in F4b,
documented in the [F4b section](#f4b--per-channel-maps--alpha-masks-current)
below.

```js
// F4a · C · current — desktop binding `C · quickjs`, web binding `C · bridge`; identical semantics
efx.setLight(slot, opts)         // slot 0..3 — point light { pos, color, range? }; null disables
efx.setDirectionalLight(opts)    // { dir, color } — the single directional light; null disables
// Materials bind to SURFACES — there is no global material state (ADR 0024):
efx.setMeshSurfaceMaterial(mesh, surfaceIndex, mat)   // mat object or null (default)
// mat: Phong channels; omitted channels take defaults:
// {
//   ambient:  { color },             // default: black
//   diffuse:  { color },             // default: white
//   specular: { color, shininess? }, // default: black (shininess 32)
//   emissive: { color },             // default: black
// }
// surfaces without a bound material render with the engine default material
// (white diffuse Phong, no maps)
```

- **Lights** are a fixed bank — four point slots + one directional light, all
  disabled at startup (vision.md fixed limits; the only slot-based resource).
  `setLight(slot, opts)` with `slot` an integer `0..3` sets
  `{ pos: [x,y,z], color: [r,g,b,a], range? }`: `range` is a finite
  attenuation radius (`>= 0`, default `0` = no falloff; attenuation is
  `clamp(1 - d/range, 0, 1)`); the color alpha is ignored. Passing `null`
  disables the slot. `setDirectionalLight({ dir, color })` sets the single
  directional light; `dir` is the direction the light **travels** (the
  direction to the light is `-dir`), and `null` disables it. Malformed bags
  throw `TypeError`; an out-of-range slot or negative/non-finite `range`
  throws `RangeError`.
- **`mat` is a JS-managed object** (no native handle, no `destroy()`); the
  engine reads (snapshots) it at binding time, so later mutation of the
  script object does not change the bound material. Re-calling
  `setMeshSurfaceMaterial` switches that surface's material; `null` restores
  the default. A channel's alpha is ignored in F4a. `createMeshData` also
  accepts a parallel `materials` array with one entry per surface
  (`materials[i]` — an object or `null` for the default), bound at creation
  and carried over at `createMesh`.
- **Lit shading** is world-space Phong, per fragment: albedo = vertex color ×
  `drawMesh` tint; `ambient·albedo + emissive + Σ(diffuse·albedo·N·L +
  specular·(N·H)^shininess)·lightColor·atten`, clamped to `[0,1]`. Emissive
  is added unmodulated; specular is not modulated by the albedo; a surface
  without `normals` uses the default normal `(0,0,1)`. With no lights, a
  default-material surface renders black (ADR 0026).

```js
// main.js — F4a sample (current API)
const ball = efx.createMesh(efx.makeSphere({ radius: 1, segments: 24 }));
efx.setClearColor([0.05, 0.05, 0.08, 1]);
efx.setCamera3D({ pos: [0, 2, 5], target: [0, 0, 0], fov: 60 });
efx.setLight(0, { pos: [3, 4, 2], color: [1, 0.95, 0.9, 1], range: 20 });
efx.setDirectionalLight({ dir: [-0.5, -1, -0.3], color: [0.2, 0.25, 0.35, 1] });
efx.setMeshSurfaceMaterial(ball, 0, {
    ambient:  { color: [0.05, 0.05, 0.05, 1] },
    diffuse:  { color: [0.8, 0.3, 0.2, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 32 },
    emissive: { color: [0, 0, 0, 1] },
});

efx.registerRenderHook(() => {
    efx.drawMesh(ball);
});
```

### F4b — per-channel maps & alpha masks (current)

Scope from roadmap F4b: per-channel maps and alpha masks extend the same
Phong material object; `uvs` (validated and stored since F3) are consumed as
the texture coordinate. No new functions — F4b is additive to the F4a
material object.

```js
// F4b · C · current — maps extend the F4a material object
// {
//   ambient:  { color, map: tex },      // tex: a live Texture (or RenderTarget, F5a)
//   diffuse:  { color, map: tex },
//   specular: { color, shininess, map: tex },
//   emissive: { color, map: tex },
//   alphaMask: tex,                     // material-level binary cutout
// }
```

- **Channel maps** — each channel's optional `map` (a live `Texture`)
  modulates that channel's color by `texture(map, uv).rgb`, sampled per
  fragment at the surface's interpolated `uv`. The specular map scales the
  specular color, not `shininess`; channel alphas are ignored by shading. An
  omitted or `null` map contributes the neutral factor `1`, so a material
  that binds no maps shades exactly as in F4a and the committed F4a goldens
  are unchanged. A surface without `uvs` samples every map at `(0, 0)`.
- **Alpha mask** — the material-level optional `alphaMask` (a live `Texture`)
  is a **binary cutout**: a fragment whose sampled mask alpha is below `0.5`
  is discarded before lighting; surviving fragments keep the albedo alpha
  (vertex-color alpha × tint alpha). The mask's RGB does not modulate
  anything and there is no soft/graded alpha.
- **Validation** — a `map` or `alphaMask` that is not a live `Texture` (a
  non-Texture value or an already-destroyed texture) throws `TypeError`, as
  does an unknown channel/material field; the previous binding is unchanged.
  `null`/omitted means no map.
- **Retained map textures** — a bound map keeps its `Texture` alive:
  `tex.destroy()` releases the script handle immediately (later script use
  throws), but the texture's native storage is freed only once no material
  binding references it (rebind without it, bind `null`, or destroy the
  owning mesh). A map bound before `destroy()` therefore keeps rendering
  until it is unbound.
- **Snapshot** — the engine reads (snapshots) the material at binding time,
  including the map handles, so later mutation of the script object does not
  change the bound material.

```js
// main.js — F4b sample (current API)
const tex = efx.createTexture(efx.createImageData({
    width: 64, height: 64, pixels: makeCheckerPixels(),
}));
const ground = efx.createMesh(efx.makePlane({ size: 10, segments: 4 }));
efx.setMeshSurfaceMaterial(ground, 0, {
    ambient:  { color: [0.1, 0.1, 0.12, 1] },
    diffuse:  { color: [1, 1, 1, 1], map: tex },
    specular: { color: [0.6, 0.6, 0.6, 1], shininess: 24 },
    emissive: { color: [0, 0, 0, 1] },
    alphaMask: tex,     // discard fragments where tex alpha < 0.5
});

efx.registerRenderHook(() => { efx.drawMesh(ground); });
```

### F5a — Render targets (current)

Scope from roadmap F5's first half: RTT with render redirection and
texture-coerced sampling (ADR 0028).

```js
// F5a · C — desktop binding `C · quickjs`, web binding `C · bridge`
efx.createRenderTarget(opts)      // { width, height } → RenderTarget
efx.beginRenderTarget(rt)         // redirect subsequent records into rt
efx.endRenderTarget()             // back to the default target
rt.destroy()                      // deferred to frame end while records or
                                  // bound material maps hold it
rt.width / rt.height              // read-only target pixels (throw when destroyed)
```

**Resources** — `createRenderTarget({ width, height })` builds a GPU
render target: both fields required positive integers in 1..4096
(`RangeError` outside, `TypeError` for missing fields or unknown options).
A RenderTarget is a native-backed class (deterministic idempotent
`destroy()`, GC-finalizer backstop) owning a color+depth attachment pair
in the environment-default pixel formats (ADR 0025) — `drawMesh` depth
tests apply unchanged inside targets.

**Redirection** — `beginRenderTarget(rt)` redirects all subsequently
recorded draws into `rt` until `endRenderTarget()`; the target is then the
*rendering surface*: the default 2D camera frame and the 3D projection
aspect follow the target's extent exactly as they follow the window (an
explicit `setCamera2D` frame stretches onto the target as onto the
window). Entering a target **clears it** to the clear color in effect at
that moment (value snapshot) — every begin starts from a cleared target,
so a target rendered in several segments shows the last segment's
contents. Nesting is rejected: a begin while a begin is active throws
(`TypeError`), an end without one throws.

**Sampling (texture coercion)** — a live RenderTarget is accepted wherever
a live Texture is accepted: `drawQuad(x, y, rt, opts?)` (size derivation
and `sourceRect` use the target's extent), per-channel material `map`s,
and `alphaMask` — with identical validation and retention semantics as
Textures (a bound target map is retained until the binding is released).
A draw that names the target currently being drawn into throws
`TypeError` and records nothing (feedback guard). Sampled content renders
upright on every backend.

**Display list** — every record carries its target; `beginRenderTarget` /
`endRenderTarget` are records too and honor record order. The renderer's
reordering freedom stops at segment boundaries; segments play back in
first-record order, so a draw sampling a target always sees that target's
completed earlier segments.

```js
// main.js — F5a sample (current API)
efx.setClearColor([0.05, 0.05, 0.08, 1]);
const scene = efx.createRenderTarget({ width: 512, height: 512 });

efx.registerRenderHook(() => {
    efx.beginRenderTarget(scene);
    efx.drawQuad(96, 96, efx.whiteTexture, { size: [320, 320], color: [1, 0.4, 0.1, 1] });
    efx.endRenderTarget();

    efx.drawQuad(64, 0, scene, { size: [512, 512] });   // sample like a texture
});
```

### F5b — Post FX (current)

Scope from roadmap F5's second half: a full-screen post-effect chain, a
per-effect `mix`, and render-resolution decoupling (ADR 0029). The
declarative single-entry shape below supersedes the earlier provisional
`setColorFilter` / `setBlur` globals (never shipped — retired before
delivery, `f5b-post-fx`).

```js
// F5b · C — desktop binding `C · quickjs`, web binding `C · bridge`
efx.setPostEffects(list | null)   // [{ effect, ...options, mix? }], null/[] clears
efx.setRenderScale(scale, opts?)  // scene resolution vs surface; { filter: 'nearest' | 'linear' }
```

**Chain declaration** — `setPostEffects(list)` sets the frame's ordered
chain; `null` or `[]` clears it. Each entry is a plain object
`{ effect, ...options, mix? }`. The call validates **eagerly and
atomically**: a non-array `list`, a non-object entry, a missing or
unregistered `effect`, an unknown option field, or a wrongly-typed value
throws `TypeError`; an out-of-range number, a `mix` outside 0..1, or more
than 8 entries throws `RangeError` — and on throw the previously set chain
stays in effect. Entries are JS-managed: the engine **snapshots** their
values at call time, so later mutation of a script-held entry object does
not change the applied chain. The chain is plain engine state (the
`setClearColor` model): the most recent value at frame resolve applies and
it persists across frames until changed.

**Effect set (v1)** — each effect has pinned defaults; every entry may set
`mix` (0..1, default 1), the entry's input/output blend:

| Effect | Options (default) | Bounds | Passes |
|---|---|---|---|
| `colorFilter` | `brightness` (1), `contrast` (1), `saturation` (1), `tint` ([1,1,1,1]) | `brightness`/`contrast`/`saturation` finite ≥ 0; `tint` 4 finite components in 0..1 | one |
| `blur` | `radius` (1) | finite > 0 and ≤ 64 (scene pixels) | several internal (separable gaussian below an internal threshold, downsample chain above) |
| `bloom` | `threshold` (0.8), `strength` (0.5) | both finite in 0..1 | composite (bright pass → downsample blur → additive up) |

- `colorFilter` — `brightness` multiplies the color, `contrast` pivots at
  0.5 grey, `saturation` 0 yields fully desaturated (grey) output and 1 is
  unchanged, `tint` multiplies rgb (alpha ignored). With all defaults the
  output is the input (identity).
- `blur` — radius in scene pixels; the pass structure and downsampling are
  engine-owned and invisible (the script-visible result is only "blurred by
  radius").
- `bloom` — texels whose luminance is below `threshold` contribute nothing;
  `strength` is the additive contribution.
- The written result of every entry is `lerp(input, output, mix)` — `mix: 0`
  leaves the input unchanged. Entries apply in array order and the order is
  observable (a chain and its reverse differ deterministically).

**Resolve pipeline and fast path** — with no chain and `scale` 1 the frame
renders direct to the default target, byte-identical to the pre-F5b path
(every committed golden stays valid, no re-baselining). Otherwise the
default segment renders into an engine-owned **implicit scene target**
(sized by the render scale), the chain runs in array order through
engine-owned ping-pong temporaries, and the final pass blits to the default
target. The implicit scene target and temporaries are never script-visible
(no handles, no class). The chain applies to the **default target's
resolve only**: draws recorded into user RenderTargets render raw and their
sampled contents are unfiltered — only the final screen resolve passes
through the chain.

**Render scale** — `setRenderScale(scale, opts?)` sets the ratio between
the scene render resolution and the default target's size: `scale` a finite
number in (0, 2] (`RangeError` otherwise; default 1), `opts.filter` one of
`'nearest'` or `'linear'` (default `'linear'`; unknown fields or values
throw `TypeError`). The scene target is the surface size multiplied by
`scale`, rounded up; the final blit scales the scene to the surface with
the chosen filter (nearest gives crisp 2×2 blocks at half resolution,
linear interpolates). Render scale is orthogonal to the 2D camera frame:
the frame maps onto the scene target (the active rendering surface), which
then maps onto the output surface. It is plain engine state, applies to the
default target's resolve, and with scale 1 and no chain the fast path
renders direct.

```js
// main.js — F5b sample (current API)
efx.setCamera2D({ frame: [640, 480] });
efx.setPostEffects([
    { effect: 'colorFilter', brightness: 1.1, saturation: 0.6 },
    { effect: 'blur', radius: 4 },
    { effect: 'bloom', threshold: 0.8, strength: 0.5, mix: 0.5 },
]);
efx.setRenderScale(0.5, { filter: 'nearest' });

efx.setPostEffects(null);   // back to the byte-identical fast path
efx.setRenderScale(1);
```

### F6a — Resource loading (current)

Scope from roadmap F6's first slice: a resource root (directory or zip),
text/image loading, and the web boot that mounts a host-provided zip before
the entry script runs. Paths are relative to the resource root
(`res://`-style: `loadText('data/level.json')`). The root is set by the player
(`player <dir|zip>`, or `--script <file> [--root <dir|zip>]`), and on the web
by a host asset-root URL.

```js
// F6a · C · current — desktop binding `C · quickjs`, web binding `C · bridge`; identical semantics
efx.loadText(path)        // → string (UTF-8)
efx.loadImage(path)       // → ImageData (PNG/JPEG decoded to rgba8)
// A texture is the composed flow (F6e): no loadTexture convenience exists.
efx.createTexture(efx.loadImage(path), opts?) // → Texture
```

- `path` is a non-empty string; a non-string throws `TypeError`. A missing,
  unreadable, or undecodable resource throws a standard `Error`; no resource
  is returned. A path that escapes the root (`..` or an absolute path) throws
  `Error`.
- `loadImage` decodes PNG/JPEG to RGBA8; JPEG (no alpha) decodes fully opaque.
  The returned `ImageData` exposes read-only `width`/`height` and the usual
  `destroy()` lifecycle.
- Loading is synchronous on every target. On the web a host-provided zip is
  fetched and mounted once before `main.js` runs; scripts never see a promise
  or a loading hook.
- Without a resource root, `load*` throws `Error`.

```js
// main.js — F6a sample (current API)
const tex = efx.createTexture(efx.loadImage('images/logo.png'),
                              { mipmaps: true });
efx.log(efx.loadText('data/welcome.txt'));
efx.setCamera2D({ frame: [640, 480] });

efx.registerRenderHook(() => {
    efx.drawQuad(32, 32, tex, { size: [128, 128] });
});
```

### F6b — glTF static import (current)

Scope from roadmap F6's second slice: glTF 2.0 static import (geometry,
materials, textures) on top of the F6a provider. The profile — `.glb`/`.gltf`
containers, external/data-URI/buffer-view references, one selected mesh,
PBR→Phong conversion, per-texture samplers, and the extension/morph policy —
is pinned by ADR 0032.

```js
// F6b · C · current — desktop binding `C · quickjs`, web binding `C · bridge`; identical semantics
efx.loadMeshData(path, opts?)   // → MeshData — one surface per glTF primitive of
                                //   the selected mesh; materials converted and bound
efx.createTexture(imageData, opts?) // opts: { wrap?: 'repeat'|'clamp'|'mirror',
                                    //         filter?: 'linear'|'nearest',
                                    //         mipmaps?: boolean }
```

- `path` is a non-empty string; a non-string throws `TypeError`. A missing,
  malformed, or unsupported asset, an unknown mesh, a primitive count outside
  1..16, or an undecodable image throws a standard `Error`; no resource is
  returned.
- `opts.mesh` is a mesh index (non-negative integer) or a mesh name (string)
  and defaults to the first mesh. An unknown field throws `TypeError`; a
  `mesh` value that is neither throws `TypeError`; a name/index that matches no
  mesh throws `Error`.
- The import converts each primitive's material to the engine's Phong material
  (base color → diffuse, emissive → emissive, metallic → specular color,
  roughness → shininess; `alphaMode: MASK` → alpha mask) and binds it per
  surface; a primitive with no material uses the engine default. Occlusion and
  normal textures are ignored. Node/scene transforms are not applied.
- `createTexture`'s optional `opts.wrap` defaults to `'repeat'` and
  `opts.filter` to `'linear'`; `opts.mipmaps` (boolean, default `false`,
  F6e) builds and uses a full mip chain, with the chosen `filter` driving
  the mipmap filter. An unknown field or value throws `TypeError`.
  glTF samplers map onto `wrap`/`filter` (imported textures get no mip
  chain).

```js
// main.js — F6 sample (current API)
efx.setCamera3D({ pos: [0, 1, 4], target: [0, 0, 0], fov: 60 });
efx.setDirectionalLight({ dir: [0, -0.5, -1], color: [1, 1, 1, 1] });
const teapot = efx.createMesh(efx.loadMeshData('models/teapot.glb'));

efx.registerRenderHook(() => {
    efx.drawMesh(teapot);
});
```

### F6c — glTF rig import (current)

Scope from roadmap F6's third slice: skin + animation **import** on top of
F6b. `loadMeshData` fills each primitive's `JOINTS_0`/`WEIGHTS_0` into the
surface's `joints`/`weights` attributes and bundles the skin (joint
hierarchy + inverse bind matrices) and every `animations[]` clip into the
returned `MeshData` as an opaque rig payload, carried onto the `Mesh` by
`createMesh`. The model and interpolation policy are pinned by ADR 0033;
posing the imported rig is delivered by F7 (`poseMesh` + the `skinned` draw
option).

```js
// F6c · C · current — desktop binding `C · quickjs`, web binding `C · bridge`; identical semantics
efx.createMeshData({ positions, joints, weights, ... }) // skinned surface attributes
efx.loadMeshData(path, opts?) // additionally imports JOINTS_0/WEIGHTS_0 + skeleton + clips
```

- `joints`/`weights` are accepted by `createMeshData` exactly as described
  in the F3 MeshData section (four influences per vertex, paired, vertex
  count matched); they are CPU-only and not uploaded to the GPU in F6c.
- `loadMeshData` normalizes `JOINTS_0` component types (u8/u16) to the
  engine's integer joints, bundles the selected mesh's node→skin skeleton,
  and imports each clip with LINEAR/STEP exact and CUBICSPLINE approximated
  as LINEAR (tangents dropped). A primitive whose joints and weights counts
  do not match its vertices fails the import.
- **Opaque rig.** No resource, function, or read-only query property is
  added for the skeleton or clips; the only new script-visible data is the
  `joints`/`weights` surface attributes. The rig is released with its
  `MeshData`/`Mesh`.

### F6d — REPL (current)

The `--repl [<root>]` console run mode drives this same `efx` namespace
interactively: it opens the normal window/frame loop and evaluates each
stdin line in the persistent script context (state persists across
lines; a throwing line is printed and the run continues). An optional
root supplies the resource provider and runs its `main.js` once before
input, so `load*` works interactively.

It adds **no** script API. `.help` and `.exit` are host commands handled
by the player — not `efx` functions. The console is desktop
(embedded-runtime) only: on Emscripten the mode reports itself
unavailable rather than silently ignoring the request. See ADR 0007.

### F7 — Skinning & animation (current)

CPU linear-blend skinning of an imported glTF rig, driven by one stateless
call; no engine playback state (ADR 0017/0018).

```js
// F7 · C · current — skin, skeleton, and clips are implicit Mesh payload (ADR 0017)
efx.poseMesh(mesh, pose)   // pose: { clip, time, weight? } or [ samples ]; CPU-poses in place
efx.drawMesh(mesh, { transform?, color?, skinned? }) // skinned: true → current posed buffer
```

- A skinned asset loads as one Mesh carrying its rig: per-surface
  `joints`/`weights` (glTF `JOINTS_0`/`WEIGHTS_0`), the skeleton (joint
  hierarchy + inverse bind matrices, glTF-style), and every animation clip.
  No rig resource, no clip/joint query property, and no playback function are
  exposed to scripts — the script owns the clock.
- `efx.poseMesh(mesh, pose)` CPU-poses a live skinned Mesh **in place**.
  `pose` is a single sample `{ clip, time, weight? }` or an array of such
  samples (a weighted blend). `clip` is a clip name (the glTF `name`, or the
  stable internal `clipN` when unnamed) or a clip index. `time` is in seconds
  and wraps modulo the clip's length; array weights are normalized
  engine-side, and a single sample ignores its weight.
- Errors: a non-Mesh/destroyed Mesh, a rig-less Mesh, a mistyped field, or an
  unknown sample field throws `TypeError`; an out-of-range clip index or a
  negative weight throws `RangeError`; an unknown clip name throws `Error`.
- `efx.drawMesh(mesh, { skinned: true })` draws the current CPU-posed
  vertices; absent/`false` draws the retained bind-pose buffer (posing never
  mutates it; ~2× vertex memory for skinned meshes). `skinned: true` on a mesh
  without a rig throws `TypeError`. The flag is per-draw, like `color`.
- No stateful playback helper (`play`/`pause`/`blend`) exists; such a
  convenience would be a pure-JS layer over `poseMesh` (an F8 candidate).

```js
// main.js — F7 sample
efx.setCamera3D({ pos: [0, 1.5, 4], target: [0, 1, 0], fov: 60 });
const hero = efx.createMesh(efx.loadMeshData('actors/hero.gltf')); // geometry + rig + clips

let t = 0;
efx.registerUpdateHook(dt => {
    t += dt;
    const k = Math.min(1, t / 2); // walk → run cross-fade over 2s
    efx.poseMesh(hero, [
        { clip: 'Walk', time: t, weight: 1 - k },
        { clip: 'Run',  time: t, weight: k },
    ]);
});

efx.registerRenderHook(() => {
    efx.drawMesh(hero, { skinned: true }); // current CPU-skinned pose
    // efx.drawMesh(hero);                 // bind (rest) pose
});
```

### F8 — High-level drawing (provisional)

Scope from roadmap F8: high-level JS layer — `drawModel`, `drawText` (font
atlas built on quads), demo resource pack. These are engine-bundled pure ES6
built only on the public `[C]` API above.

```js
// F8 · JS · provisional
efx.loadFont(path)                   // → font object (JS-managed: atlas Texture + quad layout)
efx.drawModel(mesh, mat?, opts?)     // { transform?, skinned? } — pure-JS convenience:
                                     // binds mat to every surface lacking a bound
                                     // material, then drawMesh (ADR 0024)
efx.drawText(text, x, y, opts)       // { font, size?, color? } — text as quads
```

```js
// main.js — F8 sample (provisional API)
efx.setClearColor([0.08, 0.09, 0.12, 1]);
efx.setCamera3D({ pos: [0, 2, 5], target: [0, 0, 0], fov: 60 });
const teapot = efx.loadMesh('models/teapot.mesh');
const font = efx.loadFont('fonts/perfect.ttf');

let yaw = 0;
efx.registerUpdateHook(dt => { yaw += dt * 30; });

efx.registerRenderHook(() => {
    efx.drawModel(teapot, {
        diffuse:  { color: [0.8, 0.3, 0.2, 1] },
        specular: { color: [1, 1, 1, 1], shininess: 32 },
    }, { transform: efx.mat4.rotate(efx.mat4.identity(), yaw, [0, 1, 0]) });
    efx.drawText('score: 1200', 24, 24, { font, size: 32, color: [1, 1, 1, 1] });
});
```

### F9 — Input: keyboard, mouse & window (provisional)

Keyboard and mouse input as three sub-namespaces of the single `efx` object,
with a query API for polling current state and an event API with callbacks
returning unsubscribe functions (ADR 0036). Input adds **no resource types**:
no native-backed class, no `destroy()`, no slot bank.

```js
// F9 · C · provisional — namespaces on the single efx object (ADR 0004/0036)
efx.keyboard.isDown(key) / isPressed(key) / isReleased(key)   // → boolean
efx.keyboard.onDown(fn) / onUp(fn) / onChar(fn)               // → unsubscribe
efx.mouse.isDown(button) / isPressed(button) / isReleased(button)
efx.mouse.onDown(fn) / onUp(fn) / onMove(fn) / onWheel(fn)
efx.mouse.position  // [x, y] surface px      efx.mouse.delta  // [dx, dy]
efx.mouse.x / y     // surface px             efx.mouse.wheel  // [dx, dy]
efx.window.size     // [width, height] surface px
efx.window.width / height / dpiScale
```

- **Semantics.** Level state updates on arrival; `isPressed`/`isReleased`
  are the up-to-down / down-to-up edges, valid for exactly one frame (an
  auto-repeat does not re-raise the press edge). Movement/wheel are
  per-frame deltas. Callbacks drain once per frame in arrival order **before**
  the update hooks, so a callback never runs outside a frame and queries and
  events agree within a frame. Focus loss clears held state without emitting
  synthetic up events.
- **Event arguments** are one plain object of engine primitives — never a
  DOM/host event: `{ key, repeat, mods }` (key down), `{ key, mods }` (key
  up), `{ char }` (text input), `{ button, x, y, mods }` (mouse down/up),
  `{ x, y, dx, dy }` (mouse move), `{ dx, dy }` (wheel). `key`/`button` are
  name strings; `char` is the decoded text (e.g. `'A'`); `mods` is an array
  of active modifier names (`'shift'`, `'ctrl'`, `'alt'`, `'super'`).
- **Key names** (lowercase) include `a`–`z`, `0`–`9`, `f1`–`f12`, the arrows
  `left`/`right`/`up`/`down`, `space`, `enter`, `escape`, `tab`, `backspace`,
  `insert`, `delete`, `home`, `end`, `pageup`, `pagedown`, `lshift`/`rshift`,
  `lctrl`/`rctrl`, `lalt`/`ralt`, `lsuper`/`rsuper`, the punctuation set
  (`apostrophe`, `comma`, `minus`, `period`, `slash`, `semicolon`, `equal`,
  `leftbracket`, `backslash`, `rightbracket`, `grave`), the keypad set
  (`kp0`–`kp9`, `kpdecimal`, `kpdivide`, `kpmultiply`, `kpsubtract`, `kpadd`,
  `kpenter`, `kpequal`), the locks (`capslock`, `scrolllock`, `numlock`,
  `printscreen`, `pause`), and `menu`. Mouse buttons are `left`, `right`,
  `middle`.
- **Coordinates** are **surface (framebuffer) pixels** with a top-left origin
  and y down — the same space as `drawQuad` and the 2D frame — so hit-testing
  against drawn content needs no conversion. On high-DPI displays the surface
  is larger than the logical window; `efx.window.dpiScale` is the
  surface-to-logical ratio for scripts that want logical units.
- **Errors.** Registration requires a function and throws `TypeError`
  otherwise; a query with an unknown key or button name throws `TypeError`;
  each registration returns an idempotent unsubscribe function.
- **Simulation.** A deterministic injection seam (`efx_input_inject_*`) exists
  for tests only and is not part of the script API — no simulation function
  is exposed on `efx`.

```js
// main.js — F9 sample
efx.registerUpdateHook(dt => {
    if (efx.keyboard.isPressed('space')) startJump();
    if (efx.keyboard.isDown('left')) x -= speed * dt;
});
const offMove = efx.mouse.onMove(e => { aimX = e.x; aimY = e.y; });
efx.mouse.onWheel(e => { zoom *= (1 + e.dy * 0.1); });
```

## Vision traceability

Every consumer-API property named in `vision.md` maps to exactly one catalog
section (or an open question below):

| vision.md property | Where |
|---|---|
| 2D drawing via quads | F2 |
| Additive and subtractive blending modes | F2 (`setBlendMode`) |
| 1 camera fixed | F2 `setCamera2D`, F3 `setCamera3D`, limits table |
| Rendering meshes | F3 (`createMesh` / `drawMesh`) |
| Vertex colours | F3 (per-surface `colors?` attribute, `drawMesh` tint) |
| Matrix math | F3 (`efx.mat4` / `efx.vec3` / `efx.quat`) |
| Procedural primitives | F3 (`makeCube` / `makePlane` / `makeSphere`) |
| 4 point lights, 1 directional light | F4, limits table |
| Phong material system, 4 channels + maps | F4a/F4b (`setMeshSurfaceMaterial`) |
| Alpha masks | F4b (`alphaMask`) |
| Rendering to textures | F5a (render targets — `createRenderTarget` / `beginRenderTarget`) |
| Simple post processing (color filter, blur) | F5b (`setPostEffects`, current) |
| Resource folder / zip root (`res://`-like) | F6a (load paths + dir/zip provider, current) |
| REPL console mode | F6d (drives the same `efx` namespace) |
| Skinning and animations | F7 (`poseMesh`, `drawMesh({ skinned })`, current) |
| Keyboard/mouse input query + events | F9 (`efx.keyboard`/`efx.mouse`/`efx.window`, provisional) |
| High-level functions in pure JS (`drawModel`, `drawText`) | F8 |
| Callbacks for update and rendering | F1 (Lifecycle hooks — explicit registration, ADR 0016) |
| Low/mid C + high-level JS layering | Overview (two layers), every entry tag |
| No browser/Node dependencies (incl. transitively) | Conventions (Dependencies) |
| Handles (resource objects) or pre-allocated slots for unmanaged resources | Resource & memory model |
| Fixed-function pipeline (no consumer-facing programmable shaders) | Engine-internal constraint — shapes what the API can express; internals use Sokol canned shaders (ADR 0015); no API entry |
| Immediate-mode API with re-orderable display list | Overview (immediate mode, deferred rendering) |
| Single-binary player for resource folders | Player runtime, not this API — see `openspec/specs` (`player-runtime`) |

## Open questions

Flagged gaps and deferred decisions — recorded here rather than inventing
API for them:

- **Audio** — absent from vision.md. Same treatment as input.
- **Asset format** — glTF 2.0 is pinned as the import format (meshes,
  images, skins, animation clips — roadmap F6, data model per ADR 0014).
  F6a delivered the resource root (directory or zip) and text/image loading;
  F6b delivered static import and pinned the profile (containers, references,
  material mapping, samplers, extension policy — ADR 0032). Skin/animation
  payload is F6c.
- **Procedural rigs** — F7 bundles skins/skeletons/clips at *import* only;
  constructing a rig procedurally (from `createMeshData` + skeleton data)
  has no path yet. Deferred until a concrete need appears.
- **Clip naming** — settled by F7: `poseMesh` accepts a clip name (the glTF
  `name`, or the stable internal `clipN` when unnamed) or a clip index
  (ADR 0033).
- **Stateful playback helper** — play/pause/blend convenience as pure JS
  over `poseMesh` is an F8-layer candidate, not engine state (ADR 0018).
- **REPL introspection helpers** — whether the F6 console mode needs extra
  `efx` functions beyond the interactive namespace is deferred to F6.
