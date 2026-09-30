[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / EfxBody

# Interface: EfxBody

A native-backed collider in the single physics world.

## Properties

### contacts

> `readonly` **contacts**: [`PhysicsContact`](PhysicsContact.md)[]

Read-only contacts from the last `step`; valid until the next `step`.

***

### position

> `readonly` **position**: [`Vec3`](../type-aliases/Vec3.md)

Read-only world position (mutate `velocity` to move a dynamic body).

***

### transform

> `readonly` **transform**: [`Mat4`](../type-aliases/Mat4.md)

Read-only column-major translation matrix, usable directly by `drawMesh`.

***

### velocity

> **velocity**: [`Vec3`](../type-aliases/Vec3.md)

Read-write linear velocity.

## Methods

### applyForce()

> **applyForce**(`v`): `void`

Apply a force for the next `step`.

#### Parameters

##### v

[`Vec3`](../type-aliases/Vec3.md)

Force vector in world units.

#### Returns

`void`

***

### applyImpulse()

> **applyImpulse**(`v`): `void`

Apply an instantaneous impulse.

#### Parameters

##### v

[`Vec3`](../type-aliases/Vec3.md)

Impulse vector in world units.

#### Returns

`void`

***

### destroy()

> **destroy**(): `void`

Release the native storage deterministically and idempotently.

#### Returns

`void`
