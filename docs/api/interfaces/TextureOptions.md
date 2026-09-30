[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / TextureOptions

# Interface: TextureOptions

Sampler options for `createTexture`.

## Example

```js
// tiled, minified ground texture: repeat wrap plus a mip chain
const tex = efx.createTexture(efx.loadImage('paving_color.jpg'),
                              { mipmaps: true });
```

## Properties

### filter?

> `optional` **filter?**: `"nearest"` \| `"linear"`

Texture filter (default `'linear'`); also drives the mipmap filter.

***

### mipmaps?

> `optional` **mipmaps?**: `boolean`

Build and use a full mip chain (default `false`).

***

### wrap?

> `optional` **wrap?**: `"repeat"` \| `"clamp"` \| `"mirror"`

Texture wrap mode (default `'repeat'`).
