[**EFX API**](../README.md)

***

[EFX API](../README.md) / CreateImageDataOptions

# Interface: CreateImageDataOptions

Options for `createImageData`.

## Example

```js
// a procedural radial glow (see the "Particle Showcase" sample)
const size = 32;
const px = new Uint8Array(size * size * 4);
for (let y = 0; y < size; y++) {
  for (let x = 0; x < size; x++) {
    const i = (y * size + x) * 4;
    px[i] = px[i + 1] = px[i + 2] = 255;
    px[i + 3] = 255; // ...compute coverage from the distance to center
  }
}
const glow = efx.graphics.createImageData({ width: size, height: size, pixels: px });
```

## Properties

### format?

> `optional` **format?**: `"rgba8"`

Pixel format; only `'rgba8'` is supported (default `'rgba8'`).

***

### height

> **height**: `number`

Image height in pixels (must be > 0).

***

### pixels

> **pixels**: `number`[] \| `Uint8Array`\<`ArrayBufferLike`\>

Flat RGBA8 bytes of length `width * height * 4`.

***

### width

> **width**: `number`

Image width in pixels (must be > 0).
