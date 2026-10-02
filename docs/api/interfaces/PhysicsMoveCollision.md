[**EFX API**](../README.md)

***

[EFX API](../README.md) / PhysicsMoveCollision

# Interface: PhysicsMoveCollision

One collision reported by `Character.moveAndSlide`.

## Properties

### body

> `readonly` **body**: [`EfxBody`](EfxBody.md) \| `null`

The blocking collider's handle (`null` for a static mesh or character).

***

### normal

> `readonly` **normal**: [`Vec3`](../type-aliases/Vec3.md)

Collision normal `[x, y, z]`.

***

### point

> `readonly` **point**: [`Vec3`](../type-aliases/Vec3.md)

Collision point `[x, y, z]`.
