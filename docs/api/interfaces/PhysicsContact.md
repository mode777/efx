[**EFX API**](../README.md)

***

[EFX API](../README.md) / PhysicsContact

# Interface: PhysicsContact

One contact reported on a dynamic body's `contacts` list.

## Properties

### body

> `readonly` **body**: [`EfxBody`](EfxBody.md) \| `null`

The other collider's handle (`null` for a static mesh or character).

***

### depth

> `readonly` **depth**: `number`

Penetration depth.

***

### impulse

> `readonly` **impulse**: `number`

Applied impulse magnitude.

***

### normal

> `readonly` **normal**: [`Vec3`](../type-aliases/Vec3.md)

Contact normal `[x, y, z]` in world units.

***

### point

> `readonly` **point**: [`Vec3`](../type-aliases/Vec3.md)

Contact point `[x, y, z]` in world units.

***

### sensor

> `readonly` **sensor**: `boolean`

True when the contact is with a sensor.
