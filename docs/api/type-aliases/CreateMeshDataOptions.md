[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / CreateMeshDataOptions

# Type Alias: CreateMeshDataOptions

> **CreateMeshDataOptions** = [`MeshDataBatch`](../interfaces/MeshDataBatch.md) \| [`MeshDataShorthand`](../interfaces/MeshDataShorthand.md)

`createMeshData` accepts either the batch or the shorthand form.

## Example

```js
// shorthand: one surface
const quad = efx.graphics.createMeshData({
  positions: [-4, 0, -4, 4, 0, -4, 4, 0, 4, -4, 0, 4],
  normals:   [0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0],
  uvs:       [0, 0, 6, 0, 6, 6, 0, 6],
  indices:   [0, 1, 2, 0, 2, 3],
});

// batch: several surfaces with per-surface materials
const mesh = efx.graphics.createMeshData({
  surfaces: [{ positions: [0, 0, 0, 1, 0, 0, 0, 1, 0] }],
  materials: [null],
});
```
