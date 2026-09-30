[**EmotionFX API**](../README.md)

***

[EmotionFX API](../README.md) / PhysicsMoveResult

# Interface: PhysicsMoveResult

The result of `Character.moveAndSlide`.

## Properties

### collisions

> `readonly` **collisions**: [`PhysicsMoveCollision`](PhysicsMoveCollision.md)[]

Collisions encountered during the move.

***

### floorNormal

> `readonly` **floorNormal**: [`Vec3`](../type-aliases/Vec3.md)

Floor normal `[x, y, z]` when on a floor.

***

### onCeiling

> `readonly` **onCeiling**: `boolean`

True when the move was blocked by a ceiling.

***

### onFloor

> `readonly` **onFloor**: `boolean`

True when the move ended on a walkable floor.

***

### onWall

> `readonly` **onWall**: `boolean`

True when the move was blocked by a wall.

***

### position

> `readonly` **position**: [`Vec3`](../type-aliases/Vec3.md)

Resulting world position.
