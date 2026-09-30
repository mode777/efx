[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / DrawQuadOptions

# Interface: DrawQuadOptions

Options for `drawQuad`.

## Example

```js
// a 48x48 tinted sprite (see the "Bouncing Sprites" sample)
efx.drawQuad(d.x, d.y, tex, { size: [48, 48], color: d.c });
// a cropped atlas region with an explicit pivot
efx.drawQuad(160, 16, tex, {
  size: [128, 128],
  sourceRect: { x: 128, y: 128, w: 256, h: 256 },
});
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

Uniform scale factor applied after the size is determined (default 1, must be > 0).

***

### size?

> `optional` **size?**: [`Vec2`](../type-aliases/Vec2.md)

Quad size `[width, height]` in frame pixels; defaults to the source rect or texture size.

***

### sourceRect?

> `optional` **sourceRect?**: [`SourceRect`](SourceRect.md)

Texture-pixel region to sample; defaults to the full texture.
