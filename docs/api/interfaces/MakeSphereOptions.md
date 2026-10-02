[**EFX API**](../README.md)

***

[EFX API](../README.md) / MakeSphereOptions

# Interface: MakeSphereOptions

Options for `makeSphere`.

## Example

```js
const ball = efx.graphics.createMesh(efx.graphics.makeSphere({ radius: 1.6, segments: 32 }));
```

## Properties

### material?

> `optional` **material?**: [`Material`](Material.md) \| `null`

Material bound to the primitive's single surface; `null` = engine default.

***

### radius?

> `optional` **radius?**: `number`

Sphere radius (default 1, must be > 0).

***

### segments?

> `optional` **segments?**: `number`

Longitude/latitude subdivisions (positive integer, default 16).
