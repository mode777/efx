[**EFX API**](../README.md)

***

[EFX API](../README.md) / Quat

# Type Alias: Quat

> **Quat** = \[`number`, `number`, `number`, `number`\]

A quaternion `[x, y, z, w]`.

## Example

```js
const q = efx.math.quat.fromAxisAngle(90, [0, 1, 0]);
const m = efx.math.quat.toMat4(q);
```
