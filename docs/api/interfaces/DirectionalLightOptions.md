[**EFX API**](../README.md)

***

[EFX API](../README.md) / DirectionalLightOptions

# Interface: DirectionalLightOptions

Options for `setDirectionalLight`.

## Example

```js
efx.graphics.setDirectionalLight({ dir: [-0.5, -1, -0.3], color: [0.2, 0.25, 0.35, 1] });
```

## Properties

### color

> **color**: [`Color`](../type-aliases/Color.md)

Light color `[r, g, b, a]` (alpha ignored).

***

### dir

> **dir**: [`Vec3`](../type-aliases/Vec3.md)

Direction the light travels (the direction to the light is `-dir`).
