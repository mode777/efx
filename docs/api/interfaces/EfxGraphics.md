[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxGraphics

# Interface: EfxGraphics

The graphics surface: 2D and 3D drawing, camera and light state, post
processing, and the graphics resource factories. Reached as
`efx.graphics`; names, signatures, semantics, and error behavior are
unchanged by the move from the `efx` root.

## Methods

### beginRenderTarget()

> **beginRenderTarget**(`rt`): `void`

Redirect subsequently recorded draws into a render target, clearing it on entry.

#### Parameters

##### rt

[`EfxRenderTarget`](EfxRenderTarget.md)

Target to draw into.

#### Returns

`void`

***

### createFont()

> **createFont**(`fontData`, `size`, `opts?`): [`EfxFont`](EfxFont.md)

Bake a fixed glyph atlas from FontData.

#### Parameters

##### fontData

[`EfxFontData`](EfxFontData.md)

Parsed source font.

##### size

`number`

Pixel size baked into the atlas (must be > 0).

##### opts?

Optional charset, gutter, filter, and baked effects.

###### filter?

`"nearest"` \| `"linear"`

Atlas sampler filter (default `'linear'`).

###### glyphs?

`string`

Codepoints to bake; defaults to the printable Latin-1 set.

###### outline?

[`FontOutline`](FontOutline.md) \| `null`

Baked outline ring; `null`/omitted bakes none.

###### padding?

`number`

Atlas gutter in pixels (default 1).

###### shadow?

[`FontShadow`](FontShadow.md) \| `null`

Baked blurred shadow; `null`/omitted bakes none.

#### Returns

[`EfxFont`](EfxFont.md)

The baked Font.

***

### createImageData()

> **createImageData**(`width`, `height`, `pixels`, `opts?`): [`EfxImageData`](EfxImageData.md)

Build CPU pixels as an ImageData.

#### Parameters

##### width

`number`

Image width in pixels (must be > 0).

##### height

`number`

Image height in pixels (must be > 0).

##### pixels

`number`[] \| `Uint8Array`\<`ArrayBufferLike`\>

Flat RGBA8 bytes of length `width * height * 4`.

##### opts?

Optional format (reserved for future options).

###### format?

`"rgba8"`

Pixel format; only `'rgba8'` is supported (default `'rgba8'`). Reserved for future options.

#### Returns

[`EfxImageData`](EfxImageData.md)

The new ImageData.

***

### createMesh()

> **createMesh**(`meshData`): [`EfxMesh`](EfxMesh.md)

Upload all surfaces of MeshData to a GPU mesh.

#### Parameters

##### meshData

[`EfxMeshData`](EfxMeshData.md)

Source CPU mesh data.

#### Returns

[`EfxMesh`](EfxMesh.md)

The new GPU mesh.

***

### createMeshData()

> **createMeshData**(`surfaces`, `materials?`): [`EfxMeshData`](EfxMeshData.md)

Build multi-surface MeshData from a surface list.

#### Parameters

##### surfaces

[`MeshSurfaceData`](MeshSurfaceData.md)[]

1..16 surfaces, each a Godot surface / glTF primitive.

##### materials?

([`Material`](Material.md) \| `null`)[]

Optional parallel array; `null` selects the engine default.

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The new CPU MeshData.

***

### createParticleSystem()

> **createParticleSystem**(`texture`, `max`, `lifetime`, `opts?`): [`EfxParticleSystem`](EfxParticleSystem.md)

Create a native-backed CPU particle system.

#### Parameters

##### texture

[`EfxSample`](../type-aliases/EfxSample.md)

Live texture or render target for every particle quad.

##### max

`number`

Maximum live particles (integer, 1..65536).

##### lifetime

`number` \| \[`number`, `number`\]

Particle lifetime in seconds: a number or `[min, max]`.

##### opts?

[`ParticleSystemCreateOptions`](ParticleSystemCreateOptions.md)

Optional emitter, motion, appearance, and blending options.

#### Returns

[`EfxParticleSystem`](EfxParticleSystem.md)

The new particle system.

***

### createRenderTarget()

> **createRenderTarget**(`width`, `height`): [`EfxRenderTarget`](EfxRenderTarget.md)

Create a GPU render target with a color and depth attachment.

#### Parameters

##### width

`number`

Target width in pixels (positive integer, 1..4096).

##### height

`number`

Target height in pixels (positive integer, 1..4096).

#### Returns

[`EfxRenderTarget`](EfxRenderTarget.md)

The new render target.

***

### createTexture()

> **createTexture**(`imageData`, `opts?`): [`EfxTexture`](EfxTexture.md)

Upload ImageData to a GPU texture.

#### Parameters

##### imageData

[`EfxImageData`](EfxImageData.md)

Source pixels.

##### opts?

Optional wrap, filter, and mipmap settings.

###### filter?

`"nearest"` \| `"linear"`

Texture filter (default `'linear'`); also drives the mipmap filter.

###### mipmaps?

`boolean`

Build and use a full mip chain (default `false`).

###### wrap?

`"repeat"` \| `"clamp"` \| `"mirror"`

Texture wrap mode (default `'repeat'`).

#### Returns

[`EfxTexture`](EfxTexture.md)

The new texture.

***

### drawBillboard()

> **drawBillboard**(`texture`, `pos`, `opts?`): `void`

Record one world-space billboard quad.

#### Parameters

##### texture

[`EfxSample`](../type-aliases/EfxSample.md)

Live texture or render target to sample (the thing drawn leads).

##### pos

[`Vec3`](../type-aliases/Vec3.md)

World position `[x, y, z]`.

##### opts?

Optional size, tint, facing, and depth options.

###### blend?

[`EfxBlendMode`](../type-aliases/EfxBlendMode.md)

Blend mode for this billboard; overrides the frame's blend state for this draw only.

###### color?

[`Color`](../type-aliases/Color.md)

Tint `[r, g, b, a]` (default opaque white).

###### depthTest?

`boolean`

Depth-test against opaque geometry (default `true`); never writes depth.

###### facing?

[`EfxFacing`](../type-aliases/EfxFacing.md)

`'view'` (default, full camera-facing), `'y'` (world-up), or `'plane'` (fixed oriented plane).

###### normal?

[`Vec3`](../type-aliases/Vec3.md)

Plane orientation normal for `facing: 'plane'` (default `[0, 1, 0]`).

###### rotation?

`number`

In-plane rotation in degrees (default 0).

###### size?

`number` \| [`Vec2`](../type-aliases/Vec2.md)

World-unit size: a single number or `[w, h]` (default 1).

###### sourceRect?

[`SourceRect`](SourceRect.md)

Texture-pixel atlas region; defaults to the full texture.

#### Returns

`void`

***

### drawMesh()

> **drawMesh**(`mesh`, `opts?`): `void`

Draw a whole mesh, depth-tested, under the recorded 3D camera.

#### Parameters

##### mesh

[`EfxMesh`](EfxMesh.md)

Live mesh to draw (required positional argument).

##### opts?

Optional transform, tint, and skinned flag.

###### color?

[`Color`](../type-aliases/Color.md)

Tint multiplying vertex colors (default opaque white).

###### depthWrite?

`boolean`

Depth writing (default `true`). When `false`, the mesh is depth-tested but
does not write depth, so later geometry is never occluded by it — used to
draw a camera-locked sky before the scene.

###### skinned?

`boolean`

`true` draws the current CPU-posed vertices; absent/false the bind pose.

###### transform?

[`Mat4`](../type-aliases/Mat4.md)

Column-major transform (default identity).

#### Returns

`void`

***

### drawParticles()

> **drawParticles**(`system`): `void`

Record one batch for a particle system's live particles.

#### Parameters

##### system

[`EfxParticleSystem`](EfxParticleSystem.md)

System whose live particles to draw.

#### Returns

`void`

***

### drawQuad()

> **drawQuad**(`texture`, `x`, `y`, `opts?`): `void`

Record one textured quad.

#### Parameters

##### texture

[`EfxSample`](../type-aliases/EfxSample.md)

Live texture or render target to sample (the thing drawn leads).

##### x

`number`

Quad top-left x in frame pixels.

##### y

`number`

Quad top-left y in frame pixels.

##### opts?

Optional tint, transform, size, origin, source rect, and blend.

###### blend?

[`EfxBlendMode`](../type-aliases/EfxBlendMode.md)

Blend mode for this quad; overrides the frame's blend state for this draw only.

###### color?

[`Color`](../type-aliases/Color.md)

Tint `[r, g, b, a]` (default opaque white).

###### origin?

[`Vec2`](../type-aliases/Vec2.md)

Pivot `[px, py]` in quad-local pixels for rotation/scale (default the size's center).

###### rotation?

`number`

Rotation in degrees clockwise (default 0).

###### scale?

`number`

Uniform scale factor applied after the size is determined (default 1, must be > 0).

###### size?

[`Vec2`](../type-aliases/Vec2.md)

Quad size `[width, height]` in frame pixels; defaults to the source rect or texture size.

###### sourceRect?

[`SourceRect`](SourceRect.md)

Texture-pixel region to sample; defaults to the full texture.

#### Returns

`void`

***

### drawSprites()

> **drawSprites**(`texture`, `sprites`, `opts?`): `void`

Record a batch of 2D sprite quads from one texture.

#### Parameters

##### texture

[`EfxSample`](../type-aliases/EfxSample.md)

Live texture or render target to sample.

##### sprites

`object`[]

One options bag per quad; validation is atomic.

##### opts?

Optional batch-level blend mode.

###### blend?

[`EfxBlendMode`](../type-aliases/EfxBlendMode.md)

Blend mode applied to every sprite in the batch; overrides the frame's blend state.

#### Returns

`void`

***

### drawText()

> **drawText**(`text`, `font`, `x`, `y`, `opts?`): [`TextBounds`](TextBounds.md)

Lay out and record 2D text quads.

#### Parameters

##### text

`string`

Text to draw (supports newlines).

##### font

[`EfxFont`](EfxFont.md)

Baked font to draw with.

##### x

`number`

Anchor x in frame pixels.

##### y

`number`

Anchor y in frame pixels.

##### opts?

Optional alignment, wrap, colors, rotation, and scale.

###### align?

`"left"` \| `"center"` \| `"right"` \| `"justify"`

Horizontal alignment (default `'left'`); `'justify'` requires `width`.

###### color?

[`Color`](../type-aliases/Color.md)

Fill color (default opaque white).

###### lineHeight?

`number`

Line advance in pixels; defaults to the font's `lineHeight`.

###### outlineColor?

[`Color`](../type-aliases/Color.md)

Baked-outline color (default black).

###### rotation?

`number`

Rotation in degrees about the anchor (default 0).

###### scale?

`number`

Uniform scale (default 1).

###### shadowColor?

[`Color`](../type-aliases/Color.md)

Baked-shadow color (default black).

###### valign?

`"top"` \| `"middle"` \| `"bottom"`

Vertical alignment relative to `y` (default `'top'`).

###### width?

`number`

Wrap width in pixels; required for `'justify'`.

#### Returns

[`TextBounds`](TextBounds.md)

The laid-out bounds.

***

### endRenderTarget()

> **endRenderTarget**(): `void`

Return to the default target.

#### Returns

`void`

***

### loadFontData()

> **loadFontData**(`path`): [`EfxFontData`](EfxFontData.md)

Parse a `.ttf`/`.otf` font into FontData.

#### Parameters

##### path

`string`

Resource-root-relative path.

#### Returns

[`EfxFontData`](EfxFontData.md)

The parsed FontData.

***

### loadImage()

> **loadImage**(`path`): [`EfxImageData`](EfxImageData.md)

Decode a PNG/JPEG image resource to RGBA8.

#### Parameters

##### path

`string`

Resource-root-relative path.

#### Returns

[`EfxImageData`](EfxImageData.md)

The decoded ImageData.

***

### loadMeshData()

> **loadMeshData**(`path`, `opts?`): [`EfxMeshData`](EfxMeshData.md)

Import a glTF/GLB mesh (materials converted and bound per surface).

#### Parameters

##### path

`string`

Resource-root-relative path.

##### opts?

Optional mesh selector.

###### mesh?

`string` \| `number`

Mesh selector: a non-negative index or a mesh name; defaults to the first mesh.

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The imported CPU MeshData.

***

### makeCapsule()

> **makeCapsule**(`opts?`): [`EfxMeshData`](EfxMeshData.md)

Build single-surface vertical capsule MeshData.

#### Parameters

##### opts?

Optional radius, height, segments, and bound material.

###### height?

`number`

Total tip-to-tip length including caps; must be >= 2 * radius (default 2).

###### material?

[`Material`](Material.md) \| `null`

Material bound to the primitive's single surface; `null` = engine default.

###### radius?

`number`

Capsule radius (default 1, must be > 0).

###### segments?

`number`

Longitude/latitude subdivisions (positive integer, default 16).

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The new CPU MeshData.

***

### makeCube()

> **makeCube**(`opts?`): [`EfxMeshData`](EfxMeshData.md)

Build single-surface cube MeshData.

#### Parameters

##### opts?

Optional size and bound material.

###### inverted?

`boolean`

Point normals inward and reverse winding so the inside renders (default `false`).

###### material?

[`Material`](Material.md) \| `null`

Material bound to the primitive's single surface; `null` = engine default.

###### size?

`number`

Edge length (default 1, must be > 0).

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The new CPU MeshData.

***

### makePlane()

> **makePlane**(`opts?`): [`EfxMeshData`](EfxMeshData.md)

Build single-surface plane MeshData (on XZ, facing +Y).

#### Parameters

##### opts?

Optional size, segments, and bound material.

###### material?

[`Material`](Material.md) \| `null`

Material bound to the primitive's single surface; `null` = engine default.

###### segments?

`number`

Grid subdivisions per side (positive integer, default 1).

###### size?

`number`

Edge length (default 1, must be > 0).

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The new CPU MeshData.

***

### makeSphere()

> **makeSphere**(`opts?`): [`EfxMeshData`](EfxMeshData.md)

Build single-surface UV sphere MeshData.

#### Parameters

##### opts?

Optional radius, segments, and bound material.

###### inverted?

`boolean`

Point normals inward and reverse winding so the inside renders (default `false`); use for a sky dome.

###### material?

[`Material`](Material.md) \| `null`

Material bound to the primitive's single surface; `null` = engine default.

###### radius?

`number`

Sphere radius (default 1, must be > 0).

###### segments?

`number`

Longitude/latitude subdivisions (positive integer, default 16).

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The new CPU MeshData.

***

### setBlendMode()

> **setBlendMode**(`mode`): `void`

Set the frame-local blend render state.

Applies to draws that do not carry their own `blend` (2D quads and
sprite batches, mesh surfaces without a material blend, and billboards).
The engine resets it to `'alpha'` at the start of every frame, so set it
inside the render hook. Particle systems without a configured `blend`
inherit it at draw time.

#### Parameters

##### mode

[`EfxBlendMode`](../type-aliases/EfxBlendMode.md)

`'alpha'` (frame default), `'additive'`, or `'subtractive'`.

#### Returns

`void`

***

### setCamera2D()

> **setCamera2D**(`opts`): `void`

Set the 2D virtual pixel frame and its projection state.

#### Parameters

##### opts

Frame, center, zoom, and rotation.

###### frame?

[`Vec2`](../type-aliases/Vec2.md)

Virtual resolution `[width, height]`; defaults to the current window size.

###### rotation?

`number`

Rotation in degrees around the frame center (default 0).

###### x?

`number`

World x shown at the frame center (default: frame center).

###### y?

`number`

World y shown at the frame center (default: frame center).

###### zoom?

`number`

Zoom factor around the frame center (default 1, must be > 0).

#### Returns

`void`

***

### setCamera3D()

> **setCamera3D**(`pos`, `target`, `fov`, `opts?`): `void`

Set the single 3D camera (separate from the 2D frame).

#### Parameters

##### pos

[`Vec3`](../type-aliases/Vec3.md)

Camera position in world units.

##### target

[`Vec3`](../type-aliases/Vec3.md)

Point the camera looks at, in world units.

##### fov

`number`

Vertical field of view in degrees.

##### opts?

Optional near and far clip planes.

###### far?

`number`

Far plane distance (default 100).

###### near?

`number`

Near plane distance (default 0.1).

#### Returns

`void`

***

### setClearColor()

> **setClearColor**(`color`): `void`

Set the frame clear color.

#### Parameters

##### color

[`Color`](../type-aliases/Color.md)

Clear color `[r, g, b, a]` (default black).

#### Returns

`void`

***

### setDirectionalLight()

> **setDirectionalLight**(`opts`): `void`

Set the single directional light.

#### Parameters

##### opts

\{ `color`: [`Color`](../type-aliases/Color.md); `dir`: [`Vec3`](../type-aliases/Vec3.md); \} \| `null`

Light options, or `null` to disable it.

###### Type Literal

\{ `color`: [`Color`](../type-aliases/Color.md); `dir`: [`Vec3`](../type-aliases/Vec3.md); \}

Light options, or `null` to disable it.

###### color

[`Color`](../type-aliases/Color.md)

Light color `[r, g, b, a]` (alpha ignored).

###### dir

[`Vec3`](../type-aliases/Vec3.md)

Direction the light travels (the direction to the light is `-dir`).

***

`null`

#### Returns

`void`

***

### setLight()

> **setLight**(`slot`, `opts`): `void`

Set a point-light slot.

#### Parameters

##### slot

`number`

Slot index `0..3`.

##### opts

\{ `color`: [`Color`](../type-aliases/Color.md); `pos`: [`Vec3`](../type-aliases/Vec3.md); `range?`: `number`; \} \| `null`

Light options, or `null` to disable the slot.

###### Type Literal

\{ `color`: [`Color`](../type-aliases/Color.md); `pos`: [`Vec3`](../type-aliases/Vec3.md); `range?`: `number`; \}

Light options, or `null` to disable the slot.

###### color

[`Color`](../type-aliases/Color.md)

Light color `[r, g, b, a]` (alpha ignored).

###### pos

[`Vec3`](../type-aliases/Vec3.md)

Light position in world units.

###### range?

`number`

Attenuation radius (finite, >= 0; default 0 = no falloff).

***

`null`

#### Returns

`void`

***

### setPostEffects()

> **setPostEffects**(`list`): `void`

Set the declarative post-effect chain.

#### Parameters

##### list

[`EfxPostEffect`](../type-aliases/EfxPostEffect.md)[] \| `null`

Up to 8 effect entries, or `null`/`[]` to clear the chain.

#### Returns

`void`

***

### setRenderScale()

> **setRenderScale**(`scale`, `opts?`): `void`

Set the scene-resolution scale and final blit filter.

#### Parameters

##### scale

`number`

Scene resolution ratio in `(0, 2]` (default 1).

##### opts?

Optional blit filter.

###### filter?

`"nearest"` \| `"linear"`

Final blit filter (default `'linear'`).

#### Returns

`void`

## Properties

### whiteTexture

> `readonly` **whiteTexture**: [`EfxTexture`](EfxTexture.md)

Engine-owned 1×1 opaque-white texture usable in any draw (read-only;
`destroy()` on it throws).
