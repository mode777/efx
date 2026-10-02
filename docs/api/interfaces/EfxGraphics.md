[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxGraphics

# Interface: EfxGraphics

The graphics surface: 2D and 3D drawing, camera and light state, post
processing, and the graphics resource factories. Reached as
`efx.graphics`; names, signatures, semantics, and error behavior are
unchanged by the move from the `efx` root.

## Properties

### whiteTexture

> `readonly` **whiteTexture**: [`EfxTexture`](EfxTexture.md)

Engine-owned 1×1 opaque-white texture usable in any draw (read-only;
`destroy()` on it throws).

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

> **createFont**(`fontData`, `opts`): [`EfxFont`](EfxFont.md)

Bake a fixed glyph atlas from FontData.

#### Parameters

##### fontData

[`EfxFontData`](EfxFontData.md)

Parsed source font.

##### opts

[`CreateFontOptions`](CreateFontOptions.md)

Required bake options (at minimum `size`).

#### Returns

[`EfxFont`](EfxFont.md)

The baked Font.

***

### createImageData()

> **createImageData**(`opts`): [`EfxImageData`](EfxImageData.md)

Build CPU pixels as an ImageData.

#### Parameters

##### opts

[`CreateImageDataOptions`](CreateImageDataOptions.md)

Width, height, RGBA8 pixels, and optional format.

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

> **createMeshData**(`data`): [`EfxMeshData`](EfxMeshData.md)

Build multi-surface MeshData from the batch or shorthand form.

#### Parameters

##### data

[`CreateMeshDataOptions`](../type-aliases/CreateMeshDataOptions.md)

Surface attributes and optional per-surface materials.

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The new CPU MeshData.

***

### createParticleSystem()

> **createParticleSystem**(`opts`): [`EfxParticleSystem`](EfxParticleSystem.md)

Create a native-backed CPU particle system.

#### Parameters

##### opts

[`ParticleSystemOptions`](ParticleSystemOptions.md)

System options; `texture`, `max`, and `lifetime` are required.

#### Returns

[`EfxParticleSystem`](EfxParticleSystem.md)

The new particle system.

***

### createRenderTarget()

> **createRenderTarget**(`opts`): [`EfxRenderTarget`](EfxRenderTarget.md)

Create a GPU render target with a color and depth attachment.

#### Parameters

##### opts

[`RenderTargetOptions`](RenderTargetOptions.md)

Target width and height (1..4096 each).

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

[`TextureOptions`](TextureOptions.md)

Optional wrap, filter, and mipmap settings.

#### Returns

[`EfxTexture`](EfxTexture.md)

The new texture.

***

### drawBillboard()

> **drawBillboard**(`pos`, `opts`): `void`

Record one world-space billboard quad.

#### Parameters

##### pos

[`Vec3`](../type-aliases/Vec3.md)

World position `[x, y, z]`.

##### opts

[`DrawBillboardOptions`](DrawBillboardOptions.md)

Required texture plus size, tint, facing, and depth options.

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

[`DrawMeshCallOptions`](DrawMeshCallOptions.md)

Optional transform, tint, and skinned flag.

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

> **drawQuad**(`x`, `y`, `texture`, `opts?`): `void`

Record one textured quad.

#### Parameters

##### x

`number`

Quad top-left x in frame pixels.

##### y

`number`

Quad top-left y in frame pixels.

##### texture

[`EfxSample`](../type-aliases/EfxSample.md)

Live texture or render target to sample.

##### opts?

[`DrawQuadOptions`](DrawQuadOptions.md)

Optional tint, transform, size, origin, and source rect.

#### Returns

`void`

***

### drawSprites()

> **drawSprites**(`texture`, `sprites`): `void`

Record a batch of 2D sprite quads from one texture.

#### Parameters

##### texture

[`EfxSample`](../type-aliases/EfxSample.md)

Live texture or render target to sample.

##### sprites

[`SpriteOptions`](SpriteOptions.md)[]

One options bag per quad; validation is atomic.

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

[`TextOptions`](TextOptions.md)

Optional alignment, wrap, colors, rotation, and scale.

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

[`LoadMeshDataOptions`](LoadMeshDataOptions.md)

Optional mesh selector.

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The imported CPU MeshData.

***

### makeCapsule()

> **makeCapsule**(`opts?`): [`EfxMeshData`](EfxMeshData.md)

Build single-surface vertical capsule MeshData.

#### Parameters

##### opts?

[`MakeCapsuleOptions`](MakeCapsuleOptions.md)

Optional radius, height, segments, and bound material.

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The new CPU MeshData.

***

### makeCube()

> **makeCube**(`opts?`): [`EfxMeshData`](EfxMeshData.md)

Build single-surface cube MeshData.

#### Parameters

##### opts?

[`MakeCubeOptions`](MakeCubeOptions.md)

Optional size and bound material.

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The new CPU MeshData.

***

### makePlane()

> **makePlane**(`opts?`): [`EfxMeshData`](EfxMeshData.md)

Build single-surface plane MeshData (on XZ, facing +Y).

#### Parameters

##### opts?

[`MakePlaneOptions`](MakePlaneOptions.md)

Optional size, segments, and bound material.

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The new CPU MeshData.

***

### makeSphere()

> **makeSphere**(`opts?`): [`EfxMeshData`](EfxMeshData.md)

Build single-surface UV sphere MeshData.

#### Parameters

##### opts?

[`MakeSphereOptions`](MakeSphereOptions.md)

Optional radius, segments, and bound material.

#### Returns

[`EfxMeshData`](EfxMeshData.md)

The new CPU MeshData.

***

### measureText()

> **measureText**(`text`, `font`, `opts?`): [`TextBounds`](TextBounds.md)

Lay out text without drawing it.

#### Parameters

##### text

`string`

Text to measure.

##### font

[`EfxFont`](EfxFont.md)

Baked font to measure with.

##### opts?

[`TextOptions`](TextOptions.md)

Optional alignment, wrap, and scale (matching a later draw).

#### Returns

[`TextBounds`](TextBounds.md)

The laid-out bounds.

***

### poseMesh()

> **poseMesh**(`mesh`, `pose`): `void`

CPU-pose a skinned mesh in place.

#### Parameters

##### mesh

[`EfxMesh`](EfxMesh.md)

Live skinned mesh.

##### pose

[`PoseSample`](PoseSample.md) \| [`PoseSample`](PoseSample.md)[]

One pose sample, or an array of samples to blend.

#### Returns

`void`

***

### setBlendMode()

> **setBlendMode**(`mode`): `void`

Set the blend mode for subsequently recorded 2D draws.

#### Parameters

##### mode

[`EfxBlendMode`](../type-aliases/EfxBlendMode.md)

`'alpha'` (default), `'additive'`, or `'subtractive'`.

#### Returns

`void`

***

### setCamera2D()

> **setCamera2D**(`opts`): `void`

Set the 2D virtual pixel frame and its projection state.

#### Parameters

##### opts

[`Camera2DOptions`](Camera2DOptions.md)

Frame, center, zoom, and rotation.

#### Returns

`void`

***

### setCamera3D()

> **setCamera3D**(`opts`): `void`

Set the single 3D camera (separate from the 2D frame).

#### Parameters

##### opts

[`Camera3DOptions`](Camera3DOptions.md)

Camera position, target, field of view, and clip planes.

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

[`DirectionalLightOptions`](DirectionalLightOptions.md) \| `null`

Light options, or `null` to disable it.

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

[`PointLightOptions`](PointLightOptions.md) \| `null`

Light options, or `null` to disable the slot.

#### Returns

`void`

***

### setMeshSurfaceMaterial()

> **setMeshSurfaceMaterial**(`mesh`, `surfaceIndex`, `mat`): `void`

Bind a Phong material to one mesh surface.

#### Parameters

##### mesh

[`EfxMesh`](EfxMesh.md)

Owning live mesh.

##### surfaceIndex

`number`

Surface to bind (`0`-based).

##### mat

[`Material`](Material.md) \| `null`

Material object, or `null` to restore the engine default.

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

[`RenderScaleOptions`](RenderScaleOptions.md)

Optional blit filter.

#### Returns

`void`
