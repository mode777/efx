[**EFX API**](../README.md)

***

[EFX API](../README.md) / MakeCubeOptions

# Interface: MakeCubeOptions

Options for `makeCube`.

## Example

```js
const cube = efx.graphics.createMesh(efx.graphics.makeCube({ size: 1.4 }));
```

## Properties

### material?

> `optional` **material?**: [`Material`](Material.md) \| `null`

Material bound to the primitive's single surface; `null` = engine default.

***

### size?

> `optional` **size?**: `number`

Edge length (default 1, must be > 0).
