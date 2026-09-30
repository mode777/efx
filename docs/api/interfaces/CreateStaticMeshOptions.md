[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / CreateStaticMeshOptions

# Interface: CreateStaticMeshOptions

Options for `physics.createStaticMesh`.

## Example

```js
const ramp = efx.physics.createStaticMesh(rampMesh, { friction: 0.8 });
```

## Properties

### friction?

> `optional` **friction?**: `number`

Surface friction (default 0.5).

***

### layer?

> `optional` **layer?**: `number`

Collision layer bitmask (32-bit; default all bits).

***

### mask?

> `optional` **mask?**: `number`

Collision mask bitmask (32-bit; default all bits).

***

### position?

> `optional` **position?**: [`Vec3`](../type-aliases/Vec3.md)

Initial position in world units (default `[0, 0, 0]`).

***

### restitution?

> `optional` **restitution?**: `number`

Bounciness in `[0, 1]` (default 0).

***

### sensor?

> `optional` **sensor?**: `boolean`

Report-only volume that never resolves (default `false`).
