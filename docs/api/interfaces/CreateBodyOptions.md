[**EFX API**](../README.md)

***

[EFX API](../README.md) / CreateBodyOptions

# Interface: CreateBodyOptions

Options for `physics.createBody`.

## Example

```js
const crate = efx.physics.createBody({
  dynamic: true, mass: 2, friction: 0.6, restitution: 0.1,
  shape: { type: 'box', size: [1, 1, 1] }, position: [0, 3, 0],
});
```

## Properties

### dynamic?

> `optional` **dynamic?**: `boolean`

Simulated by the solver when true (default `false` = static).

***

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

### mass?

> `optional` **mass?**: `number`

Dynamic mass (default 1, must be positive).

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

***

### shape

> **shape**: [`PhysicsShape`](../type-aliases/PhysicsShape.md)

Collider shape; required.
