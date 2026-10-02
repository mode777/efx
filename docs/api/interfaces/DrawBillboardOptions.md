[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / DrawBillboardOptions

# Interface: DrawBillboardOptions

Options for `drawBillboard`.

## Example

```js
efx.graphics.drawBillboard([0, 0.4, 0], {
  texture: spark,
  size: 0.9,
  color: [1, 0.7, 0.3, 0.9],
});
```

## Properties

### color?

> `optional` **color?**: [`Color`](../type-aliases/Color.md)

Tint `[r, g, b, a]` (default opaque white).

***

### depthTest?

> `optional` **depthTest?**: `boolean`

Depth-test against opaque geometry (default `true`); never writes depth.

***

### facing?

> `optional` **facing?**: [`EfxFacing`](../type-aliases/EfxFacing.md)

`'view'` (default, full camera-facing), `'y'` (world-up), or `'plane'` (fixed oriented plane).

***

### normal?

> `optional` **normal?**: [`Vec3`](../type-aliases/Vec3.md)

Plane orientation normal for `facing: 'plane'` (default `[0, 1, 0]`).

***

### rotation?

> `optional` **rotation?**: `number`

In-plane rotation in degrees (default 0).

***

### size?

> `optional` **size?**: `number` \| [`Vec2`](../type-aliases/Vec2.md)

World-unit size: a single number or `[w, h]` (default 1).

***

### sourceRect?

> `optional` **sourceRect?**: [`SourceRect`](SourceRect.md)

Texture-pixel atlas region; defaults to the full texture.

***

### texture

> **texture**: [`EfxSample`](../type-aliases/EfxSample.md)

Texture (or render target) to draw; required.
