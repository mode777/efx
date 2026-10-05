[**EFX API**](../README.md)

***

[EFX API](../README.md) / DrawMeshOptions

# Interface: DrawMeshOptions

Options for `drawMesh`.

## Example

```js
efx.graphics.drawMesh(cube, {
  transform: efx.math.mat4.rotate(efx.math.mat4.identity(), yaw, [0, 1, 0]),
  color: [0.95, 0.5, 0.2, 1],
});
```

## Properties

### color?

> `optional` **color?**: [`Color`](../type-aliases/Color.md)

Tint multiplying vertex colors (default opaque white).

***

### skinned?

> `optional` **skinned?**: `boolean`

`true` draws the current CPU-posed vertices; absent/false the bind pose.

***

### transform?

> `optional` **transform?**: [`Mat4`](../type-aliases/Mat4.md)

Column-major transform (default identity).
