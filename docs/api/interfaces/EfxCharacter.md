[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / EfxCharacter

# Interface: EfxCharacter

A native-backed kinematic capsule character controller. The world holds it
until `destroy()` or `physics.clear()`: dropping the last script reference
does not remove it from the simulation.

## Properties

### onFloor

> `readonly` **onFloor**: `boolean`

Read-only: true when currently standing on a floor.

***

### position

> `readonly` **position**: [`Vec3`](../type-aliases/Vec3.md)

Read-only world position.

***

### velocity

> **velocity**: [`Vec3`](../type-aliases/Vec3.md)

Read-write velocity (drives one-way pushes during `step`).

## Methods

### destroy()

> **destroy**(): `void`

Release the native storage deterministically and idempotently.

#### Returns

`void`

***

### moveAndSlide()

> **moveAndSlide**(`motion`): [`PhysicsMoveResult`](PhysicsMoveResult.md)

Sweep and slide the capsule.

#### Parameters

##### motion

[`Vec3`](../type-aliases/Vec3.md)

Desired displacement for this call, in world units.

#### Returns

[`PhysicsMoveResult`](PhysicsMoveResult.md)

The move result: position, floor/wall/ceiling flags, and collisions.
