[**EFX API**](../README.md)

***

[EFX API](../README.md) / MakeCapsuleOptions

# Interface: MakeCapsuleOptions

Options for `makeCapsule`.

## Example

```js
// a capsule matching a physics character (radius 0.4, height 1.8)
const body = efx.graphics.createMesh(efx.graphics.makeCapsule({ radius: 0.4, height: 1.8 }));
```

## Properties

### height?

> `optional` **height?**: `number`

Total tip-to-tip length including caps; must be >= 2 * radius (default 2).

***

### material?

> `optional` **material?**: [`Material`](Material.md) \| `null`

Material bound to the primitive's single surface; `null` = engine default.

***

### radius?

> `optional` **radius?**: `number`

Capsule radius (default 1, must be > 0).

***

### segments?

> `optional` **segments?**: `number`

Longitude/latitude subdivisions (positive integer, default 16).
