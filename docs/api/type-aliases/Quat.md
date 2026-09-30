[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / Quat

# Type Alias: Quat

> **Quat** = \[`number`, `number`, `number`, `number`\]

A quaternion `[x, y, z, w]`.

## Example

```js
const q = efx.quat.fromAxisAngle(90, [0, 1, 0]);
const m = efx.quat.toMat4(q);
```
