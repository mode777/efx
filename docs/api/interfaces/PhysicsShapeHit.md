[**EFX API**](../README.md)

***

[EFX API](../README.md) / PhysicsShapeHit

# Interface: PhysicsShapeHit

One shape-cast hit.

## Properties

### body

> `readonly` **body**: [`EfxBody`](EfxBody.md) \| [`EfxCharacter`](EfxCharacter.md) \| `null`

The hit collider's handle (`null` for a static mesh or character).

***

### fraction

> `readonly` **fraction**: `number`

Fraction along `motion` where the hit occurred (`0..1`).

***

### normal

> `readonly` **normal**: [`Vec3`](../type-aliases/Vec3.md)

Surface normal `[x, y, z]` at the hit.

***

### point

> `readonly` **point**: [`Vec3`](../type-aliases/Vec3.md)

Hit point `[x, y, z]`.
