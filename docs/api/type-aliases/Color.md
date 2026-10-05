[**EFX API**](../README.md)

***

[EFX API](../README.md) / Color

# Type Alias: Color

> **Color** = \[`number`, `number`, `number`, `number`\]

An RGBA color: four normalized floats in `0..1`, ordered `[r, g, b, a]`.
Most lighting and material channels ignore the alpha component.

## Example

```js
efx.graphics.setClearColor([0.05, 0.05, 0.1, 1]);
efx.graphics.drawQuad(tex, 0, 0, { color: [1, 0.5, 0, 1] });
```
