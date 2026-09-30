[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / SpriteOptions

# Interface: SpriteOptions

One entry of a `drawSprites` batch; each is equivalent to a `drawQuad` call.

## Example

```js
efx.drawSprites(spark, [
  { x: 20,  y: 20, size: [48, 48], color: [1, 0.4, 0.2, 0.9] },
  { x: 74,  y: 20, size: [48, 48], color: [1, 0.7, 0.3, 0.9] },
]);
```

## Properties

### color?

> `optional` **color?**: [`Color`](../type-aliases/Color.md)

Tint `[r, g, b, a]` (default opaque white).

***

### origin?

> `optional` **origin?**: [`Vec2`](../type-aliases/Vec2.md)

Pivot `[px, py]` in quad-local pixels for rotation/scale (default the size's center).

***

### rotation?

> `optional` **rotation?**: `number`

Rotation in degrees clockwise (default 0).

***

### scale?

> `optional` **scale?**: `number`

Uniform scale factor (default 1, must be > 0).

***

### size?

> `optional` **size?**: [`Vec2`](../type-aliases/Vec2.md)

Quad size `[width, height]` in frame pixels; defaults to the source rect or texture size.

***

### sourceRect?

> `optional` **sourceRect?**: [`SourceRect`](SourceRect.md)

Texture-pixel region to sample; defaults to the full texture.

***

### x

> **x**: `number`

Quad top-left x in frame pixels.

***

### y

> **y**: `number`

Quad top-left y in frame pixels.
