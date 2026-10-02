[**EFX API**](../README.md)

***

[EFX API](../README.md) / PhysicsRayHit

# Interface: PhysicsRayHit

One raycast hit.

## Properties

### body

> `readonly` **body**: [`EfxBody`](EfxBody.md) \| [`EfxCharacter`](EfxCharacter.md) \| `null`

The hit collider's handle (`null` for a static mesh or character).

***

### distance

> `readonly` **distance**: `number`

Distance from the ray origin.

***

### normal

> `readonly` **normal**: [`Vec3`](../type-aliases/Vec3.md)

Surface normal `[x, y, z]` at the hit.

***

### point

> `readonly` **point**: [`Vec3`](../type-aliases/Vec3.md)

Hit point `[x, y, z]`.
