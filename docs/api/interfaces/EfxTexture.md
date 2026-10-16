[**EFX API**](../README.md)

***

[EFX API](../README.md) / EfxTexture

# Interface: EfxTexture

A GPU texture (opaque native-backed class).

## Methods

### destroy()

> **destroy**(): `void`

Release the native storage deterministically and idempotently.

#### Returns

`void`

***

### update()

> **update**(`imageData`): `void`

Replace the full texture contents with same-size pixel data. Only
textures created with `{ dynamic: true }` support updates; every member
throws `TypeError` on a destroyed texture or a size mismatch.

#### Parameters

##### imageData

[`EfxImageData`](EfxImageData.md)

#### Returns

`void`

#### Example

```js
const tex = efx.graphics.createTexture(img, { dynamic: true });
function update() {
    tex.update(efx.graphics.createImageData(tex.width, tex.height, nextFramePixels()));
}
```

## Properties

### height

> `readonly` **height**: `number`

Texture height in pixels. Throws `TypeError` when destroyed.

***

### width

> `readonly` **width**: `number`

Texture width in pixels. Throws `TypeError` when destroyed.
