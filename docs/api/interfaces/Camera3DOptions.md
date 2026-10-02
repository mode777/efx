[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / Camera3DOptions

# Interface: Camera3DOptions

Options for `setCamera3D`.

## Example

```js
efx.graphics.setCamera3D({ pos: [0, 1.6, 4.2], target: [0, 0, 0], fov: 60 });
```

## Properties

### far?

> `optional` **far?**: `number`

Far plane distance (default 100).

***

### fov

> **fov**: `number`

Vertical field of view in degrees.

***

### near?

> `optional` **near?**: `number`

Near plane distance (default 0.1).

***

### pos

> **pos**: [`Vec3`](../type-aliases/Vec3.md)

Camera position in world units.

***

### target

> **target**: [`Vec3`](../type-aliases/Vec3.md)

Point the camera looks at, in world units.
