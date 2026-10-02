[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / PointLightOptions

# Interface: PointLightOptions

Options for `setLight` (a point light).

## Example

```js
efx.graphics.setLight(0, { pos: [3, 4, 2], color: [1, 0.95, 0.9, 1], range: 20 });
```

## Properties

### color

> **color**: [`Color`](../type-aliases/Color.md)

Light color `[r, g, b, a]` (alpha ignored).

***

### pos

> **pos**: [`Vec3`](../type-aliases/Vec3.md)

Light position in world units.

***

### range?

> `optional` **range?**: `number`

Attenuation radius (finite, >= 0; default 0 = no falloff).
