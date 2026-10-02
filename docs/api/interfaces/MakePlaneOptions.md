[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / MakePlaneOptions

# Interface: MakePlaneOptions

Options for `makePlane`.

## Example

```js
const ground = efx.graphics.createMesh(efx.graphics.makePlane({ size: 10, segments: 4 }));
```

## Properties

### material?

> `optional` **material?**: [`Material`](Material.md) \| `null`

Material bound to the primitive's single surface; `null` = engine default.

***

### segments?

> `optional` **segments?**: `number`

Grid subdivisions per side (positive integer, default 1).

***

### size?

> `optional` **size?**: `number`

Edge length (default 1, must be > 0).
