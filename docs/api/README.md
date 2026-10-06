**EFX API**

***

# EFX

**EFX** is a lightweight, fixed-function 3D engine with an embedded ES6
scripting layer. It targets PS2-era graphics and is deliberately small: one
global `efx` object, no imports, no setup, and no browser or Node APIs. Game
scripts run unchanged on Windows, Linux, macOS, and the browser.

## Start with `efx`

Everything lives on the single global [efx](variables/efx.md). It exposes the runtime
facilities (`log`, `quit`, `args`, and the frame hooks) and a set of domain
sub-namespaces:

- [efx.graphics](interfaces/EfxGraphics.md) — 2D/3D drawing, cameras, lights,
  materials, text, post FX, and GPU resources.
- [efx.math](interfaces/EfxMath.md) — pure-JS `mat4`, `vec3`, and `quat` helpers.
- [efx.io](interfaces/EfxIo.md) — synchronous loaders for text and binary resources.
- [efx.physics](interfaces/EfxPhysics.md) — the single physics world: bodies, a
  character controller, and queries.
- [efx.keyboard](interfaces/EfxKeyboard.md), [efx.mouse](interfaces/EfxMouse.md),
  [efx.gamepad](interfaces/EfxGamepad.md), [efx.window](interfaces/EfxWindow.md) — input
  queries and events.
- [efx.audio](interfaces/EfxAudio.md) — streamed music and engine-mixed sound
  effects.
- [efx.color](interfaces/EfxColor.md) — named color constants.

## The smallest complete scene

```js
// A lit, spinning cube on a dark stage.
efx.graphics.setClearColor([0.03, 0.04, 0.09, 1]);
efx.graphics.setCamera3D([0, 1.6, 4.2], [0, 0, 0], 60);
efx.graphics.setLight(0, { pos: [2.6, 3.6, 3.0], color: [1, 0.95, 0.9, 1], range: 30 });
efx.graphics.setDirectionalLight({ dir: [-0.4, -1.0, -0.3], color: [0.18, 0.2, 0.26, 1] });

const cube = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.4 }));
cube.setSurfaceMaterial(0, {
  ambient:  { color: [0.12, 0.12, 0.16, 1] },
  diffuse:  { color: efx.color.white },
  specular: { color: efx.color.white, shininess: 32 },
  emissive: { color: efx.color.black },
});

let t = 0;
function update(dt) { t += dt; }
function render() {
  const spin = efx.math.mat4.rotate(efx.math.mat4.identity(), t * 40, [0, 1, 0]);
  const tilt = efx.math.mat4.rotate(spin, 18, [1, 0, 0]);
  efx.graphics.drawMesh(cube, { transform: tilt, color: [0.95, 0.5, 0.2, 1] });
}
```

## Going deeper

This reference is generated from the type document
`gallery/src/api/efx.d.ts`, which is the single source of truth for every
symbol below. The conventions and design rules behind the API are documented
in the repository guidelines `docs/js-api.md`.

## Start Here

The `efx` global and the sub-namespaces it offers.

| Variable | Description |
| ------ | ------ |
| [efx](variables/efx.md) | The engine-provided script surface; the only global scripts use. It exposes the runtime and lifecycle facilities (`log`, `quit`, the read-only `args`, and the frame-hook registrations) directly, and groups the rest of the engine into domain sub-namespaces: `graphics`, `math`, `io`, `physics`, `keyboard`, `mouse`, `gamepad`, `window`, `audio`, and `color`. |

## Graphics

2D/3D drawing, cameras, lights, materials, text, post FX, and GPU resources.

| Name | Description |
| ------ | ------ |
| [EfxFont](interfaces/EfxFont.md) | A baked glyph atlas plus layout metrics (opaque native-backed class). |
| [EfxFontData](interfaces/EfxFontData.md) | A parsed TrueType/OpenType font, CPU only (opaque native-backed class). |
| [EfxGraphics](interfaces/EfxGraphics.md) | The graphics surface: 2D and 3D drawing, camera and light state, post processing, and the graphics resource factories. Reached as `efx.graphics`; names, signatures, semantics, and error behavior are unchanged by the move from the `efx` root. |
| [EfxImageData](interfaces/EfxImageData.md) | Raw CPU pixels plus size and format (opaque native-backed class). |
| [EfxMesh](interfaces/EfxMesh.md) | A GPU mesh uploaded from MeshData (opaque native-backed class). |
| [EfxMeshData](interfaces/EfxMeshData.md) | CPU mesh data holding 1..16 surfaces (opaque native-backed class). |
| [EfxParticleSystem](interfaces/EfxParticleSystem.md) | A native-backed CPU particle system. |
| [EfxRenderTarget](interfaces/EfxRenderTarget.md) | A GPU render target with color and depth attachments (opaque native-backed class). |
| [EfxTexture](interfaces/EfxTexture.md) | A GPU texture (opaque native-backed class). |
| [EfxSample](type-aliases/EfxSample.md) | A live Texture or RenderTarget — accepted anywhere a texture is sampled. |

## Math

Pure-JS `mat4`, `vec3`, and `quat` helpers.

| Interface | Description |
| ------ | ------ |
| [EfxMat4](interfaces/EfxMat4.md) | Pure-JS 4×4 matrix helpers (inputs are never mutated). |
| [EfxMath](interfaces/EfxMath.md) | Pure-JS math helpers, reached as `efx.math`. Each helper is a pure function that never mutates its arguments and returns plain JS data (column-major `Mat4` arrays, `Vec3` arrays, `Quat` arrays). |
| [EfxQuat](interfaces/EfxQuat.md) | Pure-JS quaternion helpers (inputs are never mutated). |
| [EfxVec3](interfaces/EfxVec3.md) | Pure-JS 3-component vector helpers (inputs are never mutated). |

## Input

Keyboard, mouse, window, and gamepad queries and events.

| Interface | Description |
| ------ | ------ |
| [EfxGamepad](interfaces/EfxGamepad.md) | Gamepad queries and connect/disconnect events over a fixed engine-owned bank of four pad slots, reported by index. |
| [EfxGamepadView](interfaces/EfxGamepadView.md) | A pad slot view: plain data plus query methods. Not a resource — there is nothing to create or destroy. |
| [EfxKeyboard](interfaces/EfxKeyboard.md) | Keyboard queries and event subscriptions. Queries report current frame state; `isPressed`/`isReleased` are one-frame edges. Each event registration returns an idempotent unsubscribe function. |
| [EfxMouse](interfaces/EfxMouse.md) | Mouse queries and event subscriptions. Queries report current frame state; `isPressed`/`isReleased` are one-frame edges. Each event registration returns an idempotent unsubscribe function. All coordinates are surface (framebuffer) pixels with a top-left origin and y down — the same space as `drawQuad` and the 2D frame. |
| [EfxWindow](interfaces/EfxWindow.md) | Read-only window metrics, in surface (framebuffer) pixels. On high-DPI displays the surface is larger than the logical window; `dpiScale` is the surface-to-logical ratio. |

## Physics

The single physics world: bodies, a character controller, and queries.

| Interface | Description |
| ------ | ------ |
| [EfxBody](interfaces/EfxBody.md) | A native-backed collider in the single physics world. The world holds it until `destroy()` or `physics.clear()`: dropping the last script reference does not remove it from the simulation. |
| [EfxCharacter](interfaces/EfxCharacter.md) | A native-backed kinematic capsule character controller. The world holds it until `destroy()` or `physics.clear()`: dropping the last script reference does not remove it from the simulation. |
| [EfxPhysics](interfaces/EfxPhysics.md) | The single physics world. The script owns stepping: call `step(dt)` each frame and the engine never advances the world on its own. |

## Audio

Streamed music and engine-mixed sound effects.

| Interface | Description |
| ------ | ------ |
| [EfxAudio](interfaces/EfxAudio.md) | Audio playback. The engine owns all mixing: scripts never see channels, buses, or buffers. Two source kinds are loaded separately from playback: `AudioData` holds fully-decoded PCM and can back many overlapping playheads; `AudioStream` decodes a long resource incrementally. WAV and MP3 resources are supported; only decoded PCM is played (no sequenced/modular formats). All volume control is per-handle plus the single master `volume`; fades are plain handle writes. |
| [EfxAudioData](interfaces/EfxAudioData.md) | Fully-decoded PCM loaded from the resource root (opaque native-backed class). |
| [EfxAudioHandle](interfaces/EfxAudioHandle.md) | One playing audio handle (opaque native-backed class). |
| [EfxAudioStream](interfaces/EfxAudioStream.md) | A streamed audio resource; each `playAudio` opens an independent decoder (opaque native-backed class). |

## IO

Synchronous resource loaders.

| Interface | Description |
| ------ | ------ |
| [EfxIo](interfaces/EfxIo.md) | Synchronous resource loaders, reached as `efx.io`. Paths are relative to the resource root (directory or zip) and obey its escape rules; a non-string path throws `TypeError` and a missing, unreadable, or escaping path throws a standard `Error`. |

## Values

Shared value records, primitive aliases, and descriptors.

| Name | Description |
| ------ | ------ |
| [BoxShape](interfaces/BoxShape.md) | An axis-aligned box collider. |
| [CapsuleShape](interfaces/CapsuleShape.md) | A vertical capsule collider. |
| [EfxColor](interfaces/EfxColor.md) | Named color constants, reached as `efx.color`: the CSS basic 16 plus `transparent`. Each is a `Color` (`[r, g, b, a]`) frozen at runtime, so a script cannot mutate engine state through it. There are no functions here. |
| [FontOutline](interfaces/FontOutline.md) | A baked outline ring around glyphs. |
| [FontShadow](interfaces/FontShadow.md) | A baked blurred shadow behind glyphs. |
| [Material](interfaces/Material.md) | A per-surface Phong material. It is JS-managed (no native handle, no `destroy()`) and the engine snapshots it at binding time, so later mutation of the script object does not change the bound material. |
| [MeshShape](interfaces/MeshShape.md) | A static triangle-mesh collider built from a live Mesh. |
| [MeshSurfaceData](interfaces/MeshSurfaceData.md) | One mesh surface's attribute arrays (a Godot surface / glTF primitive). |
| [PhongChannel](interfaces/PhongChannel.md) | An ambient/diffuse/emissive Phong channel. |
| [PoseSample](interfaces/PoseSample.md) | One pose sample: a clip sampled at `time` (seconds) with an optional blend `weight` (normalized engine-side across an array; a single sample ignores it). `clip` is a clip name or an index. |
| [SourceRect](interfaces/SourceRect.md) | A `{ x, y, w, h }` rectangle in texture pixels. |
| [SpecularChannel](interfaces/SpecularChannel.md) | The specular Phong channel (adds a shininess exponent). |
| [SphereShape](interfaces/SphereShape.md) | A sphere collider. |
| [Color](type-aliases/Color.md) | An RGBA color: four normalized floats in `0..1`, ordered `[r, g, b, a]`. Most lighting and material channels ignore the alpha component. |
| [FlatIndices](type-aliases/FlatIndices.md) | Flat index arrays: a plain array or a typed array of unsigned integers. |
| [FlatNumbers](type-aliases/FlatNumbers.md) | Flat floating-point attribute arrays: a plain array or a typed array. |
| [Mat4](type-aliases/Mat4.md) | A column-major 4×4 matrix as a flat 16-number array. |
| [PhysicsShape](type-aliases/PhysicsShape.md) | A collision shape accepted by bodies and by the spatial queries. |
| [Quat](type-aliases/Quat.md) | A quaternion `[x, y, z, w]`. |
| [Vec2](type-aliases/Vec2.md) | A 2-component vector `[x, y]` (frame pixels for 2D APIs, world units for 3D). |
| [Vec3](type-aliases/Vec3.md) | A 3-component vector `[x, y, z]` in world units. |

## Configuration

Option and parameter bags that are not inlined into their operations.

| Name | Description |
| ------ | ------ |
| [BloomPostEffect](interfaces/BloomPostEffect.md) | A bloom post effect. |
| [BlurPostEffect](interfaces/BlurPostEffect.md) | A separable gaussian blur post effect. |
| [ColorFilterPostEffect](interfaces/ColorFilterPostEffect.md) | A color-filter post effect (identity with all defaults). |
| [EmissionShapeOptions](interfaces/EmissionShapeOptions.md) | Emission volume for a particle system. |
| [ParticleSystemCreateOptions](interfaces/ParticleSystemCreateOptions.md) | Options for `createParticleSystem` (`texture`, `max`, and `lifetime` are positional). |
| [ParticleSystemOptions](interfaces/ParticleSystemOptions.md) | Full particle configuration; the creation inputs (`texture`, `max`, `lifetime`) are positional on `createParticleSystem`. |
| [EfxPostEffect](type-aliases/EfxPostEffect.md) | One entry of the declarative post-effect chain (`setPostEffects`). |
| [ParticleSystemSetOptions](type-aliases/ParticleSystemSetOptions.md) | Partial update bag for `ParticleSystem.set`. |

## Events

Payloads delivered to event callbacks.

| Interface | Description |
| ------ | ------ |
| [CharEvent](interfaces/CharEvent.md) | Payload of a text-input event. |
| [KeyboardDownEvent](interfaces/KeyboardDownEvent.md) | Payload of a key-down event. |
| [KeyboardUpEvent](interfaces/KeyboardUpEvent.md) | Payload of a key-up event. |
| [MouseButtonEvent](interfaces/MouseButtonEvent.md) | Payload of a mouse button event. |
| [MouseMoveEvent](interfaces/MouseMoveEvent.md) | Payload of a mouse move event. |
| [MouseWheelEvent](interfaces/MouseWheelEvent.md) | Payload of a mouse wheel event. |

## Enums

Fixed string-union identifier sets.

| Type Alias | Description |
| ------ | ------ |
| [EfxBlendMode](type-aliases/EfxBlendMode.md) | Blend mode for 2D quads, sprites, and particle batches. |
| [EfxFacing](type-aliases/EfxFacing.md) | Quad render mode for world-space billboards and particles. |
| [EfxGamepadAxis](type-aliases/EfxGamepadAxis.md) | The engine-owned semantic gamepad axis identifier set. |
| [EfxGamepadButton](type-aliases/EfxGamepadButton.md) | The engine-owned semantic gamepad button identifier set. |
| [EfxKey](type-aliases/EfxKey.md) | The engine-owned lowercase keyboard identifier set. |
| [EfxMod](type-aliases/EfxMod.md) | An active keyboard modifier name reported in an event's `mods`. |
| [EfxMouseButton](type-aliases/EfxMouseButton.md) | The engine-owned mouse button identifier set. |

## Results

Records returned by queries and layout operations.

| Interface | Description |
| ------ | ------ |
| [PhysicsContact](interfaces/PhysicsContact.md) | One contact reported on a dynamic body's `contacts` list. |
| [PhysicsMoveCollision](interfaces/PhysicsMoveCollision.md) | One collision reported by `Character.moveAndSlide`. |
| [PhysicsMoveResult](interfaces/PhysicsMoveResult.md) | The result of `Character.moveAndSlide`. |
| [PhysicsRayHit](interfaces/PhysicsRayHit.md) | One raycast hit. |
| [PhysicsShapeHit](interfaces/PhysicsShapeHit.md) | One shape-cast hit. |
| [TextBounds](interfaces/TextBounds.md) | Laid-out text bounds returned by `drawText` / `Font.measure`. |
