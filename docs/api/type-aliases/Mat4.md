[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / Mat4

# Type Alias: Mat4

> **Mat4** = \[`number`, `number`, `number`, `number`, `number`, `number`, `number`, `number`, `number`, `number`, `number`, `number`, `number`, `number`, `number`, `number`\]

A column-major 4×4 matrix as a flat 16-number array.

## Example

```js
const model = efx.mat4.translate(
  efx.mat4.rotate(efx.mat4.identity(), 45, [0, 1, 0]),
  [0, 0.5, 0],
);
efx.drawMesh(mesh, { transform: model });
```
