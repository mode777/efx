[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / LoadMeshDataOptions

# Interface: LoadMeshDataOptions

Options for `loadMeshData`.

## Example

```js
const first = efx.graphics.loadMeshData('scene.gltf');
const named = efx.graphics.loadMeshData('scene.gltf', { mesh: 'Teapot' });
const byIndex = efx.graphics.loadMeshData('scene.gltf', { mesh: 2 });
```

## Properties

### mesh?

> `optional` **mesh?**: `string` \| `number`

Mesh selector: a non-negative index or a mesh name; defaults to the first mesh.
