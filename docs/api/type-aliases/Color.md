[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / Color

# Type Alias: Color

> **Color** = \[`number`, `number`, `number`, `number`\]

An RGBA color: four normalized floats in `0..1`, ordered `[r, g, b, a]`.
Most lighting and material channels ignore the alpha component.

## Example

```js
efx.graphics.setClearColor([0.05, 0.05, 0.1, 1]);
efx.graphics.drawQuad(0, 0, tex, { color: [1, 0.5, 0, 1] });
```
